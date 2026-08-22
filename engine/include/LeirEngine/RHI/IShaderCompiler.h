#pragma once

#include "LeirEngine/RHI/RHI.h"

#include <string>
#include <vector>

namespace Leir {
namespace RHI {

// Compilation targets for the shader tooling. Mirrors Slang's
// SlangCompileTarget but stays backend-neutral. Only SpirV and DXIL are
// currently consumed by the engine; the rest are produced by the exporter for
// the future backends (Metal, WebGPU/WGSL, GLSL fallback).
enum class ShaderTarget : uint8_t {
    SpirV = 0,
    DXIL = 1,
    Metal = 2,   // Metal shading language (MSL) text
    WGSL = 3,    // WebGPU shading language text
    GLSL450 = 4, // desktop GLSL 4.50 text
    GLSLES = 5,  // GLSL ES (future, via SPIRV-Cross — not direct)
};

// A single shader resource binding discovered by reflection.
struct ShaderBinding {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    DescriptorType type = DescriptorType::CombinedImageSampler;
    uint32_t count = 1;
    ShaderStage stage = ShaderStage::Fragment;
};

// A push-constant range discovered by reflection.
struct ShaderPushConstantRange {
    ShaderStageMask stage = ShaderStageMask::Vertex;
    uint32_t offset = 0;
    uint32_t size = 0;
};

// Full reflection for one compiled shader stage.
struct ShaderReflection {
    std::vector<ShaderBinding> bindings;
    std::vector<ShaderPushConstantRange> pushConstants;
};

struct CompileResult {
    bool ok = false;
    std::string error;
    std::vector<char> bytecode;  // target bytecode (SPIR-V, DXIL, or text code)
    ShaderReflection reflection; // filled when `reflect` was true
};

// Compiles .slang shaders to multiple targets. The interface is public (engine
// include tree) but the implementation lives in the editor (SlangShaderCompiler)
// — no slang types ever leak into public headers, and the engine DLL does not
// depend on libslang (isolation §6, TODO_RHI_SLANG.md).
class IShaderCompiler {
public:
    virtual ~IShaderCompiler() = default;

    virtual bool IsAvailable() const = 0;
    virtual std::string GetVersion() const = 0;

    // Compile a .slang file. The stage is passed explicitly (derived from the
    // filename by the caller); Slang also reads the [shader("...")] attribute.
    // `macroDefines` is a ';'-separated list of preprocessor macros in the form
    // NAME=VALUE (e.g. "LEIR_BINDLESS=0") passed to the Slang session. Empty =
    // no macros.
    virtual CompileResult Compile(
        const std::string& sourcePath,
        ShaderTarget target,
        ShaderStage stage,
        bool reflect = true,
        const std::string& macroDefines = {}) = 0;

    // Compile in-memory Slang source (moduleName is used for diagnostics /
    // imports; path is the logical file path for error messages).
    virtual CompileResult CompileFromSource(
        const std::string& moduleName,
        const std::string& source,
        ShaderTarget target,
        ShaderStage stage,
        bool reflect = true,
        const std::string& macroDefines = {}) = 0;
};

} // namespace RHI
} // namespace Leir
