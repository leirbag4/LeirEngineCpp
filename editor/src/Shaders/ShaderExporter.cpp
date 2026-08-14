#include "ShaderExporter.h"

#include <LeirEngine/Core/Log.h>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>

namespace {

const std::string kShaderDir = LEIR_SHADER_SOURCE_DIR; // engine/shaders/*.slang
const std::string kExportRoot = LEIR_SHADER_EXPORT_DIR;
const std::string kRuntimeShaderDir = LEIR_SHADER_DIR; // next to the runtime bytecode

const char* TypeToString(Leir::RHI::DescriptorType t)
{
    switch (t) {
        case Leir::RHI::DescriptorType::CombinedImageSampler: return "CombinedImageSampler";
        case Leir::RHI::DescriptorType::UniformBuffer: return "UniformBuffer";
    }
    return "CombinedImageSampler";
}

const char* StageToString(Leir::RHI::ShaderStage s)
{
    switch (s) {
        case Leir::RHI::ShaderStage::Vertex: return "Vertex";
        case Leir::RHI::ShaderStage::Fragment: return "Fragment";
    }
    return "Vertex";
}

const char* StageMaskToString(Leir::RHI::ShaderStageMask m)
{
    const uint8_t v = static_cast<uint8_t>(m);
    if (v == (uint8_t)Leir::RHI::ShaderStageMask::VertexFragment)
        return "VertexFragment";
    if (v == (uint8_t)Leir::RHI::ShaderStageMask::Vertex)
        return "Vertex";
    if (v == (uint8_t)Leir::RHI::ShaderStageMask::Fragment)
        return "Fragment";
    return "VertexFragment";
}

// Canonical reflection sidecar (parsed by Leir::LoadShaderReflectionFromSidecars):
//   { "stage": "Vertex", "bindings": [ {name,set,binding,type,count,stage} ],
//     "pushConstants": [ {stage,offset,size} ] }
std::string ReflectionToJson(const Leir::RHI::ShaderReflection& reflection,
    Leir::RHI::ShaderStage stage)
{
    nlohmann::json j;
    j["stage"] = StageToString(stage);
    j["bindings"] = nlohmann::json::array();
    for (const auto& b : reflection.bindings) {
        j["bindings"].push_back({
            { "name", b.name },
            { "set", b.set },
            { "binding", b.binding },
            { "type", TypeToString(b.type) },
            { "count", b.count },
            { "stage", StageToString(b.stage) },
        });
    }
    j["pushConstants"] = nlohmann::json::array();
    for (const auto& pc : reflection.pushConstants) {
        j["pushConstants"].push_back({
            { "stage", StageMaskToString(pc.stage) },
            { "offset", pc.offset },
            { "size", pc.size },
        });
    }
    return j.dump(2);
}

} // namespace

std::vector<Leir::RHI::ShaderTarget> AllTargets()
{
    std::vector<Leir::RHI::ShaderTarget> targets = {
        Leir::RHI::ShaderTarget::SpirV,
        Leir::RHI::ShaderTarget::Metal,
        Leir::RHI::ShaderTarget::WGSL,
        Leir::RHI::ShaderTarget::GLSL450,
    };
    // DXIL needs the external 'dxc' downstream compiler (dxcompiler.dll), which
    // ships with the Vulkan SDK on Windows and is never present on macOS/Linux.
    // D3D12 is Windows-only, so the DXIL target is a Windows-only export.
#ifdef _WIN32
    targets.insert(targets.begin() + 1, Leir::RHI::ShaderTarget::DXIL);
#endif
    return targets;
}

const std::vector<ShaderExporter::ShaderFile>& ShaderExporter::ShaderFiles()
{
    static const std::vector<ShaderFile> files = {
        { "Basic.vert", Leir::RHI::ShaderStage::Vertex },
        { "Basic.frag", Leir::RHI::ShaderStage::Fragment },
        { "Sprite.vert", Leir::RHI::ShaderStage::Vertex },
        { "Sprite.frag", Leir::RHI::ShaderStage::Fragment },
        { "UI.vert", Leir::RHI::ShaderStage::Vertex },
        { "UI.frag", Leir::RHI::ShaderStage::Fragment },
    };
    return files;
}

const char* ShaderExporter::TargetLabel(Leir::RHI::ShaderTarget target)
{
    switch (target) {
        case Leir::RHI::ShaderTarget::SpirV:  return "SPIR-V";
        case Leir::RHI::ShaderTarget::DXIL:   return "DXIL";
        case Leir::RHI::ShaderTarget::Metal:  return "Metal";
        case Leir::RHI::ShaderTarget::WGSL:   return "WGSL";
        case Leir::RHI::ShaderTarget::GLSL450: return "GLSL 450";
        case Leir::RHI::ShaderTarget::GLSLES: return "GLSL ES (future)";
    }
    return "?";
}

const char* ShaderExporter::TargetExtension(Leir::RHI::ShaderTarget target)
{
    switch (target) {
        case Leir::RHI::ShaderTarget::SpirV:  return ".spv";
        case Leir::RHI::ShaderTarget::DXIL:   return ".dxil";
        case Leir::RHI::ShaderTarget::Metal:  return ".metal";
        case Leir::RHI::ShaderTarget::WGSL:   return ".wgsl";
        case Leir::RHI::ShaderTarget::GLSL450: return ".glsl";
        case Leir::RHI::ShaderTarget::GLSLES: return ".gles";
    }
    return ".bin";
}

const char* ShaderExporter::TargetDirectory(Leir::RHI::ShaderTarget target)
{
    switch (target) {
        case Leir::RHI::ShaderTarget::SpirV:  return "spirv";
        case Leir::RHI::ShaderTarget::DXIL:   return "dxil";
        case Leir::RHI::ShaderTarget::Metal:  return "metal";
        case Leir::RHI::ShaderTarget::WGSL:   return "wgsl";
        case Leir::RHI::ShaderTarget::GLSL450: return "glsl450";
        case Leir::RHI::ShaderTarget::GLSLES: return "gles";
    }
    return "misc";
}

std::vector<std::string> ShaderExporter::ExportAll(Leir::RHI::IShaderCompiler* compiler)
{
    std::vector<std::string> lines;
    if (!compiler || !compiler->IsAvailable()) {
        lines.push_back("[Export] shader compiler unavailable");
        return lines;
    }

    for (Leir::RHI::ShaderTarget target : AllTargets()) {
        const std::string outDir = std::string(kExportRoot) + "/" + TargetDirectory(target);
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);

        int ok = 0, failed = 0;
        for (const auto& file : ShaderFiles()) {
            const std::string src = std::string(kShaderDir) + "/" + file.name + ".slang";
            const std::string dst = outDir + "/" + file.name + TargetExtension(target);

            auto result = compiler->Compile(src, target, file.stage, /*reflect=*/true);
            if (!result.ok) {
                ++failed;
                lines.push_back(std::string("[Export] ") + TargetLabel(target) + " " + file.name +
                    " FAILED: " + result.error);
                continue;
            }

            std::FILE* fp = std::fopen(dst.c_str(), "wb");
            if (!fp) {
                ++failed;
                lines.push_back(std::string("[Export] ") + TargetLabel(target) + " " + file.name +
                    " failed to write " + dst);
                continue;
            }
            std::fwrite(result.bytecode.data(), 1, result.bytecode.size(), fp);
            std::fclose(fp);
            ++ok;

            // Report reflection once per shader (only on the first target) so
            // the console shows what the tooling discovered.
            if (target == Leir::RHI::ShaderTarget::SpirV) {
                Leir::XConsole::Debug("[Export] {} reflection: {} bindings, {} push ranges",
                    file.name,
                    result.reflection.bindings.size(),
                    result.reflection.pushConstants.size());

                // Emit the canonical reflection sidecar alongside the exported
                // bytecode (single source of truth for pipeline layouts).
                WriteReflectionSidecar(file.name, result.reflection, file.stage, outDir);
            }
        }

        if (failed == 0) {
            lines.push_back(std::string("[Export] ") + TargetLabel(target) + ": " + std::to_string(ok) +
                "/" + std::to_string(ShaderFiles().size()) + " shaders -> " + outDir);
        } else {
            lines.push_back(std::string("[Export] ") + TargetLabel(target) + ": " + std::to_string(ok) +
                " ok, " + std::to_string(failed) + " failed -> " + outDir);
        }
    }

    return lines;
}

bool ShaderExporter::WriteReflectionSidecar(const std::string& name,
    const Leir::RHI::ShaderReflection& reflection, Leir::RHI::ShaderStage stage,
    const std::string& outDir)
{
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    const std::string path = outDir + "/" + name + ".reflect.json";
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        Leir::XConsole::PrintWarning("[Sidecar] cannot write {}", path);
        return false;
    }
    const std::string json = ReflectionToJson(reflection, stage);
    std::fwrite(json.data(), 1, json.size(), fp);
    std::fclose(fp);
    return true;
}

std::vector<std::string> ShaderExporter::WriteRuntimeSidecars(
    Leir::RHI::IShaderCompiler* compiler)
{
    std::vector<std::string> lines;
    if (!compiler || !compiler->IsAvailable()) {
        lines.push_back("[Sidecar] shader compiler unavailable");
        return lines;
    }

    int ok = 0, failed = 0;
    for (const auto& file : ShaderFiles()) {
        const std::string src = std::string(kShaderDir) + "/" + file.name + ".slang";
        auto result = compiler->Compile(src, Leir::RHI::ShaderTarget::SpirV, file.stage,
            /*reflect=*/true);
        if (!result.ok) {
            ++failed;
            lines.push_back(std::string("[Sidecar] ") + file.name +
                " FAILED: " + result.error);
            continue;
        }
        WriteReflectionSidecar(file.name, result.reflection, file.stage, kRuntimeShaderDir);
        ++ok;
    }

    lines.push_back(std::string("[Sidecar] ") + std::to_string(ok) + "/" +
        std::to_string(ShaderFiles().size()) + " reflections -> " + kRuntimeShaderDir +
        (failed ? " (" + std::to_string(failed) + " failed)" : ""));
    return lines;
}
