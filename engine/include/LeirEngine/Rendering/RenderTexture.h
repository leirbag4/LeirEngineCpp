#pragma once
#include "LeirEngine/Core/Export.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Leir {

class VulkanDevice;

class LEIR_API RenderTexture {
public:
    RenderTexture(VulkanDevice* device, uint32_t width, uint32_t height);
    ~RenderTexture();

    void BeginRender(VkCommandBuffer cmd, VkClearValue clearColor, float depthClear = 1.0f);
    void EndRender(VkCommandBuffer cmd);

    VkDescriptorImageInfo GetDescriptorInfo() const;
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkImageView GetImageView() const { return m_ColorImageView; }
    VkSampler GetSampler() const { return m_Sampler; }

private:
    VulkanDevice* m_Device;
    uint32_t m_Width, m_Height;

    VkImage m_ColorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ColorMemory = VK_NULL_HANDLE;
    VkImageView m_ColorImageView = VK_NULL_HANDLE;

    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;

    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
};

} // namespace Leir
