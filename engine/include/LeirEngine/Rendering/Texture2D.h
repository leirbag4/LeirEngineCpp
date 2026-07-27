#pragma once

#include "LeirEngine/Core/Export.h"

#include <vulkan/vulkan.h>
#include <string>

namespace Leir {

class VulkanDevice;

class LEIR_API Texture2D {
public:
    Texture2D(VulkanDevice* device, const std::string& path);
    Texture2D(VulkanDevice* device, uint32_t width, uint32_t height,
              const unsigned char* pixels);
    ~Texture2D();

    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler GetSampler() const { return m_Sampler; }
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    VkDescriptorImageInfo GetDescriptorInfo() const {
        VkDescriptorImageInfo info{};
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = m_ImageView;
        info.sampler = m_Sampler;
        return info;
    }

private:
    void TransitionLayout(VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height);

    VulkanDevice* m_Device;
    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
};

} // namespace Leir
