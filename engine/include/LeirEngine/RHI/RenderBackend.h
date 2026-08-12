#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RHI.h"

#include <cstdint>
#include <vector>
#include <string>

// Abstract render backend interface.
//
// This is the evolutive-minimum RHI contract. It groups the operations the
// engine currently performs on Vulkan (resource creation, frame lifecycle,
// command recording) into backend-neutral methods. A concrete backend
// (VulkanBackend today, D3D12 next) implements this interface and translates
// to its native API. See TODO_RHI_SLANG.md Fase 2a.
//
// Public engine/editor code must only depend on this interface + RHI.h types.

namespace Leir {
namespace RHI {

class LEIR_API RenderBackend {
public:
    virtual ~RenderBackend() = default;

    // ---- Frame lifecycle ----
    virtual bool BeginFrame(bool skipRenderPass) = 0;
    virtual void BeginSwapchainOverlay() = 0;
    virtual void EndFrame() = 0;
    virtual void WaitIdle() = 0;

    virtual RHICommandBuffer GetCurrentCommandBuffer() const = 0;
    virtual uint32_t GetCurrentFrameIndex() const = 0;
    virtual uint32_t GetSwapchainWidth() const = 0;
    virtual uint32_t GetSwapchainHeight() const = 0;
    virtual RHIRenderPass GetRenderPass() const = 0;
    virtual RHIRenderPass GetOverlayRenderPass() const = 0;

    virtual bool WasResized() const = 0;
    virtual void ResetResized() = 0;
    virtual void NotifyResized() = 0;
    virtual void RecreateSwapchain() = 0;

    // ---- Resource creation ----
    virtual RHIShaderModule CreateShaderModule(const std::vector<char>& code) = 0;
    virtual void DestroyShaderModule(RHIShaderModule module) = 0;

    virtual RHIPipeline CreateGraphicsPipeline(const RHIPipelineDesc& desc) = 0;
    virtual void DestroyPipeline(RHIPipeline pipeline) = 0;

    virtual RHIPipelineLayout CreatePipelineLayout(
        const std::vector<RHIDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<RHIPushConstantRange>& pushConstants = {}) = 0;
    virtual void DestroyPipelineLayout(RHIPipelineLayout layout) = 0;

    virtual RHIDescriptorSetLayout CreateDescriptorSetLayout(
        const std::vector<RHIDescriptorBinding>& bindings) = 0;
    virtual void DestroyDescriptorSetLayout(RHIDescriptorSetLayout layout) = 0;

    virtual RHIDescriptorPool CreateDescriptorPool(
        const std::vector<RHIDescriptorBinding>& poolBindings,
        uint32_t maxSets) = 0;
    virtual void DestroyDescriptorPool(RHIDescriptorPool pool) = 0;

    virtual RHIDescriptorSet AllocateDescriptorSet(
        RHIDescriptorPool pool, RHIDescriptorSetLayout layout) = 0;
    virtual void WriteDescriptorSets(const std::vector<RHIDescriptorWrite>& writes) = 0;

    virtual RHIBuffer CreateBuffer(uint32_t size, BufferUsage usage,
        MemoryProperty properties, RHIDeviceMemory& memory) = 0;
    virtual void DestroyBuffer(RHIBuffer buffer) = 0;
    virtual void DestroyMemory(RHIDeviceMemory memory) = 0;
    virtual void CopyBuffer(RHIBuffer src, RHIBuffer dst, uint32_t size) = 0;
    virtual bool MapMemory(RHIDeviceMemory memory, uint32_t offset,
        uint32_t size, void** data) = 0;
    virtual void UnmapMemory(RHIDeviceMemory memory) = 0;

    virtual RHIImage CreateImage(uint32_t width, uint32_t height, Format format,
        ImageUsage usage, MemoryProperty properties, RHIDeviceMemory& memory) = 0;
    virtual void DestroyImage(RHIImage image) = 0;

    virtual RHIImageView CreateImageView(RHIImage image, Format format, Aspect aspect) = 0;
    virtual void DestroyImageView(RHIImageView imageView) = 0;

    virtual RHISampler CreateSampler(Filter filter = Filter::Linear,
        SamplerAddressMode addressMode = SamplerAddressMode::Repeat) = 0;
    virtual void DestroySampler(RHISampler sampler) = 0;

    virtual void TransitionImageLayout(RHIImage image, Format format,
        ImageLayout oldLayout, ImageLayout newLayout) = 0;
    virtual void CopyBufferToImage(RHIBuffer buffer, RHIImage image,
        uint32_t width, uint32_t height) = 0;

    virtual RHIRenderPass CreateRenderPass(const std::vector<Format>& colorFormats,
        Format depthFormat, bool overlay) = 0;
    virtual void DestroyRenderPass(RHIRenderPass renderPass) = 0;

    virtual RHIFramebuffer CreateFramebuffer(RHIRenderPass renderPass,
        uint32_t width, uint32_t height,
        const std::vector<RHIImageView>& attachments) = 0;
    virtual void DestroyFramebuffer(RHIFramebuffer framebuffer) = 0;

    // ---- Command recording ----
    virtual void CmdBeginRenderPass(RHICommandBuffer cmd, RHIRenderPass renderPass,
        RHIFramebuffer framebuffer,
        const std::vector<RHIClearValue>& clearValues,
        uint32_t width, uint32_t height) = 0;
    virtual void CmdEndRenderPass(RHICommandBuffer cmd) = 0;

    virtual void CmdBindPipeline(RHICommandBuffer cmd, RHIPipeline pipeline) = 0;
    virtual void CmdBindDescriptorSets(RHICommandBuffer cmd, RHIPipelineLayout layout,
        uint32_t firstSet, const std::vector<RHIDescriptorSet>& sets) = 0;
    virtual void CmdBindVertexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) = 0;
    virtual void CmdBindIndexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) = 0;
    virtual void CmdDraw(RHICommandBuffer cmd, uint32_t vertexCount, uint32_t firstVertex) = 0;
    virtual void CmdDrawIndexed(RHICommandBuffer cmd, uint32_t indexCount,
        uint32_t instanceCount, uint32_t firstIndex) = 0;
    virtual void CmdPushConstants(RHICommandBuffer cmd, RHIPipelineLayout layout,
        ShaderStageMask stage, uint32_t offset, uint32_t size, const void* data) = 0;
    virtual void CmdSetViewport(RHICommandBuffer cmd, const RHIViewport& viewport) = 0;
    virtual void CmdSetScissor(RHICommandBuffer cmd, const RHIRect2D& scissor) = 0;

    // ---- Pipeline barrier (manual, for RT transitions inside a command) ----
    virtual void CmdBarrier(RHICommandBuffer cmd) = 0;
};

// Factory: create the default backend (Vulkan) for a window.
class LEIR_API BackendFactory {
public:
    // Creates the backend selected by the LEIR_BACKEND macro. Returns nullptr
    // on failure.
    static RenderBackend* Create(void* window, int width, int height,
        bool vsync, const std::string& appName);
    // Creates a specific backend (Vulkan).
    static RenderBackend* CreateVulkan(void* window, int width, int height,
        bool vsync, const std::string& appName);
    static void Destroy(RenderBackend* backend);
};

} // namespace RHI
} // namespace Leir
