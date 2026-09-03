#include "LeirEngine/Rendering/SwapchainTarget.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "LeirEngine/Core/Log.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Leir {

SwapchainTarget::SwapchainTarget(VkInstance instance, VkPhysicalDevice physicalDevice,
                                 VkDevice device, VkQueue graphicsQueue, VkQueue presentQueue,
                                 GLFWwindow* window,
                                 VkRenderPass renderPass, VkRenderPass overlayRenderPass,
                                 bool vsync)
    : m_Instance(instance)
    , m_PhysicalDevice(physicalDevice)
    , m_Device(device)
    , m_GraphicsQueue(graphicsQueue)
    , m_PresentQueue(presentQueue)
    , m_RenderPass(renderPass)
    , m_OverlayRenderPass(overlayRenderPass)
    , m_Vsync(vsync)
    , m_Window(window)
{
    CreateSurface();

    // Fase 2: each window gets its OWN command pool instead of sharing the
    // main device's pool. This eliminates any cross-window interference from
    // command buffer allocation/reset within a shared pool.
    {
        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, families.data());
        uint32_t graphicsFam = UINT32_MAX;
        for (uint32_t i = 0; i < queueCount; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsFam = i; break; }
        }
        if (graphicsFam == UINT32_MAX)
            throw std::runtime_error("SwapchainTarget: no graphics queue family");

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsFam;
        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
            throw std::runtime_error("SwapchainTarget: failed to create command pool");
        XConsole::Debug("SwapchainTarget: own command pool created (fam {})", graphicsFam);
    }

    CreateSwapchain();
    CreateSwapchain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
    CreateCommandBuffers();
    CreateSyncObjects();
}

SwapchainTarget::~SwapchainTarget()
{
    if (m_Device == VK_NULL_HANDLE)
        return;
    vkDeviceWaitIdle(m_Device);
    CleanupSwapchain();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
}

// ---- Surface ----

void SwapchainTarget::CreateSurface()
{
    if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to create window surface");
    XConsole::Debug("SwapchainTarget: surface created");
}

// ---- Swapchain ----

VkSurfaceFormatKHR SwapchainTarget::ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR SwapchainTarget::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
    if (m_Vsync) {
        for (const auto& m : modes) {
            if (m == VK_PRESENT_MODE_FIFO_KHR) return m;
        }
    } else {
        for (const auto& m : modes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainTarget::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps) const
{
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    int w, h;
    glfwGetFramebufferSize(m_Window, &w, &h);
    return {
        std::clamp((uint32_t)w, caps.minImageExtent.width, caps.maxImageExtent.width),
        std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height)
    };
}

void SwapchainTarget::CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    if (modeCount)
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &modeCount, modes.data());

    auto format = ChooseFormat(formats);
    auto presentMode = ChoosePresentMode(modes);
    auto extent = ChooseExtent(caps);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    // Find graphics + present queue families for this surface.
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueCount, families.data());

    uint32_t graphicsFam = UINT32_MAX, presentFam = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            graphicsFam = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &present);
        if (present) presentFam = i;
        if (graphicsFam != UINT32_MAX && presentFam != UINT32_MAX) break;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_Surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t fams[] = { graphicsFam, presentFam };
    if (graphicsFam != presentFam) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = fams;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to create swapchain");

    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_Images.resize(imageCount);
    m_ImagesInFlight.resize(imageCount, VK_NULL_HANDLE);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data());

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_RenderFinishedSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
        vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]);

    m_Format = format.format;
    m_Extent = extent;
}

void SwapchainTarget::CreateImageViews()
{
    m_ImageViews.resize(m_Images.size());
    for (size_t i = 0; i < m_Images.size(); ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = m_Images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = m_Format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_Device, &info, nullptr, &m_ImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("SwapchainTarget: failed to create image view");
    }
}

// ---- Depth ----

void SwapchainTarget::CreateDepthResources()
{
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.extent = { m_Extent.width, m_Extent.height, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.format = depthFormat;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &imgInfo, nullptr, &m_DepthImage) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to create depth image");

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(m_Device, m_DepthImage, &reqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProps);

    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((reqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX)
        throw std::runtime_error("SwapchainTarget: no suitable memory type for depth");

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = reqs.size;
    alloc.memoryTypeIndex = memType;
    if (vkAllocateMemory(m_Device, &alloc, nullptr, &m_DepthMemory) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to allocate depth memory");

    vkBindImageMemory(m_Device, m_DepthImage, m_DepthMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to create depth image view");

    TransitionImageLayout(m_DepthImage, depthFormat,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void SwapchainTarget::TransitionImageLayout(VkImage image, VkFormat /*format*/,
    VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);

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
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
}

// ---- Framebuffers ----

void SwapchainTarget::CreateFramebuffers()
{
    m_Framebuffers.resize(m_ImageViews.size());
    m_OverlayFramebuffers.resize(m_ImageViews.size());
    for (size_t i = 0; i < m_ImageViews.size(); ++i) {
        VkImageView attachments[] = { m_ImageViews[i], m_DepthImageView };
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_RenderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = m_Extent.width;
        info.height = m_Extent.height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_Device, &info, nullptr, &m_Framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("SwapchainTarget: failed to create framebuffer");

        VkFramebufferCreateInfo overlayInfo{};
        overlayInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        overlayInfo.renderPass = m_OverlayRenderPass;
        overlayInfo.attachmentCount = 1;
        overlayInfo.pAttachments = &m_ImageViews[i];
        overlayInfo.width = m_Extent.width;
        overlayInfo.height = m_Extent.height;
        overlayInfo.layers = 1;
        if (vkCreateFramebuffer(m_Device, &overlayInfo, nullptr, &m_OverlayFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("SwapchainTarget: failed to create overlay framebuffer");
    }
}

// ---- Command Buffers + Sync ----

void SwapchainTarget::CreateCommandBuffers()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = m_CommandPool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = (uint32_t)m_CommandBuffers.size();
    if (vkAllocateCommandBuffers(m_Device, &info, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("SwapchainTarget: failed to allocate command buffers");
}

void SwapchainTarget::CreateSyncObjects()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_ImageAvailableSemaphores[i]);
        vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]);
    }
}

// ---- Frame Lifecycle ----

bool SwapchainTarget::BeginFrame(uint32_t& outImageIndex)
{
    // Minimized windows have no presentable surface: the swapchain is 0×0 and
    // vkAcquireNextImageKHR(UINT64_MAX) would block forever, freezing the whole
    // main loop (the editor renders ALL windows on the main thread). Skip the
    // frame while iconified; the resize callback fires again on restore.
    if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
        return false;

    if (m_NeedsResize) {
        RecreateSwapchain();
        return false;
    }

    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("SwapchainTarget: failed to acquire swapchain image");

    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(m_Device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    m_ImagesInFlight[imageIndex] = m_InFlightFences[m_CurrentFrame];

    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &beginInfo);

    m_InOverlay = false;
    outImageIndex = imageIndex;
    return true;
}

void SwapchainTarget::BeginClearRenderPass(uint32_t imageIndex, const VkClearValue& clearColor, float depthClear)
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    VkClearValue clearValues[2];
    clearValues[0] = clearColor;
    clearValues[1].depthStencil = { depthClear, 0 };

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_RenderPass;
    rpInfo.framebuffer = m_Framebuffers[imageIndex];
    rpInfo.renderArea.extent = m_Extent;
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = (float)m_Extent.height;
    viewport.width = (float)m_Extent.width;
    viewport.height = -(float)m_Extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_Extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_InOverlay = false;
}

void SwapchainTarget::BeginOverlayRenderPass(uint32_t imageIndex)
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    // Transition swapchain image from UNDEFINED → COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Images[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_OverlayRenderPass;
    rpInfo.framebuffer = m_OverlayFramebuffers[imageIndex];
    rpInfo.renderArea.extent = m_Extent;
    rpInfo.clearValueCount = 0;
    rpInfo.pClearValues = nullptr;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = (float)m_Extent.height;
    viewport.width = (float)m_Extent.width;
    viewport.height = -(float)m_Extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_Extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_InOverlay = true;
}

void SwapchainTarget::EndFrame(uint32_t imageIndex)
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore renderFinished = m_RenderFinishedSemaphores[imageIndex];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphores[m_CurrentFrame];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished;

    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_NeedsResize) {
        m_NeedsResize = false;
        RecreateSwapchain();
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ---- Cleanup / Recreate ----

void SwapchainTarget::CleanupSwapchain()
{
    if (m_Device == VK_NULL_HANDLE)
        return;

    vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
    m_DepthImageView = VK_NULL_HANDLE;
    vkDestroyImage(m_Device, m_DepthImage, nullptr);
    m_DepthImage = VK_NULL_HANDLE;
    vkFreeMemory(m_Device, m_DepthMemory, nullptr);
    m_DepthMemory = VK_NULL_HANDLE;

    for (auto sem : m_RenderFinishedSemaphores)
        vkDestroySemaphore(m_Device, sem, nullptr);
    m_RenderFinishedSemaphores.clear();

    for (auto fb : m_Framebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    m_Framebuffers.clear();
    for (auto fb : m_OverlayFramebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    m_OverlayFramebuffers.clear();
    for (auto iv : m_ImageViews)
        vkDestroyImageView(m_Device, iv, nullptr);
    m_ImageViews.clear();
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    m_Swapchain = VK_NULL_HANDLE;
}

void SwapchainTarget::RecreateSwapchain()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_Window, &w, &h);
    // Defensive: a minimized window reports 0×0 and never fires more events —
    // the wait loop below would spin forever. Keep the swapchain marked dirty
    // and bail; BeginFrame() skips iconified windows and will rebuild on restore.
    if ((w == 0 || h == 0) && glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
        return;
    int waits = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_Window, &w, &h);
        if (w == 0 || h == 0) {
            ++waits;
            glfwWaitEvents();
        }
    }
    if (waits > 0)
        XConsole::Debug("[Timing] SwapchainTarget::RecreateSwapchain waited {} times", waits);

    vkDeviceWaitIdle(m_Device);
    CleanupSwapchain();
    CreateSwapchain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
    m_NeedsResize = false;
}

} // namespace Leir