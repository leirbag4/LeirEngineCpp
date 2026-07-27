#pragma once

#include "LeirEngine/Core/Export.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace Leir {

class VulkanDevice;
class Texture2D;
class Shader;

class LEIR_API Material {
public:
    Material(VulkanDevice* device, std::shared_ptr<Shader> shader);
    ~Material();

    void SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture);
    void SetColor(const glm::vec4& color) { m_Color = color; }
    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const glm::vec3& value);

    void Bind(VkCommandBuffer cmd, VkPipelineLayout layout) const;

    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
    std::shared_ptr<Shader> GetShader() const { return m_Shader; }
    VkDescriptorSetLayout GetUBOSetLayout() const;

    // Recreate pipeline when device is lost
    void RecreatePipeline(VkRenderPass renderPass);

private:
    void CreateDescriptorPool();
    void CreateDescriptorSetLayout();
    void CreateDescriptorSet();
    void UpdateDescriptorSet();
    void CreatePipeline(VkRenderPass renderPass);

    VulkanDevice* m_Device;
    std::shared_ptr<Shader> m_Shader;
    glm::vec4 m_Color{1.0f};

    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_UBOSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;

    std::shared_ptr<Texture2D> m_Texture;
};

} // namespace Leir
