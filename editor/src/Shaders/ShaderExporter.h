#pragma once

#include <LeirEngine/RHI/IShaderCompiler.h>

#include <string>
#include <vector>

// Exports every engine .slang shader to all supported targets (SPIR-V, DXIL,
// MSL, WGSL, GLSL 450). Editor-only dev tool — the compiled files land under
// LEIR_SHADER_EXPORT_DIR so they can be inspected / used by future backends.
class ShaderExporter {
public:
    // Compile all engine shaders to every target. Returns one console line per
    // target summarizing the result (or per-shader on failure).
    static std::vector<std::string> ExportAll(Leir::RHI::IShaderCompiler* compiler);

    // Compile every engine shader to SPIR-V (reflect=true) and write the
    // canonical reflection sidecar next to the runtime bytecode
    // (LEIR_SHADER_DIR/<name>.reflect.json) so the engine derives its pipeline
    // layouts from the shader signature (Plan B, Fase 2). Best-effort.
    static std::vector<std::string> WriteRuntimeSidecars(Leir::RHI::IShaderCompiler* compiler);

    // Compile the editor-grid + gizmo + Basic/Sprite/UI shaders to WGSL and
    // write them to LEIR_SHADER_DIR, post-processed for the WebGPU backend
    // (entries vs_main/ps_main, push @group(N)@binding(0) with N derived from
    // the reflection, vertex input locations reordered, binding_array size,
    // matrix multiply fix). Single-source: the WebGPU backend loads these
    // generated files instead of hand-maintained .wgsl mirrors. Best-effort.
    static std::vector<std::string> WriteRuntimeWebGpuShaders(Leir::RHI::IShaderCompiler* compiler);

    // Compile the web-demo shaders (Basic/Sprite/UI) to WGSL with
    // LEIR_BINDLESS=0 (single-texture, the browser cannot compile
    // binding_array) and write <name>.web.wgsl into outDir, post-processed the
    // same way. Single-source for the web export. Best-effort.
    static std::vector<std::string> WriteWebShaders(Leir::RHI::IShaderCompiler* compiler,
        const std::string& outDir);

    // Serialize one stage's reflection to <outDir>/<name>.reflect.json in the
    // canonical format parsed by Leir::LoadShaderReflectionFromSidecars.
    static bool WriteReflectionSidecar(const std::string& name,
        const Leir::RHI::ShaderReflection& reflection, Leir::RHI::ShaderStage stage,
        const std::string& outDir);

    // Human-readable label for a target, e.g. "SPIR-V" / "DXIL" / "Metal" ...
    static const char* TargetLabel(Leir::RHI::ShaderTarget target);

private:
    struct ShaderFile {
        const char* name;                         // "Basic.vert" (no extension)
        Leir::RHI::ShaderStage stage;
    };

    static const std::vector<ShaderFile>& ShaderFiles();
    static const char* TargetExtension(Leir::RHI::ShaderTarget target);
    static const char* TargetDirectory(Leir::RHI::ShaderTarget target);
};
