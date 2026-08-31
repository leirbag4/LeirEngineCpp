#pragma once

/**
 * @file SwapchainTarget.h
 * @brief Self-contained per-window swapchain + present pipeline (shared Vulkan device).
 * @ingroup Rendering
 *
 * Owns everything needed to present to ONE window while sharing the physical
 * device, logical device, queues and command pool from a parent VulkanDevice:
 * surface, swapchain, image views, depth, framebuffers, per-image semaphores,
 * per-frame acquire semaphores + fences, and command buffers. This is what lets
 * external (detached) editor windows render their own UI to their own OS window.
 */

#include "LeirEngine/Core/Export.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace Leir {

/**
 * @brief Self-contained present target for one window.
 * @ingroup Rendering
 */
class LEIR_API SwapchainTarget {
public:
    /**
     * @brief Constructs the present target for a GLFW window.
     * @param[in] instance Vulkan instance.
     * @param[in] physicalDevice Physical device.
     * @param[in] device Logical device (shared).
     * @param[in] graphicsQueue Graphics queue (shared).
     * @param[in] presentQueue Present queue (shared).
     * @param[in] commandPool Command pool (shared).
     * @param[in] window GLFW window.
     * @param[in] renderPass Main render pass (with depth).
     * @param[in] overlayRenderPass Overlay render pass (color only).
     * @param[in] vsync V-sync.
     */
    SwapchainTarget(VkInstance instance, VkPhysicalDevice physicalDevice,
                    VkDevice device, VkQueue graphicsQueue, VkQueue presentQueue,
                    VkCommandPool commandPool, GLFWwindow* window,
                    VkRenderPass renderPass, VkRenderPass overlayRenderPass,
                    bool vsync);

    ~SwapchainTarget();

    SwapchainTarget(const SwapchainTarget&) = delete;
    SwapchainTarget& operator=(const SwapchainTarget&) = delete;

    /**
     * @brief Acquires the next swapchain image and begins the command buffer.
     * @details Caller then records the UI, then calls EndFrame().
     * @param[out] outImageIndex Acquired image index.
     * @return False if the swapchain was recreated (call again next frame).
     */
    bool BeginFrame(uint32_t& outImageIndex);

    /**
     * @brief Begins the main render pass (LOOP_OP_CLEAR) on the current image.
     * @details Used to clear the window to a solid color (for tests or 3D content).
     * @param[in] imageIndex Acquired image index.
     * @param[in] clearColor Clear color for the framebuffer.
     * @param[in] depthClear Depth clear value (default 1.0f).
     */
    void BeginClearRenderPass(uint32_t imageIndex, const VkClearValue& clearColor, float depthClear = 1.0f);

    /**
     * @brief Begins the overlay render pass (LOAD_OP_LOAD) on the current image.
     * @param[in] imageIndex Acquired image index.
     */
    void BeginOverlayRenderPass(uint32_t imageIndex);

    /**
     * @brief Ends the command buffer, submits and presents.
     * @param[in] imageIndex Acquired image index.
     */
    void EndFrame(uint32_t imageIndex);

    /**
     * @brief Recreates the swapchain after a resize/out-of-date event.
     */
    void RecreateSwapchain();

    // ---- Getters ----
    VkExtent2D GetExtent() const { return m_Extent; }
    VkFormat GetFormat() const { return m_Format; }
    VkFramebuffer GetOverlayFramebuffer(uint32_t imageIndex) const { return m_OverlayFramebuffers[imageIndex]; }
    VkSurfaceKHR GetSurface() const { return m_Surface; }
    VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
    VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffers[m_CurrentFrame]; }
    uint32_t GetImageCount() const { return (uint32_t)m_Images.size(); }
    GLFWwindow* GetWindow() const { return m_Window; }
    bool IsValid() const { return m_Swapchain != VK_NULL_HANDLE; }

    /**
     * @brief Marks the swapchain as needing recreation (resize callback).
     */
    void MarkResized() { m_NeedsResize = true; }
    bool NeedsResize() const { return m_NeedsResize; }
    bool WasResized() const { return m_NeedsResize; }
    void ResetResized() { m_NeedsResize = false; }

private:
    void CreateSurface();
    void CreateSwapchain();
    void CreateImageViews();
    void CreateDepthResources();
    void CreateFramebuffers();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapchain();

    VkSurfaceFormatKHR ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps) const;
    void TransitionImageLayout(VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);

    // Shared state (from parent VulkanDevice)
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkRenderPass m_OverlayRenderPass = VK_NULL_HANDLE;
    bool m_Vsync = true;

    // Per-window state
    GLFWwindow* m_Window = nullptr;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_Images;
    VkFormat m_Format;
    VkExtent2D m_Extent;
    std::vector<VkImageView> m_ImageViews;
    std::vector<VkFramebuffer> m_Framebuffers;
    std::vector<VkFramebuffer> m_OverlayFramebuffers;

    // Depth
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;

    // Per-swapchain-image render-finished semaphores
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_ImagesInFlight; // per swapchain image

    // Per-frame sync + command buffers
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    uint32_t m_CurrentFrame = 0;

    // Resize tracking
    bool m_NeedsResize = false;
    bool m_InOverlay = false;
};

} // namespace Leir