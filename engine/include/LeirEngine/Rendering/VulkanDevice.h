#pragma once

#include "LeirEngine/Core/Export.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>
#include <functional>

struct GLFWwindow;

namespace Leir {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    bool IsComplete() const { return graphics.has_value() && present.has_value(); }
};

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct LEIR_API VulkanDeviceConfig {
    std::string appName = "LeirEngine";
    uint32_t appVersion = VK_MAKE_VERSION(0, 1, 0);
    int windowWidth = 1280;
    int windowHeight = 720;
    bool enableValidationLayers = true;
};

class LEIR_API VulkanDevice {
public:
    VulkanDevice(GLFWwindow* window, const VulkanDeviceConfig& config = {});
    ~VulkanDevice();

    // Frame lifecycle
    bool BeginFrame(bool skipRenderPass = false);
    void BeginOverlay();
    void BeginSwapchainOverlay();
    void EndFrame();

    // Helpers
    VkCommandBuffer GetCurrentCommandBuffer() const { return m_CommandBuffers[m_CurrentFrame]; }
    uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }
    VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkRenderPass GetOverlayRenderPass() const { return m_OverlayRenderPass; }
    VkDevice GetDevice() const { return m_Device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    VkCommandPool GetCommandPool() const { return m_CommandPool; }

    // Resource creation helpers
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    VkPipeline CreateGraphicsPipeline(
        VkPipelineLayout layout,
        VkRenderPass renderPass,
        const std::vector<VkPipelineShaderStageCreateInfo>& stages,
        VkVertexInputBindingDescription vertexBinding,
        const std::vector<VkVertexInputAttributeDescription>& vertexAttributes,
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
        bool depthTestEnable = true,
        bool blendEnable = false
    ) const;
    VkPipelineLayout CreatePipelineLayout(
        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<VkPushConstantRange>& pushConstants = {}
    ) const;
    VkDescriptorSetLayout CreateDescriptorSetLayout(
        const std::vector<VkDescriptorSetLayoutBinding>& bindings
    ) const;
    VkDescriptorPool CreateDescriptorPool(
        const std::vector<VkDescriptorPoolSize>& poolSizes,
        uint32_t maxSets
    ) const;

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Buffer helpers
    VkBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const;
    void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;
    void CreateImage(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& memory) const;
    VkImageView CreateImageView(VkImage image, VkFormat format,
        VkImageAspectFlags aspectFlags) const;
    VkSampler CreateSampler() const;

    // Resize
    bool WasResized() const { return m_FramebufferResized; }
    void ResetResized() { m_FramebufferResized = false; }
    void RecreateSwapchain();

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface(GLFWwindow* window);
    void CreateSwapchain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateCommandPool();
    void CreateDepthResources();
    void TransitionImageLayout(VkImage image, VkFormat format,
        VkImageLayout oldLayout, VkImageLayout newLayout);
    void CreateFramebuffers();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapchain();

    bool IsDeviceSuitable(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupport QuerySwapchainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* userData);

    VulkanDeviceConfig m_Config;

    // Core
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_SwapchainImages;
    VkFormat m_SwapchainFormat;
    VkExtent2D m_SwapchainExtent;
    std::vector<VkImageView> m_SwapchainImageViews;
    std::vector<VkFramebuffer> m_SwapchainFramebuffers;

    // Depth
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;

    // Render pass
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkRenderPass m_OverlayRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_OverlayFramebuffers;

    // Command pools
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;

    // Sync
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    uint32_t m_CurrentFrame = 0;
    bool m_FramebufferResized = false;

    // Command buffers
    std::vector<VkCommandBuffer> m_CommandBuffers;

    // Temp storage for current swapchain image index
    uint32_t m_CurrentImageIndex = 0;
    bool m_InOverlay = false;

    // Validation
    std::vector<const char*> m_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
    std::vector<const char*> m_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};

} // namespace Leir
