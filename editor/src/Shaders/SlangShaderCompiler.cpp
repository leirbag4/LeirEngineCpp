#include "SlangShaderCompiler.h"

#include <LeirEngine/Core/Log.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <fstream>
#include <sstream>
#include <cstdio>

namespace Leir {
namespace RHI {

namespace {

// Slang's component API only produces correct capability validation for all
// targets when the module is loaded from the file system (IModule loaded via
// ISession::loadModule). loadModuleFromSource (in-memory modules) mis-reports
// cbuffers as requiring Std140DataLayout, which is unavailable on DXIL/Metal/
// WGSL targets (error[E36107]). So CompileFromSource stages its source to a
// temp .slang file and goes through the same file-based path.
std::string TempSlangFilePath(const std::string& moduleName)
{
    const char* base = getenv("TEMP");
    std::string dir = base ? base : ".";
    static int counter = 0;
    return dir + "/leir_slang_" + moduleName + "_" + std::to_string(++counter) + ".slang";
}

std::string DirOf(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

SlangCompileTarget ToSlangTarget(ShaderTarget target)
{
    switch (target) {
        case ShaderTarget::SpirV:   return SLANG_SPIRV;
        case ShaderTarget::DXIL:    return SLANG_DXIL;
        case ShaderTarget::Metal:   return SLANG_METAL;
        case ShaderTarget::WGSL:    return SLANG_WGSL;
        case ShaderTarget::GLSL450: return SLANG_GLSL;
        case ShaderTarget::GLSLES:  return SLANG_GLSL; // future: SPIRV-Cross
    }
    return SLANG_SPIRV;
}

const char* TargetProfileName(ShaderTarget target)
{
    switch (target) {
        case ShaderTarget::SpirV: return "spirv_1_3";
        case ShaderTarget::DXIL:  return "sm_6_0";
        default:                  return nullptr;
    }
}

std::string DiagToString(slang::IBlob* diag)
{
    if (!diag)
        return {};
    const char* text = static_cast<const char*>(diag->getBufferPointer());
    return text ? std::string(text) : std::string();
}

std::vector<char> BlobToVector(slang::IBlob* blob)
{
    std::vector<char> out;
    if (!blob)
        return out;
    const char* data = static_cast<const char*>(blob->getBufferPointer());
    const size_t size = blob->getBufferSize();
    out.assign(data, data + size);
    return out;
}

ShaderStageMask StageMask(ShaderStage stage)
{
    return stage == ShaderStage::Vertex ? ShaderStageMask::Vertex
                                        : ShaderStageMask::Fragment;
}

// Map a Slang parameter category to our DescriptorType. Best effort: the RHI
// currently only knows CombinedImageSampler + UniformBuffer.
DescriptorType ToDescriptorType(slang::ParameterCategory category)
{
    switch (category) {
        case slang::ParameterCategory::ConstantBuffer:
            return DescriptorType::UniformBuffer;
        case slang::ParameterCategory::ShaderResource:
        case slang::ParameterCategory::UnorderedAccess:
        case slang::ParameterCategory::SamplerState:
        default:
            return DescriptorType::CombinedImageSampler;
    }
}

} // namespace

SlangShaderCompiler::SlangShaderCompiler()
{
    SlangGlobalSessionDesc desc = {};
    desc.apiVersion = SLANG_API_VERSION;
    desc.enableGLSL = true;
    if (SLANG_SUCCEEDED(slang_createGlobalSession2(&desc, &m_GlobalSession))) {
        XConsole::Debug("[Slang] global session created ({})",
            m_GlobalSession ? m_GlobalSession->getBuildTagString() : "unknown");
    } else {
        m_GlobalSession = nullptr;
        XConsole::PrintError("[Slang] failed to create global session");
    }
}

SlangShaderCompiler::~SlangShaderCompiler()
{
    if (m_GlobalSession)
        m_GlobalSession->release();
}

bool SlangShaderCompiler::IsAvailable() const { return m_GlobalSession != nullptr; }

std::string SlangShaderCompiler::GetVersion() const
{
    if (!m_GlobalSession)
        return "unavailable";
    const char* tag = m_GlobalSession->getBuildTagString();
    return tag ? std::string(tag) : std::string("unknown");
}

CompileResult SlangShaderCompiler::Compile(
    const std::string& sourcePath,
    ShaderTarget target,
    ShaderStage stage,
    bool reflect)
{
    if (!m_GlobalSession)
        return { false, "slang global session unavailable", {}, {} };

    slang::ISession* session = CreateSession(target);
    if (!session)
        return { false, "failed to create slang session", {}, {} };
    Slang::ComPtr<slang::ISession> sessionPtr;
    sessionPtr.attach(session);

    slang::IBlob* loadDiag = nullptr;
    slang::IModule* module = session->loadModule(sourcePath.c_str(), &loadDiag);
    if (!module) {
        std::string err = "slang module load failed: " + DiagToString(loadDiag);
        return { false, err, {}, {} };
    }

    return CompileModule(session, module, target, stage, reflect);
}

CompileResult SlangShaderCompiler::CompileFromSource(
    const std::string& moduleName,
    const std::string& source,
    ShaderTarget target,
    ShaderStage stage,
    bool reflect)
{
    if (!m_GlobalSession)
        return { false, "slang global session unavailable", {}, {} };

    // Stage to a temp file: Slang's file-based loading path is required (see
    // TempSlangFilePath comment).
    const std::string tempPath = TempSlangFilePath(moduleName);
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return { false, "failed to write temp slang source: " + tempPath, {}, {} };
        out.write(source.data(), static_cast<std::streamsize>(source.size()));
    }

    slang::ISession* session = CreateSession(target);
    if (!session) {
        std::remove(tempPath.c_str());
        return { false, "failed to create slang session", {}, {} };
    }
    Slang::ComPtr<slang::ISession> sessionPtr;
    sessionPtr.attach(session);

    slang::IBlob* loadDiag = nullptr;
    slang::IModule* module = session->loadModule(tempPath.c_str(), &loadDiag);
    const bool loaded = module != nullptr;
    std::string loadError = DiagToString(loadDiag);
    std::remove(tempPath.c_str());
    if (!loaded) {
        std::string err = "slang module load failed: " + loadError;
        return { false, err, {}, {} };
    }

    return CompileModule(session, module, target, stage, reflect);
}

slang::ISession* SlangShaderCompiler::CreateSession(ShaderTarget target)
{
    slang::TargetDesc targetDesc;
    targetDesc.format = ToSlangTarget(target);
    if (const char* profile = TargetProfileName(target))
        targetDesc.profile = m_GlobalSession->findProfile(profile);

    slang::SessionDesc sessionDesc;
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    slang::ISession* session = nullptr;
    if (SLANG_FAILED(m_GlobalSession->createSession(sessionDesc, &session)))
        return nullptr;
    return session;
}

CompileResult SlangShaderCompiler::CompileModule(
    slang::ISession* session,
    slang::IModule* module,
    ShaderTarget target,
    ShaderStage stage,
    bool reflect)
{
    slang::IEntryPoint* entryPoint = nullptr;
    if (SLANG_FAILED(module->findEntryPointByName("main", &entryPoint))) {
        return { false, "slang entry point 'main' not found", {}, {} };
    }
    // entryPoint is borrowed: the composite below keeps it alive.

    slang::IComponentType* components[] = { module, entryPoint };
    slang::IComponentType* composite = nullptr;
    slang::IBlob* compDiag = nullptr;
    if (SLANG_FAILED(session->createCompositeComponentType(
            components, 2, &composite, &compDiag))) {
        std::string err = "slang composite failed: " + DiagToString(compDiag);
        return { false, err, {}, {} };
    }
    Slang::ComPtr<slang::IComponentType> compositePtr;
    compositePtr.attach(composite);

    slang::IBlob* code = nullptr;
    slang::IBlob* codeDiag = nullptr;
    if (SLANG_FAILED(composite->getEntryPointCode(0, 0, &code, &codeDiag))) {
        std::string err = "slang codegen failed: " + DiagToString(codeDiag);
        return { false, err, {}, {} };
    }
    Slang::ComPtr<slang::IBlob> codePtr;
    codePtr.attach(code);

    CompileResult result;
    result.ok = true;
    result.bytecode = BlobToVector(code);

    if (reflect) {
        slang::IBlob* layoutDiag = nullptr;
        slang::ProgramLayout* layout = composite->getLayout(0, &layoutDiag);
        if (layout) {
            ShaderReflection reflection;
            const unsigned count = layout->getParameterCount();
            for (unsigned i = 0; i < count; ++i) {
                slang::VariableLayoutReflection* param = layout->getParameterByIndex(i);
                if (!param)
                    continue;

                const slang::ParameterCategory category = param->getCategory();
                if (category == slang::ParameterCategory::PushConstantBuffer) {
                    ShaderPushConstantRange range;
                    range.stage = StageMask(stage);
                    range.offset = (uint32_t)param->getOffset(
                        slang::ParameterCategory::PushConstantBuffer);
                    range.size = (uint32_t)param->getTypeLayout()->getSize(
                        slang::ParameterCategory::PushConstantBuffer);
                    reflection.pushConstants.push_back(range);
                    continue;
                }

                ShaderBinding binding;
                if (param->getName())
                    binding.name = param->getName();
                binding.set = param->getBindingSpace();
                binding.binding = param->getBindingIndex();
                binding.type = ToDescriptorType(category);
                binding.count = 1;
                binding.stage = stage;
                reflection.bindings.push_back(binding);
            }
            result.reflection = std::move(reflection);
        }
    }

    return result;
}

} // namespace RHI
} // namespace Leir