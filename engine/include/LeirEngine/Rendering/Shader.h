#pragma once

#include "LeirEngine/Core/Export.h"

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace Leir {

class VulkanDevice;

struct LEIR_API ShaderStage {
    VkShaderStageFlagBits stage;
    std::vector<char> spirv;
};

class LEIR_API Shader {
public:
    Shader(VulkanDevice* device, const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    VkPipelineShaderStageCreateInfo GetStageInfo(size_t index) const;
    size_t GetStageCount() const { return m_Stages.size(); }
    const std::vector<VkPipelineShaderStageCreateInfo>& GetStageInfos() const { return m_StageInfos; }

    static std::vector<char> ReadFile(const std::string& path);

private:
    VulkanDevice* m_Device;
    std::vector<ShaderStage> m_Stages;
    std::vector<VkShaderModule> m_Modules;
    std::vector<VkPipelineShaderStageCreateInfo> m_StageInfos;
};

} // namespace Leir
