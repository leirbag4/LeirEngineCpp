#include "ShaderExporter.h"

#include <LeirEngine/Core/Log.h>

#include <cstdio>
#include <filesystem>

namespace {

const std::string kShaderDir = LEIR_SHADER_SOURCE_DIR; // engine/shaders/*.slang
const std::string kExportRoot = LEIR_SHADER_EXPORT_DIR;

} // namespace

std::vector<Leir::RHI::ShaderTarget> AllTargets()
{
    return {
        Leir::RHI::ShaderTarget::SpirV,
        Leir::RHI::ShaderTarget::DXIL,
        Leir::RHI::ShaderTarget::Metal,
        Leir::RHI::ShaderTarget::WGSL,
        Leir::RHI::ShaderTarget::GLSL450,
    };
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
