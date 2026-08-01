#include "LeirEngine/Rendering/RenderTexture.h"
#include "LeirEngine/Rendering/VulkanDevice.h"
#include <stdexcept>
#include <vector>

namespace Leir {

RenderTexture::RenderTexture(VulkanDevice* device, uint32_t width, uint32_t height)
    : m_Device(device), m_Width(width), m_Height(height)
{
    CreateRenderPass();
    CreateSampler();
    CreateResources();
    CreateDescriptorResources();
}

RenderTexture::~RenderTexture()
{
    auto dev = m_Device->GetDevice();
    vkDeviceWaitIdle(dev);
    DestroyDescriptorResources();
    DestroyResources();
    if (m_Sampler) vkDestroySampler(dev, m_Sampler, nullptr);
    if (m_RenderPass) vkDestroyRenderPass(dev, m_RenderPass, nullptr);
}

void RenderTexture::Resize(uint32_t width, uint32_t height)
{
    if (width == m_Width && height == m_Height)
        return;

    auto dev = m_Device->GetDevice();
    vkDeviceWaitIdle(dev);
    DestroyResources();
    m_Width = width;
    m_Height = height;
    CreateResources();
    UpdateDescriptor();
}

void RenderTexture::CreateRenderPass()
{
    auto dev = m_Device->GetDevice();
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkAttachmentDescription color{};
    color.format = colorFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format = depthFormat;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { color, depth };
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    if (vkCreateRenderPass(dev, &rpInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen render pass");
}

void RenderTexture::CreateSampler()
{
    auto dev = m_Device->GetDevice();
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(dev, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create sampler");
}

void RenderTexture::CreateResources()
{
    auto dev = m_Device->GetDevice();
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    // Color image (color attachment + sampled)
    m_Device->CreateImage(m_Width, m_Height, colorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_ColorImage, m_ColorMemory);
    m_ColorImageView = m_Device->CreateImageView(m_ColorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    // Depth image
    m_Device->CreateImage(m_Width, m_Height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_DepthImage, m_DepthMemory);
    m_DepthImageView = m_Device->CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    // Framebuffer
    VkImageView fbAttachments[] = { m_ColorImageView, m_DepthImageView };
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_RenderPass;
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = fbAttachments;
    fbInfo.width = m_Width;
    fbInfo.height = m_Height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(dev, &fbInfo, nullptr, &m_Framebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create offscreen framebuffer");
}

void RenderTexture::DestroyResources()
{
    auto dev = m_Device->GetDevice();
    if (m_Framebuffer) vkDestroyFramebuffer(dev, m_Framebuffer, nullptr);
    m_Framebuffer = VK_NULL_HANDLE;
    if (m_DepthImageView) vkDestroyImageView(dev, m_DepthImageView, nullptr);
    if (m_DepthImage) vkDestroyImage(dev, m_DepthImage, nullptr);
    if (m_DepthMemory) vkFreeMemory(dev, m_DepthMemory, nullptr);
    m_DepthImageView = VK_NULL_HANDLE;
    m_DepthImage = VK_NULL_HANDLE;
    m_DepthMemory = VK_NULL_HANDLE;
    if (m_ColorImageView) vkDestroyImageView(dev, m_ColorImageView, nullptr);
    if (m_ColorImage) vkDestroyImage(dev, m_ColorImage, nullptr);
    if (m_ColorMemory) vkFreeMemory(dev, m_ColorMemory, nullptr);
    m_ColorImageView = VK_NULL_HANDLE;
    m_ColorImage = VK_NULL_HANDLE;
    m_ColorMemory = VK_NULL_HANDLE;
}

void RenderTexture::BeginRender(VkCommandBuffer cmd, VkClearValue clearColor, float depthClear)
{
    VkExtent2D extent = { m_Width, m_Height };

    // Transition color image to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearValue clearValues[2];
    clearValues[0] = clearColor;
    clearValues[1].depthStencil = { depthClear, 0 };

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_RenderPass;
    rpInfo.framebuffer = m_Framebuffer;
    rpInfo.renderArea.extent = extent;
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Viewport and scissor for the RT
    VkViewport vp{};
    vp.width = (float)m_Width;
    vp.height = (float)m_Height;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void RenderTexture::EndRender(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);

    // Transition color image to SHADER_READ_ONLY_OPTIMAL for sampling
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkDescriptorImageInfo RenderTexture::GetDescriptorInfo() const
{
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = m_ColorImageView;
    info.sampler = m_Sampler;
    return info;
}

void RenderTexture::CreateDescriptorResources()
{
    auto dev = m_Device->GetDevice();

    // Layout (structurally identical to UIRenderer's UI texture layout: binding 0,
    // combined image sampler, fragment). Descriptor sets from this layout are
    // compatible with the UI pipeline layout.
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    m_DescSetLayout = m_Device->CreateDescriptorSetLayout({ binding });

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };
    m_DescPool = m_Device->CreateDescriptorPool(poolSizes, 1);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescSetLayout;
    if (vkAllocateDescriptorSets(dev, &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate render texture descriptor set");

    UpdateDescriptor();
}

void RenderTexture::UpdateDescriptor()
{
    auto dev = m_Device->GetDevice();
    VkDescriptorImageInfo imgInfo = GetDescriptorInfo();
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}

void RenderTexture::DestroyDescriptorResources()
{
    auto dev = m_Device->GetDevice();
    if (m_DescPool) vkDestroyDescriptorPool(dev, m_DescPool, nullptr);
    if (m_DescSetLayout) vkDestroyDescriptorSetLayout(dev, m_DescSetLayout, nullptr);
    m_DescriptorSet = VK_NULL_HANDLE;
    m_DescPool = VK_NULL_HANDLE;
    m_DescSetLayout = VK_NULL_HANDLE;
}

} // namespace Leir
