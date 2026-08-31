#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include <memory>
#include <string>
#include <vector>

// Vulkan implementation of the RenderBackend interface.
//
// Wraps the existing VulkanDevice (which owns the swapchain, render passes,
// command buffers, sync objects) and translates RHI-neutral types/ops to
// Vulkan. The Vulkan logic itself is not rewritten — VulkanDevice stays the
// source of truth; this backend only adapts the RHI contract to it.
//
// The public header must not include Vulkan types; VulkanDevice is kept behind
// a PIMPL.

namespace Leir { class SwapchainTarget; }

namespace Leir {
namespace RHI {

class LEIR_API VulkanBackend : public RenderBackend {
public:
    // window: native GLFWwindow*.
    VulkanBackend(void* window, int width, int height, bool vsync,
                  const std::string& appName);
    ~VulkanBackend() override;

    // RenderBackend
    bool BeginFrame(bool skipRenderPass) override;
    void BeginSwapchainOverlay() override;
    void EndFrame() override;
    void WaitIdle() override;

    const GCaps& GetCaps() const override;
    const char* GetBackendName() const override { return "vulkan"; }

    RHICommandBuffer GetCurrentCommandBuffer() const override;
    uint32_t GetCurrentFrameIndex() const override;
    uint32_t GetSwapchainWidth() const override;
    uint32_t GetSwapchainHeight() const override;
    RHIRenderPass GetRenderPass() const override;
    RHIRenderPass GetOverlayRenderPass() const override;

    bool WasResized() const override;
    void ResetResized() override;
    void NotifyResized() override;
    void RecreateSwapchain() override;

    RHIShaderModule CreateShaderModule(const std::vector<char>& code) override;
    void DestroyShaderModule(RHIShaderModule module) override;

    RHIPipeline CreateGraphicsPipeline(const RHIPipelineDesc& desc) override;
    void DestroyPipeline(RHIPipeline pipeline) override;

    RHIPipelineLayout CreatePipelineLayout(
        const std::vector<RHIDescriptorSetLayout>& descriptorSetLayouts,
        const std::vector<RHIPushConstantRange>& pushConstants) override;
    void DestroyPipelineLayout(RHIPipelineLayout layout) override;

    RHIDescriptorSetLayout CreateDescriptorSetLayout(
        const std::vector<RHIDescriptorBinding>& bindings) override;
    void DestroyDescriptorSetLayout(RHIDescriptorSetLayout layout) override;

    RHIDescriptorPool CreateDescriptorPool(
        const std::vector<RHIDescriptorBinding>& poolBindings,
        uint32_t maxSets) override;
    void DestroyDescriptorPool(RHIDescriptorPool pool) override;

    RHIDescriptorSet AllocateDescriptorSet(
        RHIDescriptorPool pool, RHIDescriptorSetLayout layout) override;
    void WriteDescriptorSets(const std::vector<RHIDescriptorWrite>& writes) override;

    RHIBuffer CreateBuffer(uint32_t size, BufferUsage usage,
        MemoryProperty properties, RHIDeviceMemory& memory) override;
    void DestroyBuffer(RHIBuffer buffer) override;
    void DestroyMemory(RHIDeviceMemory memory) override;
    void CopyBuffer(RHIBuffer src, RHIBuffer dst, uint32_t size) override;
    bool MapMemory(RHIDeviceMemory memory, uint32_t offset,
        uint32_t size, void** data) override;
    void UnmapMemory(RHIDeviceMemory memory) override;

    RHIImage CreateImage(uint32_t width, uint32_t height, Format format,
        ImageUsage usage, MemoryProperty properties, RHIDeviceMemory& memory) override;
    void DestroyImage(RHIImage image) override;

    RHIImageView CreateImageView(RHIImage image, Format format, Aspect aspect) override;
    void DestroyImageView(RHIImageView imageView) override;

    RHISampler CreateSampler(Filter filter, SamplerAddressMode addressMode) override;
    void DestroySampler(RHISampler sampler) override;

    void TransitionImageLayout(RHIImage image, Format format,
        ImageLayout oldLayout, ImageLayout newLayout) override;
    void CopyBufferToImage(RHIBuffer buffer, RHIImage image,
        uint32_t width, uint32_t height) override;

    RHIRenderPass CreateRenderPass(const std::vector<Format>& colorFormats,
        Format depthFormat, bool overlay) override;
    void DestroyRenderPass(RHIRenderPass renderPass) override;

    RHIPassTemplate CreatePassTemplate(const RHIPassTemplateDesc& desc) override;
    void DestroyPassTemplate(RHIPassTemplate passTemplate) override;

    uint32_t RegisterBindlessTexture(const RHIDescriptorImageInfo& info) override;
    void UpdateBindlessTexture(uint32_t index, const RHIDescriptorImageInfo& info) override;
    void UnregisterBindlessTexture(uint32_t index) override;
    RHIDescriptorSet GetBindlessDescriptorSet() const override;
    uint32_t GetBindlessMaxTextures() const override;

    RHIFramebuffer CreateFramebuffer(RHIRenderPass renderPass,
        uint32_t width, uint32_t height,
        const std::vector<RHIImageView>& attachments) override;
    void DestroyFramebuffer(RHIFramebuffer framebuffer) override;

    void CmdBeginRenderPass(RHICommandBuffer cmd, RHIPassTemplate passTemplate,
        RHIFramebuffer framebuffer) override;
    void CmdEndRenderPass(RHICommandBuffer cmd) override;

    void CmdExecuteGraph(RHICommandBuffer cmd, const GCommandGraph& graph) override;

    void CmdBindPipeline(RHICommandBuffer cmd, RHIPipeline pipeline) override;
    void CmdBindDescriptorSets(RHICommandBuffer cmd, RHIPipelineLayout layout,
        uint32_t firstSet, const std::vector<RHIDescriptorSet>& sets) override;
    void CmdBindVertexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) override;
    void CmdBindIndexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) override;
    void CmdDraw(RHICommandBuffer cmd, uint32_t vertexCount, uint32_t firstVertex) override;
    void CmdDrawIndexed(RHICommandBuffer cmd, uint32_t indexCount,
        uint32_t instanceCount, uint32_t firstIndex) override;
    void CmdPushConstants(RHICommandBuffer cmd, RHIPipelineLayout layout,
        ShaderStageMask stage, uint32_t offset, uint32_t size, const void* data) override;
    void CmdSetViewport(RHICommandBuffer cmd, const RHIViewport& viewport) override;
    void CmdSetScissor(RHICommandBuffer cmd, const RHIRect2D& scissor) override;
    void CmdBarrier(RHICommandBuffer cmd) override;
    void CmdTransitionImageLayout(RHICommandBuffer cmd, RHIImage image,
        Format format, ImageLayout oldLayout, ImageLayout newLayout, Aspect aspect) override;

    // ---- Multi-window (external targets) ----
    // Creates a swapchain target for an additional GLFW window, sharing this
    // backend's Vulkan device/queues/render passes. The returned object is
    // self-contained for its own frame loop (BeginFrame/EndFrame).
    // window: native GLFWwindow*.
    Leir::SwapchainTarget* CreateSwapchainTarget(void* window) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace RHI
} // namespace Leir
