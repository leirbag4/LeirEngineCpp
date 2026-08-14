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
