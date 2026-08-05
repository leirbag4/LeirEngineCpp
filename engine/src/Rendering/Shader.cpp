#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/VulkanDevice.h"

#include "LeirEngine/Core/Log.h"
#include <fstream>
#include <stdexcept>

namespace Leir {

Shader::Shader(VulkanDevice* device, const std::string& vertexPath, const std::string& fragmentPath)
    : m_Device(device)
{
    ShaderStage vertStage;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.spirv = ReadFile(vertexPath);
    m_Stages.push_back(vertStage);

    ShaderStage fragStage;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.spirv = ReadFile(fragmentPath);
    m_Stages.push_back(fragStage);

    for (const auto& stage : m_Stages) {
        VkShaderModule module = m_Device->CreateShaderModule(stage.spirv);
        m_Modules.push_back(module);

        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = stage.stage;
        info.module = module;
        info.pName = "main";
        m_StageInfos.push_back(info);
    }

    XConsole::Println("Shader loaded: {} + {}", vertexPath, fragmentPath);
}

Shader::~Shader()
{
    for (auto module : m_Modules)
        vkDestroyShaderModule(m_Device->GetDevice(), module, nullptr);
}

VkPipelineShaderStageCreateInfo Shader::GetStageInfo(size_t index) const
{
    if (index < m_StageInfos.size())
        return m_StageInfos[index];
    return {};
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
