#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/VulkanDevice.h"

#include <stb_image.h>

#include <spdlog/spdlog.h>

namespace Leir {

void Texture2D::CreateFromData(const unsigned char* pixels, uint32_t width, uint32_t height)
{
    m_Width = width;
    m_Height = height;
    VkDeviceSize imageSize = (VkDeviceSize)width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingMemory);

    void* data;
    vkMapMemory(m_Device->GetDevice(), stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, (size_t)imageSize);
    vkUnmapMemory(m_Device->GetDevice(), stagingMemory);

    m_Device->CreateImage(width, height, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Image, m_Memory);

    TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, m_Image, width, height);
    TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_Device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), stagingMemory, nullptr);

    m_ImageView = m_Device->CreateImageView(m_Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    m_Sampler = m_Device->CreateSampler();
}

Texture2D::Texture2D(VulkanDevice* device, const std::string& path)
    : m_Device(device)
{
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    static stbi_uc fallback[] = { 255, 0, 255, 255 };
    bool useFallback = false;

    if (!pixels) {
        spdlog::error("Failed to load texture: {}", path);
        pixels = fallback;
        texWidth = texHeight = 1;
        texChannels = 4;
        useFallback = true;
    }

    CreateFromData(pixels, (uint32_t)texWidth, (uint32_t)texHeight);

    if (!useFallback)
        stbi_image_free(pixels);

    spdlog::info("Texture loaded: {} ({}x{})", path, m_Width, m_Height);
}

Texture2D::Texture2D(VulkanDevice* device, uint32_t width, uint32_t height,
                     const unsigned char* pixels)
    : m_Device(device)
{
    CreateFromData(pixels, width, height);
}

Texture2D::Texture2D(VulkanDevice* device, Image& image)
    : m_Device(device)
{
    CreateFromData(image.GetData(), image.GetWidth(), image.GetHeight());
}

Texture2D::~Texture2D()
{
    vkDestroySampler(m_Device->GetDevice(), m_Sampler, nullptr);
    vkDestroyImageView(m_Device->GetDevice(), m_ImageView, nullptr);
    vkDestroyImage(m_Device->GetDevice(), m_Image, nullptr);
    vkFreeMemory(m_Device->GetDevice(), m_Memory, nullptr);
}

void Texture2D::UpdateFromImage(Image& image)
{
    VkDeviceSize imageSize = (VkDeviceSize)image.GetWidth() * image.GetHeight() * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingMemory);

    void* data;
    vkMapMemory(m_Device->GetDevice(), stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, image.GetData(), (size_t)imageSize);
    vkUnmapMemory(m_Device->GetDevice(), stagingMemory);

    TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer, m_Image, image.GetWidth(), image.GetHeight());
    TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(m_Device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), stagingMemory, nullptr);

    m_Width = image.GetWidth();
    m_Height = image.GetHeight();
}

void Texture2D::TransitionLayout(VkImage image, VkFormat format,
    VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Device->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
               newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Device->GetGraphicsQueue());

    vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetCommandPool(), 1, &cmd);
}

void Texture2D::CopyBufferToImage(VkBuffer buffer, VkImage image,
    uint32_t width, uint32_t height)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Device->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_Device->GetDevice(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Device->GetGraphicsQueue());

    vkFreeCommandBuffers(m_Device->GetDevice(), m_Device->GetCommandPool(), 1, &cmd);
}

} // namespace Leir
