#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include "LeirEngine/Core/Log.h"
#include <fstream>
#include <stdexcept>

namespace Leir {

Shader::Shader(RHI::RenderBackend* device,
               const std::string& vertexPath, const std::string& fragmentPath)
    : m_Device(device)
    , m_VertexPath(vertexPath)
    , m_FragmentPath(fragmentPath)
{
    m_Stages.push_back(RHI::ShaderStage::Vertex);
    m_Stages.push_back(RHI::ShaderStage::Fragment);
    Load();
    XConsole::Println("Shader loaded: {} + {}", vertexPath, fragmentPath);
}

void Shader::Load()
{
    auto vertCode = ReadFile(m_VertexPath);
    auto fragCode = ReadFile(m_FragmentPath);

    std::vector<std::vector<char>> codes = { std::move(vertCode), std::move(fragCode) };

    for (size_t i = 0; i < m_Stages.size(); ++i) {
        RHI::RHIShaderModule module = m_Device->CreateShaderModule(codes[i]);
        m_Modules.push_back(module);

        RHI::RHIShaderStageInfo info{};
        info.stage = m_Stages[i];
        info.module = module;
        info.entryPoint = "main";
        m_StageInfos.push_back(info);
    }
}

void Shader::Reload()
{
    // Destroy the previous modules, re-read the bytecode and recreate them.
    // The stage list / entry points are unchanged.
    for (auto& module : m_Modules)
        m_Device->DestroyShaderModule(module);
    m_Modules.clear();
    m_StageInfos.clear();

    Load();
    XConsole::Println("Shader reloaded: {} + {}", m_VertexPath, m_FragmentPath);
}

Shader::~Shader()
{
    for (auto& module : m_Modules)
        m_Device->DestroyShaderModule(module);
}

std::vector<char> Shader::ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + path);

    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    file.close();
    return buffer;
}

} // namespace Leir
