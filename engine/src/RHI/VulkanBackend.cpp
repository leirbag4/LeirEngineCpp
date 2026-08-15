#include "LeirEngine/RHI/VulkanBackend.h"
#if defined(_WIN32) && defined(_MSC_VER)
#include "LeirEngine/RHI/D3D12Backend.h"
#endif
#include "LeirEngine/Rendering/VulkanDevice.h"

#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "LeirEngine/Core/Log.h"

// Vulkan implementation of the RenderBackend interface.
//
// Handle mapping: the RHI Handle (uint64_t) stores the native Vulkan handle
// directly. Non-dispatchable Vulkan handles (VkBuffer, VkImage, VkSampler,
// VkPipeline, ...) are already uint64_t; dispatchable ones (VkDevice,
// VkCommandBuffer) are pointers cast to uint64_t. This keeps the translation
// a simple cast with no lookup tables.

namespace Leir {
namespace RHI {

namespace {

// ---- enum conversions ----

// Persistent render-pass state record (TODO_RHI_SLANG.md §3.1 GPassTemplate).
struct PassTemplateRec {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkClearValue> clears;
    VkViewport viewport{};
    VkRect2D scissor{};
    VkExtent2D renderArea{};
};

VkFormat ToVk(Format f) {
    switch (f) {
        case Format::R32G32B32_SFLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32_SFLOAT:       return VK_FORMAT_R32G32_SFLOAT;
        case Format::R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::R8G8B8A8_SRGB:       return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::B8G8R8A8_SRGB:       return VK_FORMAT_B8G8R8A8_SRGB;
        case Format::D32_SFLOAT:          return VK_FORMAT_D32_SFLOAT;
        case Format::R32_SFLOAT:          return VK_FORMAT_R32_SFLOAT;
    }
    return VK_FORMAT_UNDEFINED;
}

VkPrimitiveTopology ToVk(Topology t) {
    switch (t) {
        case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkPolygonMode ToVk(PolygonMode m) {
    (void)m;
    return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags ToVk(CullMode c) {
    switch (c) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    }
    return VK_CULL_MODE_NONE;
}

VkVertexInputRate ToVk(VertexInputRate r) {
    return r == VertexInputRate::Instance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                          : VK_VERTEX_INPUT_RATE_VERTEX;
}

VkDescriptorType ToVk(DescriptorType d) {
    switch (d) {
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

VkFilter ToVk(Filter f) {
    return f == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}

VkSamplerAddressMode ToVk(SamplerAddressMode a) {
    switch (a) {
        case SamplerAddressMode::Repeat:      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkBufferUsageFlags ToVk(BufferUsage u) {
    VkBufferUsageFlags flags = 0;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(BufferUsage::TransferSrc)) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(BufferUsage::TransferDst)) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(BufferUsage::Vertex))       flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(BufferUsage::Index))        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(BufferUsage::Uniform))      flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return flags;
}

VkMemoryPropertyFlags ToVk(MemoryProperty p) {
    VkMemoryPropertyFlags flags = 0;
    if (static_cast<uint8_t>(p) & static_cast<uint8_t>(MemoryProperty::DeviceLocal)) flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (static_cast<uint8_t>(p) & static_cast<uint8_t>(MemoryProperty::HostVisible)) flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (static_cast<uint8_t>(p) & static_cast<uint8_t>(MemoryProperty::HostCoherent)) flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return flags;
}

VkImageUsageFlags ToVk(ImageUsage u) {
    VkImageUsageFlags flags = 0;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(ImageUsage::TransferDst))          flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(ImageUsage::TransferSrc))          flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(ImageUsage::Sampled))              flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(ImageUsage::ColorAttachment))      flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (static_cast<uint8_t>(u) & static_cast<uint8_t>(ImageUsage::DepthStencilAttachment)) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    return flags;
}

VkImageAspectFlags ToVk(Aspect a) {
    VkImageAspectFlags flags = 0;
    if (static_cast<uint8_t>(a) & static_cast<uint8_t>(Aspect::Color)) flags |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (static_cast<uint8_t>(a) & static_cast<uint8_t>(Aspect::Depth)) flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    return flags;
}

VkShaderStageFlagBits ToVk(ShaderStage s) {
    return s == ShaderStage::Fragment ? VK_SHADER_STAGE_FRAGMENT_BIT
                                      : VK_SHADER_STAGE_VERTEX_BIT;
}

VkShaderStageFlags ToVk(ShaderStageMask m) {
    VkShaderStageFlags flags = 0;
    if (static_cast<uint8_t>(m) & static_cast<uint8_t>(ShaderStageMask::Vertex))   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<uint8_t>(m) & static_cast<uint8_t>(ShaderStageMask::Fragment)) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    return flags;
}

VkImageLayout ToVk(ImageLayout l) {
    switch (l) {
        case ImageLayout::Undefined:             return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::TransferDst:           return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ImageLayout::ShaderReadOnly:        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::ColorAttachment:       return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkAttachmentLoadOp ToVk(LoadOp op) {
    switch (op) {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp ToVk(StoreOp op) {
    return op == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE
                                : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkDescriptorSetLayoutBinding ToVk(const RHIDescriptorBinding& b) {
    VkDescriptorSetLayoutBinding out{};
    out.binding = b.binding;
    out.descriptorType = ToVk(b.type);
    out.descriptorCount = b.count;
    out.stageFlags = ToVk(b.stage);
    return out;
}

VkVertexInputBindingDescription ToVk(const RHIVertexInputBinding& b) {
    VkVertexInputBindingDescription out{};
    out.binding = b.binding;
    out.stride = b.stride;
    out.inputRate = ToVk(b.inputRate);
    return out;
}

VkVertexInputAttributeDescription ToVk(const RHIVertexAttribute& a) {
    VkVertexInputAttributeDescription out{};
    out.location = a.location;
    out.binding = a.binding;
    out.format = ToVk(a.format);
    out.offset = a.offset;
    return out;
}

} // namespace

struct VulkanBackend::Impl {
    VulkanDevice device;
    GCaps caps;
    Impl(void* window, int width, int height, bool vsync, const std::string& appName)
        : device(static_cast<GLFWwindow*>(window),
                 VulkanDeviceConfig{
                     appName, VK_MAKE_VERSION(0, 1, 0), width, height,
                     true, vsync })
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device.GetPhysicalDevice(), &props);

        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceFeatures(device.GetPhysicalDevice(), &features);

        // Feature query for descriptor indexing (bindless).
        bool descriptorIndexing = false;
        if (device.GetDevice()) {
            VkPhysicalDeviceDescriptorIndexingFeatures dif{};
            dif.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            VkPhysicalDeviceFeatures2 f2{};
            f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            f2.pNext = &dif;
            vkGetPhysicalDeviceFeatures2(device.GetPhysicalDevice(), &f2);
            descriptorIndexing =
                dif.descriptorBindingPartiallyBound && dif.shaderSampledImageArrayNonUniformIndexing &&
                dif.runtimeDescriptorArray;
        }

        const auto& lim = props.limits;
        caps.maxTexturesPerTable = lim.maxPerStageDescriptorSampledImages;
        caps.maxUniformBuffersPerTable = lim.maxPerStageDescriptorUniformBuffers;
        caps.maxSamplersPerTable = lim.maxPerStageDescriptorSamplers;
        caps.maxStorageBuffersPerTable = lim.maxPerStageDescriptorStorageBuffers;
        caps.maxPushConstantsSize = lim.maxPushConstantsSize;
        caps.maxColorAttachments = lim.maxColorAttachments;
        caps.maxTextureSize = lim.maxImageDimension2D;
        caps.minUniformBufferOffsetAlignment = (uint32_t)lim.minUniformBufferOffsetAlignment;
        caps.bindless = descriptorIndexing;
        caps.multiRenderTarget = lim.maxColorAttachments >= 2;
        caps.instancing = true;
        caps.compute = true; // compute is core Vulkan since 1.0
        caps.storageBuffers = true;
        caps.sRGB = true;
        caps.wireframe = features.fillModeNonSolid;
        caps.anisotropicFiltering = features.samplerAnisotropy;

        InitBindless();
    }

    // ---- Bindless texture table (descriptor indexing) ----
    uint32_t bindlessCount = 0;
    uint32_t bindlessNext = 0;
    std::vector<uint32_t> bindlessFree;
    VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE;
    VkDescriptorPool bindlessPool = VK_NULL_HANDLE;
    VkDescriptorSet bindlessSet = VK_NULL_HANDLE;

    void InitBindless() {
        if (!caps.bindless) return;
        bindlessCount = caps.maxTexturesPerTable;
        bool uab = device.IsDescriptorIndexingUpdateAfterBind();
        if (uab) {
            // The non-update-after-bind limits are too small for a bindless
            // table (samplers=64 / resources=200 on this iGPU) and fail set
            // layout / pipeline creation. Use the update-after-bind limits.
            VkPhysicalDeviceProperties2 props2{};
            props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            VkPhysicalDeviceDescriptorIndexingProperties dip{};
            dip.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
            props2.pNext = &dip;
            vkGetPhysicalDeviceProperties2(device.GetPhysicalDevice(), &props2);
            uint32_t img = dip.maxPerStageDescriptorUpdateAfterBindSampledImages;
            uint32_t sam = dip.maxPerStageDescriptorUpdateAfterBindSamplers;
            bindlessCount = std::min({
                img, sam, dip.maxPerStageUpdateAfterBindResources,
                dip.maxDescriptorSetUpdateAfterBindSampledImages,
                dip.maxDescriptorSetUpdateAfterBindSamplers
            });
        }
        if (bindlessCount == 0) return;

        XConsole::Println("Vulkan bindless table: {} textures ({})",
            bindlessCount, uab ? "update-after-bind" : "static limits");

        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = bindlessCount;
        b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindlessLayout = device.CreateBindlessDescriptorSetLayout({b});

        VkDescriptorPoolSize ps{};
        ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = bindlessCount;
        VkDescriptorPoolCreateFlags poolFlags = 0;
        if (uab) poolFlags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        bindlessPool = device.CreateDescriptorPool({ps}, 1, poolFlags);

        VkDescriptorSetLayout layouts[] = { bindlessLayout };
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = bindlessPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = layouts;
        if (vkAllocateDescriptorSets(device.GetDevice(), &ai, &bindlessSet) != VK_SUCCESS)
            XConsole::PrintError("VulkanBackend: failed to allocate bindless descriptor set");
    }

    uint32_t NextBindlessIndex() {
        if (!bindlessFree.empty()) {
            uint32_t i = bindlessFree.back();
            bindlessFree.pop_back();
            return i;
        }
        if (bindlessNext < bindlessCount) return bindlessNext++;
        XConsole::PrintError("VulkanBackend: bindless texture table full ({})", bindlessCount);
        return 0;
    }

    ~Impl() {
        if (bindlessPool)
            vkDestroyDescriptorPool(device.GetDevice(), bindlessPool, nullptr);
        if (bindlessLayout)
            vkDestroyDescriptorSetLayout(device.GetDevice(), bindlessLayout, nullptr);
    }
};

VulkanBackend::VulkanBackend(void* window, int width, int height, bool vsync,
                             const std::string& appName)
    : m_Impl(std::make_unique<Impl>(window, width, height, vsync, appName))
{
}

VulkanBackend::~VulkanBackend() = default;

const GCaps& VulkanBackend::GetCaps() const { return m_Impl->caps; }

// ---- Frame lifecycle ----

bool VulkanBackend::BeginFrame(bool skipRenderPass) { return m_Impl->device.BeginFrame(skipRenderPass); }
void VulkanBackend::BeginSwapchainOverlay() { m_Impl->device.BeginSwapchainOverlay(); }
void VulkanBackend::EndFrame() { m_Impl->device.EndFrame(); }
void VulkanBackend::WaitIdle() { vkDeviceWaitIdle(m_Impl->device.GetDevice()); }

RHICommandBuffer VulkanBackend::GetCurrentCommandBuffer() const {
    RHICommandBuffer cb;
    cb.handle = reinterpret_cast<uint64_t>(m_Impl->device.GetCurrentCommandBuffer());
    return cb;
}
uint32_t VulkanBackend::GetCurrentFrameIndex() const { return m_Impl->device.GetCurrentFrameIndex(); }
uint32_t VulkanBackend::GetSwapchainWidth() const { return m_Impl->device.GetSwapchainExtent().width; }
uint32_t VulkanBackend::GetSwapchainHeight() const { return m_Impl->device.GetSwapchainExtent().height; }
RHIRenderPass VulkanBackend::GetRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->device.GetRenderPass());
    return rp;
}
RHIRenderPass VulkanBackend::GetOverlayRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->device.GetOverlayRenderPass());
    return rp;
}

bool VulkanBackend::WasResized() const { return m_Impl->device.WasResized(); }
void VulkanBackend::ResetResized() { m_Impl->device.ResetResized(); }
void VulkanBackend::NotifyResized() { m_Impl->device.NotifyResized(); }
void VulkanBackend::RecreateSwapchain() { m_Impl->device.RecreateSwapchain(); }

// ---- Resource creation ----

RHIShaderModule VulkanBackend::CreateShaderModule(const std::vector<char>& code) {
    RHIShaderModule m;
    m.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateShaderModule(code));
    return m;
}
void VulkanBackend::DestroyShaderModule(RHIShaderModule module) {
    vkDestroyShaderModule(m_Impl->device.GetDevice(), reinterpret_cast<VkShaderModule>(module.handle), nullptr);
}

RHIPipeline VulkanBackend::CreateGraphicsPipeline(const RHIPipelineDesc& desc) {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(desc.stages.size());
    for (const auto& s : desc.stages) {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = ToVk(s.stage);
        info.module = reinterpret_cast<VkShaderModule>(s.module.handle);
        info.pName = s.entryPoint;
        stages.push_back(info);
    }
    RHIPipeline p;
    p.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateGraphicsPipeline(
        reinterpret_cast<VkPipelineLayout>(desc.layout.handle),
        reinterpret_cast<VkRenderPass>(desc.renderPass.handle),
        stages,
        ToVk(desc.vertexBinding),
        [&]() {
            std::vector<VkVertexInputAttributeDescription> v;
            v.reserve(desc.vertexAttributes.size());
            for (const auto& a : desc.vertexAttributes) v.push_back(ToVk(a));
            return v;
        }(),
        ToVk(desc.topology),
        ToVk(desc.polygonMode),
        ToVk(desc.cullMode),
        desc.depthTestEnable,
        desc.blend.enable));
    return p;
}
void VulkanBackend::DestroyPipeline(RHIPipeline pipeline) {
    vkDestroyPipeline(m_Impl->device.GetDevice(), reinterpret_cast<VkPipeline>(pipeline.handle), nullptr);
}

RHIPipelineLayout VulkanBackend::CreatePipelineLayout(
    const std::vector<RHIDescriptorSetLayout>& layouts,
    const std::vector<RHIPushConstantRange>& pushConstants) {
    std::vector<VkDescriptorSetLayout> vkLayouts;
    vkLayouts.reserve(layouts.size());
    for (const auto& l : layouts)
        vkLayouts.push_back(reinterpret_cast<VkDescriptorSetLayout>(l.handle));

    std::vector<VkPushConstantRange> vkRanges;
    vkRanges.reserve(pushConstants.size());
    for (const auto& r : pushConstants) {
        VkPushConstantRange pr{};
        pr.stageFlags = ToVk(r.stage);
        pr.offset = r.offset;
        pr.size = r.size;
        vkRanges.push_back(pr);
    }

    RHIPipelineLayout out;
    out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreatePipelineLayout(vkLayouts, vkRanges));
    return out;
}
void VulkanBackend::DestroyPipelineLayout(RHIPipelineLayout layout) {
    vkDestroyPipelineLayout(m_Impl->device.GetDevice(), reinterpret_cast<VkPipelineLayout>(layout.handle), nullptr);
}

RHIDescriptorSetLayout VulkanBackend::CreateDescriptorSetLayout(
    const std::vector<RHIDescriptorBinding>& bindings) {
    bool anyBindless = false;
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());
    for (const auto& b : bindings) {
        vkBindings.push_back(ToVk(b));
        if (b.bindless) {
            anyBindless = true;
            vkBindings.back().descriptorCount = m_Impl->bindlessCount;
        }
    }
    RHIDescriptorSetLayout out;
    if (anyBindless)
        out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateBindlessDescriptorSetLayout(vkBindings));
    else
        out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateDescriptorSetLayout(vkBindings));
    return out;
}
void VulkanBackend::DestroyDescriptorSetLayout(RHIDescriptorSetLayout layout) {
    vkDestroyDescriptorSetLayout(m_Impl->device.GetDevice(), reinterpret_cast<VkDescriptorSetLayout>(layout.handle), nullptr);
}

RHIDescriptorPool VulkanBackend::CreateDescriptorPool(
    const std::vector<RHIDescriptorBinding>& poolBindings, uint32_t maxSets) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(poolBindings.size());
    for (const auto& b : poolBindings) {
        VkDescriptorPoolSize ps{};
        ps.type = ToVk(b.type);
        ps.descriptorCount = b.count;
        poolSizes.push_back(ps);
    }
    RHIDescriptorPool out;
    out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateDescriptorPool(poolSizes, maxSets));
    return out;
}
void VulkanBackend::DestroyDescriptorPool(RHIDescriptorPool pool) {
    vkDestroyDescriptorPool(m_Impl->device.GetDevice(), reinterpret_cast<VkDescriptorPool>(pool.handle), nullptr);
}

RHIDescriptorSet VulkanBackend::AllocateDescriptorSet(
    RHIDescriptorPool pool, RHIDescriptorSetLayout layout) {
    VkDescriptorSetLayout vkLayout = reinterpret_cast<VkDescriptorSetLayout>(layout.handle);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = reinterpret_cast<VkDescriptorPool>(pool.handle);
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &vkLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(m_Impl->device.GetDevice(), &allocInfo, &set) != VK_SUCCESS) {
        XConsole::PrintError("VulkanBackend: failed to allocate descriptor set");
    }
    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(set);
    return out;
}

void VulkanBackend::WriteDescriptorSets(const std::vector<RHIDescriptorWrite>& writes) {
    std::vector<VkWriteDescriptorSet> vkWrites;
    std::vector<VkDescriptorImageInfo> imgInfos;
    std::vector<VkDescriptorBufferInfo> bufInfos;
    vkWrites.reserve(writes.size());
    imgInfos.reserve(writes.size());
    bufInfos.reserve(writes.size());
    for (const auto& w : writes) {
        VkWriteDescriptorSet vw{};
        vw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vw.dstSet = reinterpret_cast<VkDescriptorSet>(w.dstSet.handle);
        vw.dstBinding = w.dstBinding;
        vw.dstArrayElement = w.dstArrayElement;
        vw.descriptorCount = w.count;
        vw.descriptorType = ToVk(w.type);
        if (w.type == DescriptorType::CombinedImageSampler && w.imageInfo.valid) {
            VkDescriptorImageInfo ii{};
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ii.imageView = reinterpret_cast<VkImageView>(w.imageInfo.imageView.handle);
            ii.sampler = reinterpret_cast<VkSampler>(w.imageInfo.sampler.handle);
            imgInfos.push_back(ii);
            vw.pImageInfo = &imgInfos.back();
        } else if (w.type == DescriptorType::UniformBuffer && w.bufferInfo.valid) {
            VkDescriptorBufferInfo bi{};
            bi.buffer = reinterpret_cast<VkBuffer>(w.bufferInfo.buffer.handle);
            bi.offset = w.bufferInfo.offset;
            bi.range = w.bufferInfo.range;
            bufInfos.push_back(bi);
            vw.pBufferInfo = &bufInfos.back();
        }
        vkWrites.push_back(vw);
    }
    if (!vkWrites.empty())
        vkUpdateDescriptorSets(m_Impl->device.GetDevice(),
            (uint32_t)vkWrites.size(), vkWrites.data(), 0, nullptr);
}

uint32_t VulkanBackend::RegisterBindlessTexture(const RHIDescriptorImageInfo& info) {
    auto& impl = *m_Impl;
    if (!impl.bindlessSet) {
        XConsole::PrintError("VulkanBackend: bindless not supported");
        return 0;
    }
    uint32_t index = impl.NextBindlessIndex();
    UpdateBindlessTexture(index, info);
    return index;
}

void VulkanBackend::UpdateBindlessTexture(uint32_t index, const RHIDescriptorImageInfo& info) {
    auto& impl = *m_Impl;
    if (!impl.bindlessSet) return;
    RHIDescriptorWrite w;
    w.dstSet = GetBindlessDescriptorSet();
    w.dstBinding = 0;
    w.dstArrayElement = index;
    w.count = 1;
    w.type = DescriptorType::CombinedImageSampler;
    w.imageInfo = info;
    WriteDescriptorSets({w});
}

void VulkanBackend::UnregisterBindlessTexture(uint32_t index) {
    auto& impl = *m_Impl;
    if (!impl.bindlessSet || index >= impl.bindlessCount) return;
    impl.bindlessFree.push_back(index);
}

RHIDescriptorSet VulkanBackend::GetBindlessDescriptorSet() const {
    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(m_Impl->bindlessSet);
    return out;
}

uint32_t VulkanBackend::GetBindlessMaxTextures() const { return m_Impl->bindlessCount; }

RHIBuffer VulkanBackend::CreateBuffer(uint32_t size, BufferUsage usage,
    MemoryProperty properties, RHIDeviceMemory& memory) {
    VkBuffer buf = m_Impl->device.CreateBuffer(size, ToVk(usage), ToVk(properties),
        reinterpret_cast<VkDeviceMemory&>(memory.handle));
    RHIBuffer out;
    out.handle = reinterpret_cast<uint64_t>(buf);
    return out;
}
void VulkanBackend::DestroyBuffer(RHIBuffer buffer) {
    vkDestroyBuffer(m_Impl->device.GetDevice(), reinterpret_cast<VkBuffer>(buffer.handle), nullptr);
}
void VulkanBackend::DestroyMemory(RHIDeviceMemory memory) {
    vkFreeMemory(m_Impl->device.GetDevice(), reinterpret_cast<VkDeviceMemory>(memory.handle), nullptr);
}
void VulkanBackend::CopyBuffer(RHIBuffer src, RHIBuffer dst, uint32_t size) {
    m_Impl->device.CopyBuffer(reinterpret_cast<VkBuffer>(src.handle),
        reinterpret_cast<VkBuffer>(dst.handle), size);
}
bool VulkanBackend::MapMemory(RHIDeviceMemory memory, uint32_t offset,
    uint32_t size, void** data) {
    return vkMapMemory(m_Impl->device.GetDevice(),
        reinterpret_cast<VkDeviceMemory>(memory.handle), offset, size, 0, data) == VK_SUCCESS;
}
void VulkanBackend::UnmapMemory(RHIDeviceMemory memory) {
    vkUnmapMemory(m_Impl->device.GetDevice(), reinterpret_cast<VkDeviceMemory>(memory.handle));
}

RHIImage VulkanBackend::CreateImage(uint32_t width, uint32_t height, Format format,
    ImageUsage usage, MemoryProperty properties, RHIDeviceMemory& memory) {
    VkImage image = VK_NULL_HANDLE;
    m_Impl->device.CreateImage(width, height, ToVk(format), VK_IMAGE_TILING_OPTIMAL,
        ToVk(usage), ToVk(properties), image,
        reinterpret_cast<VkDeviceMemory&>(memory.handle));
    RHIImage out;
    out.handle = reinterpret_cast<uint64_t>(image);
    return out;
}
void VulkanBackend::DestroyImage(RHIImage image) {
    vkDestroyImage(m_Impl->device.GetDevice(), reinterpret_cast<VkImage>(image.handle), nullptr);
}

RHIImageView VulkanBackend::CreateImageView(RHIImage image, Format format, Aspect aspect) {
    RHIImageView out;
    out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateImageView(
        reinterpret_cast<VkImage>(image.handle), ToVk(format), ToVk(aspect)));
    return out;
}
void VulkanBackend::DestroyImageView(RHIImageView imageView) {
    vkDestroyImageView(m_Impl->device.GetDevice(), reinterpret_cast<VkImageView>(imageView.handle), nullptr);
}

RHISampler VulkanBackend::CreateSampler(Filter filter, SamplerAddressMode addressMode) {
    RHISampler out;
    out.handle = reinterpret_cast<uint64_t>(m_Impl->device.CreateSampler(ToVk(filter), ToVk(addressMode)));
    return out;
}
void VulkanBackend::DestroySampler(RHISampler sampler) {
    vkDestroySampler(m_Impl->device.GetDevice(), reinterpret_cast<VkSampler>(sampler.handle), nullptr);
}

void VulkanBackend::TransitionImageLayout(RHIImage image, Format format,
    ImageLayout oldLayout, ImageLayout newLayout) {
    VkDevice dev = m_Impl->device.GetDevice();
    VkCommandPool pool = m_Impl->device.GetCommandPool();
    VkQueue queue = m_Impl->device.GetGraphicsQueue();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dev, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = ToVk(oldLayout);
    barrier.newLayout = ToVk(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = reinterpret_cast<VkImage>(image.handle);
    VkImageAspectFlags aspect = (newLayout == ImageLayout::DepthStencilAttachment)
        ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == ImageLayout::Undefined &&
        newLayout == ImageLayout::TransferDst) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == ImageLayout::TransferDst &&
               newLayout == ImageLayout::ShaderReadOnly) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == ImageLayout::ShaderReadOnly &&
               newLayout == ImageLayout::TransferDst) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == ImageLayout::Undefined &&
               newLayout == ImageLayout::DepthStencilAttachment) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT |
                                VK_ACCESS_MEMORY_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(dev, pool, 1, &cmd);
}

void VulkanBackend::CopyBufferToImage(RHIBuffer buffer, RHIImage image,
    uint32_t width, uint32_t height) {
    // The existing VulkanDevice has no CopyBufferToImage; do it inline.
    VkDevice dev = m_Impl->device.GetDevice();
    VkCommandPool pool = m_Impl->device.GetCommandPool();
    VkQueue queue = m_Impl->device.GetGraphicsQueue();

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dev, &allocInfo, &cmd);

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
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, reinterpret_cast<VkBuffer>(buffer.handle),
        reinterpret_cast<VkImage>(image.handle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(dev, pool, 1, &cmd);
}

RHIRenderPass VulkanBackend::CreateRenderPass(const std::vector<Format>& colorFormats,
    Format depthFormat, bool overlay) {
    VkDevice dev = m_Impl->device.GetDevice();

    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs;
    VkAttachmentReference depthRef{};
    bool hasDepth = depthFormat == Format::D32_SFLOAT;

    for (size_t i = 0; i < colorFormats.size(); ++i) {
        VkAttachmentDescription c{};
        c.format = ToVk(colorFormats[i]);
        c.samples = VK_SAMPLE_COUNT_1_BIT;
        c.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        c.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        c.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        c.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        c.initialLayout = overlay ? VK_IMAGE_LAYOUT_UNDEFINED
                                  : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        c.finalLayout = overlay ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(c);
        VkAttachmentReference ref{};
        ref.attachment = (uint32_t)i;
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRefs.push_back(ref);
    }

    if (hasDepth) {
        VkAttachmentDescription d{};
        d.format = ToVk(depthFormat);
        d.samples = VK_SAMPLE_COUNT_1_BIT;
        d.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        d.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        d.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        d.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthRef.attachment = (uint32_t)attachments.size();
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(d);
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = (uint32_t)colorRefs.size();
    subpass.pColorAttachments = colorRefs.data();
    if (hasDepth) subpass.pDepthStencilAttachment = &depthRef;

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

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (uint32_t)attachments.size();
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(dev, &rpInfo, nullptr, &rp) != VK_SUCCESS)
        throw std::runtime_error("VulkanBackend: failed to create render pass");

    RHIRenderPass out;
    out.handle = reinterpret_cast<uint64_t>(rp);
    return out;
}
void VulkanBackend::DestroyRenderPass(RHIRenderPass renderPass) {
    vkDestroyRenderPass(m_Impl->device.GetDevice(), reinterpret_cast<VkRenderPass>(renderPass.handle), nullptr);
}

RHIPassTemplate VulkanBackend::CreatePassTemplate(const RHIPassTemplateDesc& desc) {
    PassTemplateRec* rec = new PassTemplateRec();
    rec->renderPass = reinterpret_cast<VkRenderPass>(desc.renderPass.handle);

    rec->clears.reserve(desc.clearValues.size());
    for (const auto& c : desc.clearValues) {
        VkClearValue vc{};
        if (c.isDepth) {
            vc.depthStencil = { c.depth, c.stencil };
        } else {
            vc.color = { {c.color.x, c.color.y, c.color.z, c.color.w} };
        }
        rec->clears.push_back(vc);
    }

    // Viewport is in logical units; flip Y once here: GLM/NDC (y-up) → Vulkan
    // framebuffer (y-down). Matches the previous per-frame CmdBeginRenderPass.
    rec->viewport.x = desc.viewport.x;
    rec->viewport.y = desc.viewport.y + desc.viewport.height;
    rec->viewport.width = desc.viewport.width;
    rec->viewport.height = -desc.viewport.height;
    rec->viewport.minDepth = desc.viewport.minDepth;
    rec->viewport.maxDepth = desc.viewport.maxDepth;

    rec->scissor.offset = { (int32_t)desc.scissor.x, (int32_t)desc.scissor.y };
    rec->scissor.extent = { desc.scissor.width, desc.scissor.height };
    rec->renderArea = rec->scissor.extent;

    RHIPassTemplate out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void VulkanBackend::DestroyPassTemplate(RHIPassTemplate passTemplate) {
    delete reinterpret_cast<PassTemplateRec*>(passTemplate.handle);
}

RHIFramebuffer VulkanBackend::CreateFramebuffer(RHIRenderPass renderPass,
    uint32_t width, uint32_t height, const std::vector<RHIImageView>& attachments) {
    std::vector<VkImageView> views;
    views.reserve(attachments.size());
    for (const auto& a : attachments)
        views.push_back(reinterpret_cast<VkImageView>(a.handle));

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = reinterpret_cast<VkRenderPass>(renderPass.handle);
    fbInfo.attachmentCount = (uint32_t)views.size();
    fbInfo.pAttachments = views.data();
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;

    VkFramebuffer fb = VK_NULL_HANDLE;
    if (vkCreateFramebuffer(m_Impl->device.GetDevice(), &fbInfo, nullptr, &fb) != VK_SUCCESS)
        throw std::runtime_error("VulkanBackend: failed to create framebuffer");

    RHIFramebuffer out;
    out.handle = reinterpret_cast<uint64_t>(fb);
    return out;
}
void VulkanBackend::DestroyFramebuffer(RHIFramebuffer framebuffer) {
    vkDestroyFramebuffer(m_Impl->device.GetDevice(), reinterpret_cast<VkFramebuffer>(framebuffer.handle), nullptr);
}

// ---- Command recording ----

void VulkanBackend::CmdBeginRenderPass(RHICommandBuffer cmd, RHIPassTemplate passTemplate,
    RHIFramebuffer framebuffer) {
    VkCommandBuffer vkCmd = reinterpret_cast<VkCommandBuffer>(cmd.handle);
    PassTemplateRec* rec = reinterpret_cast<PassTemplateRec*>(passTemplate.handle);

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = rec->renderPass;
    rpInfo.framebuffer = reinterpret_cast<VkFramebuffer>(framebuffer.handle);
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = rec->renderArea;
    rpInfo.clearValueCount = (uint32_t)rec->clears.size();
    rpInfo.pClearValues = rec->clears.empty() ? nullptr : rec->clears.data();
    vkCmdBeginRenderPass(vkCmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(vkCmd, 0, 1, &rec->viewport);
    vkCmdSetScissor(vkCmd, 0, 1, &rec->scissor);
}
void VulkanBackend::CmdEndRenderPass(RHICommandBuffer cmd) {
    vkCmdEndRenderPass(reinterpret_cast<VkCommandBuffer>(cmd.handle));
}

void VulkanBackend::CmdBindPipeline(RHICommandBuffer cmd, RHIPipeline pipeline) {
    vkCmdBindPipeline(reinterpret_cast<VkCommandBuffer>(cmd.handle),
        VK_PIPELINE_BIND_POINT_GRAPHICS, reinterpret_cast<VkPipeline>(pipeline.handle));
}
void VulkanBackend::CmdBindDescriptorSets(RHICommandBuffer cmd, RHIPipelineLayout layout,
    uint32_t firstSet, const std::vector<RHIDescriptorSet>& sets) {
    std::vector<VkDescriptorSet> vkSets;
    vkSets.reserve(sets.size());
    for (const auto& s : sets) vkSets.push_back(reinterpret_cast<VkDescriptorSet>(s.handle));
    vkCmdBindDescriptorSets(reinterpret_cast<VkCommandBuffer>(cmd.handle),
        VK_PIPELINE_BIND_POINT_GRAPHICS, reinterpret_cast<VkPipelineLayout>(layout.handle),
        firstSet, (uint32_t)vkSets.size(), vkSets.data(), 0, nullptr);
}
void VulkanBackend::CmdBindVertexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    VkBuffer vb[] = { reinterpret_cast<VkBuffer>(buffer.handle) };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(reinterpret_cast<VkCommandBuffer>(cmd.handle), 0, 1, vb, offsets);
}
void VulkanBackend::CmdBindIndexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    vkCmdBindIndexBuffer(reinterpret_cast<VkCommandBuffer>(cmd.handle),
        reinterpret_cast<VkBuffer>(buffer.handle), 0, VK_INDEX_TYPE_UINT32);
}
void VulkanBackend::CmdDraw(RHICommandBuffer cmd, uint32_t vertexCount, uint32_t firstVertex) {
    vkCmdDraw(reinterpret_cast<VkCommandBuffer>(cmd.handle), vertexCount, 1, firstVertex, 0);
}
void VulkanBackend::CmdDrawIndexed(RHICommandBuffer cmd, uint32_t indexCount,
    uint32_t instanceCount, uint32_t firstIndex) {
    vkCmdDrawIndexed(reinterpret_cast<VkCommandBuffer>(cmd.handle), indexCount,
        instanceCount, firstIndex, 0, 0);
}
void VulkanBackend::CmdPushConstants(RHICommandBuffer cmd, RHIPipelineLayout layout,
    ShaderStageMask stage, uint32_t offset, uint32_t size, const void* data) {
    vkCmdPushConstants(reinterpret_cast<VkCommandBuffer>(cmd.handle),
        reinterpret_cast<VkPipelineLayout>(layout.handle), ToVk(stage), offset, size, data);
}
void VulkanBackend::CmdSetViewport(RHICommandBuffer cmd, const RHIViewport& viewport) {
    VkViewport vp{};
    vp.x = viewport.x;
    vp.y = viewport.y;
    vp.width = viewport.width;
    vp.height = viewport.height;
    vp.minDepth = viewport.minDepth;
    vp.maxDepth = viewport.maxDepth;
    vkCmdSetViewport(reinterpret_cast<VkCommandBuffer>(cmd.handle), 0, 1, &vp);
}
void VulkanBackend::CmdSetScissor(RHICommandBuffer cmd, const RHIRect2D& scissor) {
    VkRect2D s{};
    s.offset = { scissor.x, scissor.y };
    s.extent = { scissor.width, scissor.height };
    vkCmdSetScissor(reinterpret_cast<VkCommandBuffer>(cmd.handle), 0, 1, &s);
}
void VulkanBackend::CmdBarrier(RHICommandBuffer cmd) {
    (void)cmd;
}

void VulkanBackend::CmdTransitionImageLayout(RHICommandBuffer cmd, RHIImage image,
    Format format, ImageLayout oldLayout, ImageLayout newLayout, Aspect aspect) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = ToVk(oldLayout);
    barrier.newLayout = ToVk(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = reinterpret_cast<VkImage>(image.handle);
    barrier.subresourceRange.aspectMask = ToVk(aspect);
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == ImageLayout::Undefined &&
        newLayout == ImageLayout::ColorAttachment) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == ImageLayout::ColorAttachment &&
               newLayout == ImageLayout::ShaderReadOnly) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(reinterpret_cast<VkCommandBuffer>(cmd.handle),
        srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    (void)format;
}

// ---- Factory ----

RenderBackend* BackendFactory::Create(const std::string& backend,
    void* window, int width, int height, bool vsync, const std::string& appName) {
    if (backend == "d3d12")
        return CreateD3D12(window, width, height, vsync, appName);
    if (backend == "vulkan")
        return CreateVulkan(window, width, height, vsync, appName);
#if LEIR_BACKEND == LEIR_BACKEND_D3D12
    return CreateD3D12(window, width, height, vsync, appName);
#else
    return CreateVulkan(window, width, height, vsync, appName);
#endif
}

RenderBackend* BackendFactory::CreateVulkan(void* window, int width, int height,
    bool vsync, const std::string& appName) {
    return new VulkanBackend(window, width, height, vsync, appName);
}

RenderBackend* BackendFactory::CreateD3D12(void* window, int width, int height,
    bool vsync, const std::string& appName) {
#if defined(_WIN32) && defined(_MSC_VER)
    return new D3D12Backend(window, width, height, vsync, appName);
#else
    // D3D12 backend requires MSVC (MinGW CI builds Vulkan only).
    (void)window; (void)width; (void)height; (void)vsync; (void)appName;
    return nullptr;
#endif
}

void BackendFactory::Destroy(RenderBackend* backend) {
    delete backend;
}

} // namespace RHI
} // namespace Leir
