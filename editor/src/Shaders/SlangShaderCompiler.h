#pragma once

#include <LeirEngine/RHI/IShaderCompiler.h>

namespace slang {
struct IGlobalSession;
struct ISession;
struct IModule;
}

namespace Leir {
namespace RHI {

// IShaderCompiler implementation backed by libslang (the Slang compiler
// shipped with the Vulkan SDK). Editor-only dev tool: the engine never links
// Slang. Uses the modern C++ API: IGlobalSession -> ISession -> IModule ->
// findEntryPointByName -> createCompositeComponentType -> getEntryPointCode /
// getLayout.
class SlangShaderCompiler : public IShaderCompiler {
public:
    SlangShaderCompiler();
    ~SlangShaderCompiler() override;

    bool IsAvailable() const override;
    std::string GetVersion() const override;

    CompileResult Compile(
        const std::string& sourcePath,
        ShaderTarget target,
        ShaderStage stage,
        bool reflect = true) override;

    CompileResult CompileFromSource(
        const std::string& moduleName,
        const std::string& source,
        ShaderTarget target,
        ShaderStage stage,
        bool reflect = true) override;

private:
    slang::ISession* CreateSession(ShaderTarget target);
    CompileResult CompileModule(
        slang::ISession* session,
        slang::IModule* module,
        ShaderTarget target,
        ShaderStage stage,
        bool reflect);

    slang::IGlobalSession* m_GlobalSession = nullptr;
};

} // namespace RHI
} // namespace Leir