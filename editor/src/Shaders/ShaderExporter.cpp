#include "ShaderExporter.h"

#include <LeirEngine/Core/Log.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
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

// ---- WGSL post-processing (single-source Grid shaders -> WebGPU backend) ----

// The WebGPU backend hardcodes the entry points vs_main/ps_main; Slang's WGSL
// output names the entry after the .slang function (main). Rename it.
void RenameWgslEntry(std::string& wgsl, Leir::RHI::ShaderStage stage)
{
    const std::string from = "fn main(";
    const std::string to =
        stage == Leir::RHI::ShaderStage::Vertex ? "fn vs_main(" : "fn ps_main(";
    const size_t p = wgsl.find(from);
    if (p != std::string::npos)
        wgsl.replace(p, from.size(), to);
}

// Slang emits the push constants as a bare global `var<uniform> X :` with no
// group/binding (the variable is named after the .slang push member, e.g.
// `push_0` or `screenSize_0`). The WebGPU backend emulates push constants as a
// uniform buffer at group index = the pipeline's descriptor-set count, binding
// 0. Annotate every bare `var<uniform>` global (those not already carrying an
// @group annotation, like the UBOs) with the given group.
void AnnotateWgslPush(std::string& wgsl, uint32_t group)
{
    const std::string marker = "var<uniform> ";
    size_t p = 0;
    while ((p = wgsl.find(marker, p)) != std::string::npos) {
        const size_t lineStart = wgsl.rfind('\n', p);
        const size_t lineBegin = lineStart == std::string::npos ? 0 : lineStart + 1;
        const size_t groupAt = wgsl.find("@group(", lineBegin);
        if (groupAt == std::string::npos || groupAt > p) {
            const std::string annot =
                "@group(" + std::to_string(group) + ") @binding(0) ";
            wgsl.insert(p, annot);
            p += annot.size() + marker.size();
        } else {
            p += marker.size();
        }
    }
}

// Slang's WGSL writer assigns VERTEX-INPUT locations from the HLSL semantics
// (POSITION0/TEXCOORDN/COLOR0), not the explicit vk::location, so they come out
// reordered (the grid exports start=0, end=5, color=6, cornerX=1, ...). The
// engine's vertex input state provides them in declaration order, so renumber
// the fields of the `vertexInput_*` struct to 0..N in declaration order.
void RenumberVertexInputLocations(std::string& wgsl)
{
    const size_t marker = wgsl.find("struct vertexInput_");
    if (marker == std::string::npos)
        return;
    const size_t brace = wgsl.find('{', marker);
    if (brace == std::string::npos)
        return;
    const size_t closeBrace = wgsl.find("};", brace);
    if (closeBrace == std::string::npos)
        return;

    const std::string locPrefix = "@location(";
    int loc = 0;
    size_t pos = brace;
    while (pos < closeBrace) {
        const size_t lp = wgsl.find(locPrefix, pos);
        if (lp == std::string::npos || lp >= closeBrace)
            break;
        const size_t close = wgsl.find(')', lp);
        if (close == std::string::npos || close >= closeBrace)
            break;
        const std::string repl = locPrefix + std::to_string(loc) + ")";
        wgsl.replace(lp, close - lp + 1, repl);
        ++loc;
        pos = lp + repl.size();
    }
}

// Slang's WGSL target emits `vector * matrix` (row-vector) for the logical
// `matrix * vector` because it stores matrices transposed in WGSL. With our GLM
// column-major UBO the reconstructed WGSL matrix equals the logical one, so
// `vector * matrix` computes the TRANSPOSE (broken projection — the grid
// rendered as solid rectangles pointing everywhere). Swap the two operands so
// it computes `matrix * vector`.
void FixWgslMatrixMultiply(std::string& wgsl)
{
    const std::string op = " * (mat4x4<f32>";
    size_t p = 0;
    while ((p = wgsl.find(op, p)) != std::string::npos) {
        // Matrix expression opens at the '(' right after " * ".
        const size_t matOpen = p + 3;
        size_t depth = 0;
        size_t i = matOpen;
        for (; i < wgsl.size(); ++i) {
            if (wgsl[i] == '(') ++depth;
            else if (wgsl[i] == ')') { if (--depth == 0) break; }
        }
        const size_t matEnd = i; // inclusive ')'
        if (matEnd >= wgsl.size())
            break;

        // The vector expression's closing ')' is just before the " * ".
        const size_t vecClose = p - 1;
        depth = 0;
        size_t j = vecClose;
        for (; j > 0; --j) {
            if (wgsl[j] == ')') ++depth;
            else if (wgsl[j] == '(') { if (--depth == 0) break; }
        }
        if (depth != 0)
            break;
        const size_t vecOpen = j; // inclusive '('

        const std::string vecExpr = wgsl.substr(vecOpen, vecClose - vecOpen + 1);
        const std::string matExpr = wgsl.substr(matOpen, matEnd - matOpen + 1);
        wgsl.replace(vecOpen, matEnd - vecOpen + 1, matExpr + " * " + vecExpr);
        p = vecOpen + matExpr.size() + 3 + vecExpr.size();
    }
}

// Slang's WGSL emits the bindless table as a plain runtime array of textures/
// samplers (`array<texture_2d<f32>>`), which is INVALID WGSL — naga rejects an
// array of resources; only `binding_array` is legal and it needs a size. The
// WebGPU backend's bindless layout declares bindingArraySize = kBindlessMax
// (16), so rewrite them to sized binding_arrays.
void FixWgslBindless(std::string& wgsl)
{
    const char* kTexture = "array<texture_2d<f32>>";
    const char* kTexRepl = "binding_array<texture_2d<f32>, 16>";
    const char* kSampler = "array<sampler>";
    const char* kSampRepl = "binding_array<sampler, 16>";
    for (;;) {
        const size_t p = wgsl.find(kTexture);
        if (p == std::string::npos)
            break;
        wgsl.replace(p, std::strlen(kTexture), kTexRepl);
    }
    for (;;) {
        const size_t p = wgsl.find(kSampler);
        if (p == std::string::npos)
            break;
        wgsl.replace(p, std::strlen(kSampler), kSampRepl);
    }
}

struct WgslShaderPair { const char* vert; const char* frag; };

// Compile the given .slang pairs to WGSL (with the macro defines), post-process
// for the WebGPU backend and write <name><outExt> into outDir. Returns the
// number of files written (appends failure lines to `lines`).
int GenerateWgslPairs(Leir::RHI::IShaderCompiler* compiler,
    const WgslShaderPair* pairs, int pairCount,
    const std::string& macroDefines, const std::string& outDir,
    const std::string& outExt, std::vector<std::string>& lines)
{
    int ok = 0, failed = 0;
    for (int p = 0; p < pairCount; ++p) {
        const WgslShaderPair& pair = pairs[p];
        const std::string vertPath = std::string(kShaderDir) + "/" + pair.vert + ".slang";
        const std::string fragPath = std::string(kShaderDir) + "/" + pair.frag + ".slang";

        // Compile both stages to WGSL with reflection (the reflection gives the
        // descriptor-set layout used to compute the backend's push group).
        auto vertRes = compiler->Compile(vertPath, Leir::RHI::ShaderTarget::WGSL,
            Leir::RHI::ShaderStage::Vertex, /*reflect=*/true, macroDefines);
        auto fragRes = compiler->Compile(fragPath, Leir::RHI::ShaderTarget::WGSL,
            Leir::RHI::ShaderStage::Fragment, /*reflect=*/true, macroDefines);
        if (!vertRes.ok || !fragRes.ok) {
            ++failed;
            lines.push_back("[WebGPU] " + std::string(pair.vert) + "/" + pair.frag +
                " FAILED: " + (!vertRes.ok ? vertRes.error : fragRes.error));
            continue;
        }

        // Push group = number of distinct descriptor sets across both stages
        // (the backend binds the push UBO at group index = setLayouts.size()).
        uint32_t pushGroup = 0;
        for (const auto& b : vertRes.reflection.bindings)
            pushGroup = std::max(pushGroup, b.set + 1);
        for (const auto& b : fragRes.reflection.bindings)
            pushGroup = std::max(pushGroup, b.set + 1);

        // Post-process + write each stage.
        struct StageOut { const char* name; Leir::RHI::ShaderStage stage; std::string text; };
        StageOut stages[2] = {
            { pair.vert, Leir::RHI::ShaderStage::Vertex,
              std::string(vertRes.bytecode.data(), vertRes.bytecode.size()) },
            { pair.frag, Leir::RHI::ShaderStage::Fragment,
              std::string(fragRes.bytecode.data(), fragRes.bytecode.size()) },
        };
        for (auto& s : stages) {
            RenameWgslEntry(s.text, s.stage);
            AnnotateWgslPush(s.text, pushGroup);
            FixWgslBindless(s.text);
            if (s.stage == Leir::RHI::ShaderStage::Vertex) {
                RenumberVertexInputLocations(s.text);
                FixWgslMatrixMultiply(s.text);
            }
            const std::string dst = outDir + "/" + s.name + outExt;
            std::FILE* fp = std::fopen(dst.c_str(), "wb");
            if (!fp) {
                ++failed;
                lines.push_back("[WebGPU] cannot write " + dst);
                continue;
            }
            std::fwrite(s.text.data(), 1, s.text.size(), fp);
            std::fclose(fp);
            ++ok;
        }
    }
    return ok;
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
        { "Grid.vert", Leir::RHI::ShaderStage::Vertex },
        { "Grid.frag", Leir::RHI::ShaderStage::Fragment },
        { "Gizmo.vert", Leir::RHI::ShaderStage::Vertex },
        { "Gizmo.frag", Leir::RHI::ShaderStage::Fragment },
        { "GizmoSolid.vert", Leir::RHI::ShaderStage::Vertex },
        { "GizmoSolid.frag", Leir::RHI::ShaderStage::Fragment },
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

std::vector<std::string> ShaderExporter::WriteRuntimeWebGpuShaders(
    Leir::RHI::IShaderCompiler* compiler)
{
    std::vector<std::string> lines;
    if (!compiler || !compiler->IsAvailable()) {
        lines.push_back("[WebGPU] shader compiler unavailable");
        return lines;
    }

    // All engine shader pairs, single-source from .slang. Basic/Sprite/UI use
    // the bindless flag (LEIR_BINDLESS=1 native); Grid/Gizmo have no bindless.
    static const WgslShaderPair kPairs[] = {
        { "Grid.vert", "Grid.frag" },
        { "Gizmo.vert", "Gizmo.frag" },
        { "GizmoSolid.vert", "GizmoSolid.frag" },
        { "Basic.vert", "Basic.frag" },
        { "Sprite.vert", "Sprite.frag" },
        { "UI.vert", "UI.frag" },
    };
    const std::string defines = "LEIR_BINDLESS=1"; // native (bindless) variant

    int ok = GenerateWgslPairs(compiler, kPairs, (int)(sizeof(kPairs) / sizeof(kPairs[0])),
        defines, kRuntimeShaderDir, ".wgsl", lines);
    lines.push_back(std::string("[WebGPU] WGSL ") + std::to_string(ok) + "/" +
        std::to_string((int)(sizeof(kPairs) / sizeof(kPairs[0])) * 2) + " -> " +
        kRuntimeShaderDir);
    return lines;
}

std::vector<std::string> ShaderExporter::WriteWebShaders(
    Leir::RHI::IShaderCompiler* compiler, const std::string& outDir)
{
    std::vector<std::string> lines;
    if (!compiler || !compiler->IsAvailable()) {
        lines.push_back("[WebGPU] shader compiler unavailable");
        return lines;
    }

    // Web-demo shaders (the browser WebGPU cannot compile binding_array, so the
    // single-texture LEIR_BINDLESS=0 variant).
    static const WgslShaderPair kWebPairs[] = {
        { "Basic.vert", "Basic.frag" },
        { "Sprite.vert", "Sprite.frag" },
        { "UI.vert", "UI.frag" },
    };
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    const int ok = GenerateWgslPairs(compiler, kWebPairs,
        (int)(sizeof(kWebPairs) / sizeof(kWebPairs[0])),
        "LEIR_BINDLESS=0", outDir, ".web.wgsl", lines);
    lines.push_back(std::string("[WebGPU] web WGSL ") + std::to_string(ok) + "/" +
        std::to_string((int)(sizeof(kWebPairs) / sizeof(kWebPairs[0])) * 2) + " -> " +
        outDir);
    return lines;
}
