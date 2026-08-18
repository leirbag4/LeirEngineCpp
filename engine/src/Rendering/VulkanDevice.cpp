#include "LeirEngine/Rendering/VulkanDevice.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "LeirEngine/Core/Log.h"
#include <set>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Leir {

// ---- Helpers ----

static std::vector<const char*> GetRequiredExtensions(bool enableValidation) {
    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> result(extensions, extensions + count);
    if (enableValidation)
        result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return result;
}

// ---- Constructor / Destructor ----

VulkanDevice::VulkanDevice(GLFWwindow* window, const VulkanDeviceConfig& config)
    : m_Config(config)
    , m_Window(window)
{
    CreateInstance();
    if (config.enableValidationLayers)
        SetupDebugMessenger();
    CreateSurface(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateImageViews();
    CreateRenderPass();
    CreateCommandPool();
    CreateDepthResources();
    CreateFramebuffers();
    CreateCommandBuffers();
    CreateSyncObjects();
    XConsole::Println("Vulkan device initialized");
}

VulkanDevice::~VulkanDevice()
{
    auto ms = [](std::chrono::steady_clock::time_point a,
                 std::chrono::steady_clock::time_point b) -> double {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    auto t0 = std::chrono::steady_clock::now();
    vkDeviceWaitIdle(m_Device);
    auto t1 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] VulkanDevice vkDeviceWaitIdle: {:.1f} ms", ms(t0, t1));
    CleanupSwapchain();
    // Render passes persist across swapchain recreations (format rarely changes)
    if (m_OverlayRenderPass) {
        vkDestroyRenderPass(m_Device, m_OverlayRenderPass, nullptr);
        m_OverlayRenderPass = VK_NULL_HANDLE;
    }
    if (m_RenderPass) {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }
    // m_RenderFinishedSemaphores (per swapchain image) are freed in CleanupSwapchain.
    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    auto t2 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] VulkanDevice resource destruction: {:.1f} ms", ms(t1, t2));
    vkDestroyDevice(m_Device, nullptr);
    if (m_Config.enableValidationLayers) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_Instance, m_DebugMessenger, nullptr);
    }
    auto t3 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] VulkanDevice vkDestroyDevice+instance: {:.1f} ms", ms(t2, t3));
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);
    XConsole::Debug("[Timing] VulkanDevice dtor total: {:.1f} ms", ms(t0,
        std::chrono::steady_clock::now()));
}

// ---- Instance ----

void VulkanDevice::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_Config.appName.c_str();
    appInfo.applicationVersion = m_Config.appVersion;
    appInfo.pEngineName = "LeirEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions(m_Config.enableValidationLayers);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (m_Config.enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = DebugCallback;
        createInfo.pNext = &debugInfo;
        createInfo.enabledLayerCount = (uint32_t)m_ValidationLayers.size();
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        XConsole::PrintError("Failed to create Vulkan instance");
        throw std::runtime_error("Vulkan instance creation failed");
    }
    XConsole::Println("Vulkan instance created");
}

// ---- Debug Messenger ----

void VulkanDevice::SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        m_Instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        if (func(m_Instance, &info, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
            XConsole::PrintWarning("Failed to set up Vulkan debug messenger");
        }
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        XConsole::PrintError("Vulkan: {}", data->pMessage);
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        XConsole::PrintWarning("Vulkan: {}", data->pMessage);
    return VK_FALSE;
}

// ---- Surface ----

void VulkanDevice::CreateSurface(GLFWwindow* window)
{
    if (glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface");
}

// ---- Physical Device ----

void VulkanDevice::PickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }
    if (m_PhysicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("No suitable GPU found");

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    XConsole::Println("GPU: {} (type: {})", props.deviceName,
        props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete" : "integrated");
}

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device)
{
    auto indices = FindQueueFamilies(device);
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    bool swapchainAdequate = false;
    if (extensionsSupported) {
        auto support = QuerySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }
    return indices.IsComplete() && extensionsSupported && swapchainAdequate;
}

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device) const
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &present);
        if (present) indices.present = i;
        if (indices.IsComplete()) break;
    }
    return indices;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    required.insert(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
    for (const auto& ext : available)
        required.erase(ext.extensionName);
    return required.empty();
}

SwapchainSupport VulkanDevice::QuerySwapchainSupport(VkPhysicalDevice device) const
{
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &support.capabilities);
    XConsole::Println("Surface capabilities: currentExtent {}x{}, min {}x{}, max {}x{}",
        support.capabilities.currentExtent.width, support.capabilities.currentExtent.height,
        support.capabilities.minImageExtent.width, support.capabilities.minImageExtent.height,
        support.capabilities.maxImageExtent.width, support.capabilities.maxImageExtent.height);
    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &count, nullptr);
    if (count) {
        support.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &count, support.formats.data());
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &count, nullptr);
    if (count) {
        support.presentModes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &count, support.presentModes.data());
    }
    return support;
}

// ---- Logical Device ----

void VulkanDevice::CreateLogicalDevice()
{
    auto indices = FindQueueFamilies(m_PhysicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueFams = { indices.graphics.value(), indices.present.value() };

    float priority = 1.0f;
    for (uint32_t fam : uniqueFams) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = fam;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.samplerAnisotropy = VK_TRUE;

    // Descriptor indexing (bindless textures, TODO_RHI_SLANG.md §3.5): core in
    // Vulkan 1.2, exposed via VkPhysicalDeviceFeatures2 chained below. We first
    // query what the device supports (some are optional) and enable only the
    // features the engine's bindless table needs.
    VkPhysicalDeviceDescriptorIndexingFeatures supported{};
    supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    {
        VkPhysicalDeviceFeatures2 probe{};
        probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        probe.pNext = &supported;
        vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &probe);
    }

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexing{};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexing.runtimeDescriptorArray = VK_TRUE;
    descriptorIndexing.descriptorBindingPartiallyBound = VK_TRUE;
    // Update-after-bind lets the bindless table exceed the non-update-after-bind
    // per-stage sampler/resource limits (VUID-VkPipelineLayoutCreateInfo-
    // descriptorType-03016 / VUID-VkGraphicsPipelineCreateInfo-layout-01688).
    // The generic descriptorBindingUpdateAfterBind feature was removed from the
    // Vulkan 1.4 headers — the per-type descriptorBindingSampledImageUpdateAfterBind
    // is the one relevant to the combined-image-sampler bindless table.
    descriptorIndexing.descriptorBindingSampledImageUpdateAfterBind =
        supported.descriptorBindingSampledImageUpdateAfterBind;
    m_DescriptorIndexingUpdateAfterBind = supported.descriptorBindingSampledImageUpdateAfterBind;
    features2.pNext = &descriptorIndexing;

    std::vector<const char*> deviceExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = (uint32_t)queueInfos.size();
    info.pQueueCreateInfos = queueInfos.data();
    info.pNext = &features2;
    info.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    info.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_PhysicalDevice, &info, nullptr, &m_Device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");

    vkGetDeviceQueue(m_Device, indices.graphics.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, indices.present.value(), 0, &m_PresentQueue);
}

// ---- Swapchain ----

void VulkanDevice::CreateSwapchain()
{
    auto support = QuerySwapchainSupport(m_PhysicalDevice);
    auto format = ChooseSwapchainFormat(support.formats);
    auto presentMode = ChoosePresentMode(support.presentModes);
    auto extent = ChooseSwapchainExtent(support.capabilities);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_Surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = FindQueueFamilies(m_PhysicalDevice);
    uint32_t fams[] = { indices.graphics.value(), indices.present.value() };
    if (indices.graphics != indices.present) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = fams;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");

    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_SwapchainImages.resize(imageCount);
    m_ImagesInFlight.resize(imageCount, VK_NULL_HANDLE);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

    // One render-finished semaphore per swapchain image, so a presented image's
    // semaphore is only reused once that image is re-acquired (indexing by the
    // frame slot reused the semaphore while the swapchain still referenced it
    // → VUID-vkQueueSubmit-pSignalSemaphores-00067).
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    m_RenderFinishedSemaphores.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
        vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_RenderFinishedSemaphores[i]);
    m_SwapchainFormat = format.format;
    m_SwapchainExtent = extent;
}

VkSurfaceFormatKHR VulkanDevice::ChooseSwapchainFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR VulkanDevice::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
    if (m_Config.vsync) {
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

VkExtent2D VulkanDevice::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& caps) const
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

void VulkanDevice::CreateImageViews()
{
    m_SwapchainImageViews.resize(m_SwapchainImages.size());
    for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
        m_SwapchainImageViews[i] = CreateImageView(
            m_SwapchainImages[i], m_SwapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

// ---- Render Pass ----

void VulkanDevice::CreateRenderPass()
{
    VkAttachmentDescription color{};
    color.format = m_SwapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format = VK_FORMAT_D32_SFLOAT;
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
    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    if (vkCreateRenderPass(m_Device, &info, nullptr, &m_RenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");

    // Overlay render pass (no depth, loads existing color)
    VkAttachmentDescription overlayColor{};
    overlayColor.format = m_SwapchainFormat;
    overlayColor.samples = VK_SAMPLE_COUNT_1_BIT;
    overlayColor.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    overlayColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    overlayColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    overlayColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    overlayColor.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    overlayColor.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference overlayColorRef{};
    overlayColorRef.attachment = 0;
    overlayColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription overlaySubpass{};
    overlaySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    overlaySubpass.colorAttachmentCount = 1;
    overlaySubpass.pColorAttachments = &overlayColorRef;

    VkRenderPassCreateInfo overlayInfo{};
    overlayInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    overlayInfo.attachmentCount = 1;
    overlayInfo.pAttachments = &overlayColor;
    overlayInfo.subpassCount = 1;
    overlayInfo.pSubpasses = &overlaySubpass;

    if (vkCreateRenderPass(m_Device, &overlayInfo, nullptr, &m_OverlayRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create overlay render pass");
}

// ---- Command Pool ----

void VulkanDevice::CreateCommandPool()
{
    auto indices = FindQueueFamilies(m_PhysicalDevice);
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = indices.graphics.value();
    if (vkCreateCommandPool(m_Device, &info, nullptr, &m_CommandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}

// ---- Depth Resources ----

void VulkanDevice::CreateDepthResources()
{
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    CreateImage(m_SwapchainExtent.width, m_SwapchainExtent.height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthMemory);
    m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    TransitionImageLayout(m_DepthImage, depthFormat,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void VulkanDevice::TransitionImageLayout(VkImage image, VkFormat format,
    VkImageLayout oldLayout, VkImageLayout newLayout)
{
    (void)format;
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

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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

void VulkanDevice::CreateFramebuffers()
{
    m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
    m_OverlayFramebuffers.resize(m_SwapchainImageViews.size());
    for (size_t i = 0; i < m_SwapchainImageViews.size(); ++i) {
        VkImageView attachments[] = { m_SwapchainImageViews[i], m_DepthImageView };
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_RenderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = m_SwapchainExtent.width;
        info.height = m_SwapchainExtent.height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_Device, &info, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");

        // Overlay framebuffer (color only, no depth)
        VkFramebufferCreateInfo overlayInfo{};
        overlayInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        overlayInfo.renderPass = m_OverlayRenderPass;
        overlayInfo.attachmentCount = 1;
        overlayInfo.pAttachments = &m_SwapchainImageViews[i];
        overlayInfo.width = m_SwapchainExtent.width;
        overlayInfo.height = m_SwapchainExtent.height;
        overlayInfo.layers = 1;
        if (vkCreateFramebuffer(m_Device, &overlayInfo, nullptr, &m_OverlayFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create overlay framebuffer");
    }
}

// ---- Command Buffers ----

void VulkanDevice::CreateCommandBuffers()
{
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = m_CommandPool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = (uint32_t)m_CommandBuffers.size();
    if (vkAllocateCommandBuffers(m_Device, &info, m_CommandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}

// ---- Sync ----

void VulkanDevice::CreateSyncObjects()
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

    // Render-finished semaphores are created per swapchain image in
    // CreateSwapchain (which runs BEFORE this in the constructor) and indexed
    // by the acquired image, not by frame. Do NOT touch that vector here.
}

// ---- Frame Lifecycle ----

bool VulkanDevice::BeginFrame(bool skipRenderPass)
{
    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swapchain image");

    // If the swapchain image was used by a previous frame, wait for that frame
    // to finish before writing to it again (prevents re-acquiring an image that
    // is still being presented, which reuses the acquire semaphore while it is
    // in flight → VUID-vkQueueSubmit-pSignalSemaphores-00067).
    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(m_Device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    m_ImagesInFlight[imageIndex] = m_InFlightFences[m_CurrentFrame];

    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkResetCommandBuffer(cmd, 0);
    vkBeginCommandBuffer(cmd, &beginInfo);

    m_CurrentImageIndex = imageIndex;
    m_InOverlay = false;

    if (!skipRenderPass) {
        VkClearValue clearValues[2];
        clearValues[0].color = { {0.15f, 0.15f, 0.2f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_RenderPass;
        rpInfo.framebuffer = m_SwapchainFramebuffers[imageIndex];
        rpInfo.renderArea.extent = m_SwapchainExtent;
        rpInfo.clearValueCount = 2;
        rpInfo.pClearValues = clearValues;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Flip viewport Y: GLM/NDC (y-up) → Vulkan framebuffer (y-down)
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = (float)m_SwapchainExtent.height;
        viewport.width = (float)m_SwapchainExtent.width;
        viewport.height = -(float)m_SwapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = m_SwapchainExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    return true;
}

void VulkanDevice::BeginOverlay()
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];
    vkCmdEndRenderPass(cmd);

    // Transition swapchain image from PRESENT_SRC_KHR â†’ COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SwapchainImages[m_CurrentImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_OverlayRenderPass;
    rpInfo.framebuffer = m_OverlayFramebuffers[m_CurrentImageIndex];
    rpInfo.renderArea.extent = m_SwapchainExtent;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Flip viewport Y: screen coords (top-left origin) â†’ NDC (bottom-left)
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = (float)m_SwapchainExtent.height;
    viewport.width = (float)m_SwapchainExtent.width;
    viewport.height = -(float)m_SwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_InOverlay = true;
}

void VulkanDevice::BeginSwapchainOverlay()
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SwapchainImages[m_CurrentImageIndex];
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
    rpInfo.framebuffer = m_OverlayFramebuffers[m_CurrentImageIndex];
    rpInfo.renderArea.extent = m_SwapchainExtent;
    rpInfo.clearValueCount = 0;
    rpInfo.pClearValues = nullptr;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Flip viewport Y for screen-space coordinates
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = (float)m_SwapchainExtent.height;
    viewport.width = (float)m_SwapchainExtent.width;
    viewport.height = -(float)m_SwapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_SwapchainExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_InOverlay = true;
}

void VulkanDevice::EndFrame()
{
    VkCommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore renderFinished = m_RenderFinishedSemaphores[m_CurrentImageIndex];

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
    presentInfo.pImageIndices = &m_CurrentImageIndex;

    VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
        m_FramebufferResized = false;
        RecreateSwapchain();
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ---- Recreate ----

void VulkanDevice::RecreateSwapchain()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_Window, &w, &h);
    if (w == 0 || h == 0)
        XConsole::Debug("[Timing] RecreateSwapchain: framebuffer is 0x0 (w={} h={}), entering wait loop", w, h);
    int waits = 0;
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(m_Window, &w, &h);
        if (w == 0 || h == 0) {
            ++waits;
            if (waits == 1 || (waits % 30) == 0)
                XConsole::Debug("[Timing] RecreateSwapchain waiting for nonzero fb... (waits={}, w={}, h={})", waits, w, h);
        }
        glfwWaitEvents();
    }
    if (waits > 0)
        XConsole::Debug("[Timing] RecreateSwapchain left wait loop after {} waits (fb {}x{})", waits, w, h);

    auto t0 = std::chrono::steady_clock::now();
    vkDeviceWaitIdle(m_Device);
    auto t1 = std::chrono::steady_clock::now();
    if (std::chrono::duration<double, std::milli>(t1 - t0).count() > 5.0)
        XConsole::Debug("[Timing] RecreateSwapchain vkDeviceWaitIdle: {:.1f} ms",
            std::chrono::duration<double, std::milli>(t1 - t0).count());
    CleanupSwapchain();

    CreateSwapchain();
    CreateImageViews();
    CreateDepthResources();
    CreateFramebuffers();
    auto t2 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] RecreateSwapchain done ({:.1f} ms)", 
        std::chrono::duration<double, std::milli>(t2 - t0).count());
}

void VulkanDevice::CleanupSwapchain()
{
    vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
    vkDestroyImage(m_Device, m_DepthImage, nullptr);
    vkFreeMemory(m_Device, m_DepthMemory, nullptr);

    for (auto sem : m_RenderFinishedSemaphores)
        vkDestroySemaphore(m_Device, sem, nullptr);
    m_RenderFinishedSemaphores.clear();

    for (auto fb : m_SwapchainFramebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    for (auto fb : m_OverlayFramebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    m_OverlayFramebuffers.clear();
    for (auto iv : m_SwapchainImageViews)
        vkDestroyImageView(m_Device, iv, nullptr);
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

// ---- Resource Helpers ----

VkShaderModule VulkanDevice::CreateShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(m_Device, &info, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");
    return module;
}

VkPipeline VulkanDevice::CreateGraphicsPipeline(
    VkPipelineLayout layout,
    VkRenderPass renderPass,
    const std::vector<VkPipelineShaderStageCreateInfo>& stages,
    VkVertexInputBindingDescription vertexBinding,
    const std::vector<VkVertexInputAttributeDescription>& vertexAttributes,
    VkPrimitiveTopology topology,
    VkPolygonMode polygonMode,
    VkCullModeFlags cullMode,
    bool depthTestEnable,
    bool depthWriteEnable,
    bool blendEnable) const
{
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = (uint32_t)vertexAttributes.size();
    vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = polygonMode;
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (blendEnable) {
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        blend.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blend;

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = (uint32_t)stages.size();
    info.pStages = stages.data();
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &colorBlend;
    info.pDynamicState = &dynamicState;
    info.layout = layout;
    info.renderPass = renderPass;

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");
    return pipeline;
}

VkPipelineLayout VulkanDevice::CreatePipelineLayout(
    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
    const std::vector<VkPushConstantRange>& pushConstants) const
{
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    info.pSetLayouts = descriptorSetLayouts.data();
    info.pushConstantRangeCount = (uint32_t)pushConstants.size();
    info.pPushConstantRanges = pushConstants.data();

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(m_Device, &info, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");
    return layout;
}

VkDescriptorSetLayout VulkanDevice::CreateDescriptorSetLayout(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) const
{
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = (uint32_t)bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(m_Device, &info, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
    return layout;
}

VkDescriptorSetLayout VulkanDevice::CreateBindlessDescriptorSetLayout(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) const
{
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    // Partially bound: not every array element must be written before use
    // (the global bindless table starts empty). The set-level
    // VK_DESCRIPTOR_SET_LAYOUT_CREATE_PARTIALLY_BOUND_BIT was removed in the
    // Vulkan 1.4 headers — the capability is per-binding via
    // VkDescriptorSetLayoutBindingFlagsCreateInfo now.
    std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(),
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo.bindingCount = (uint32_t)bindings.size();
    flagsInfo.pBindingFlags = bindingFlags.data();
    info.pNext = &flagsInfo;

    // Update-after-bind: exempts the binding from the non-update-after-bind
    // per-stage limits (maxPerStageDescriptorSamplers / maxPerStageResources),
    // which are too small for a real bindless table (64/200 on this iGPU).
    if (m_DescriptorIndexingUpdateAfterBind)
        info.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    info.bindingCount = (uint32_t)bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(m_Device, &info, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create bindless descriptor set layout");
    return layout;
}

VkDescriptorPool VulkanDevice::CreateDescriptorPool(
    const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets,
    VkDescriptorPoolCreateFlags flags) const
{
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = flags;
    info.poolSizeCount = (uint32_t)poolSizes.size();
    info.pPoolSizes = poolSizes.data();
    info.maxSets = maxSets;

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(m_Device, &info, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
    return pool;
}

uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

VkBuffer VulkanDevice::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkDeviceMemory& memory) const
{
    VkBuffer buffer;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &info, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(m_Device, buffer, &reqs);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = reqs.size;
    alloc.memoryTypeIndex = FindMemoryType(reqs.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &alloc, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");

    vkBindBufferMemory(m_Device, buffer, memory, 0);
    return buffer;
}

void VulkanDevice::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const
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

    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &cmd);
}

void VulkanDevice::CreateImage(uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties, VkImage& image,
    VkDeviceMemory& memory) const
{
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = { width, height, 1 };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = format;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image");

    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(m_Device, image, &reqs);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = reqs.size;
    alloc.memoryTypeIndex = FindMemoryType(reqs.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &alloc, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory");

    vkBindImageMemory(m_Device, image, memory, 0);
}

VkImageView VulkanDevice::CreateImageView(VkImage image, VkFormat format,
    VkImageAspectFlags aspectFlags) const
{
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange.aspectMask = aspectFlags;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    VkImageView view;
    if (vkCreateImageView(m_Device, &info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view");
    return view;
}

VkSampler VulkanDevice::CreateSampler() const
{
    return CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

VkSampler VulkanDevice::CreateSampler(VkFilter filter, VkSamplerAddressMode addressMode) const
{
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = filter;
    info.minFilter = filter;
    info.addressModeU = addressMode;
    info.addressModeV = addressMode;
    info.addressModeW = addressMode;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = 16.0f;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    if (vkCreateSampler(m_Device, &info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create sampler");
    return sampler;
}

} // namespace Leir
