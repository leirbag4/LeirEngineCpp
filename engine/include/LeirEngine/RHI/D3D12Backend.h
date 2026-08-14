#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include <memory>
#include <string>
#include <vector>

// D3D12 implementation of the RenderBackend interface.
//
// Same contract as VulkanBackend but translates to Direct3D 12 (DXGI swapchain,
// ID3D12Device, descriptor heaps, root signatures, PSOs). The root signature is
// derived from the RHI pipeline layout:
//   - push constants   -> root constants at register b1 (the .slang shaders pin
//                         the push constant cbuffer to b1 via register(b1))
//   - UniformBuffer set-> root descriptor CBV at register b0 (vertex stage)
//   - image sampler set-> descriptor tables: SRV t0 + sampler s0 (pixel stage)
// Vertex attribute semantics (RHIVertexAttribute::semantic) feed the D3D12
// input layout; they must match what Slang emits (POSITION/NORMAL/TEXCOORD...).
//
// The public header must not include D3D12 types; everything is behind a PIMPL.

namespace Leir {
namespace RHI {

class LEIR_API D3D12Backend : public RenderBackend {
public:
    // window: native GLFWwindow*.
    D3D12Backend(void* window, int width, int height, bool vsync,
                 const std::string& appName);
    ~D3D12Backend() override;

    // RenderBackend
    bool BeginFrame(bool skipRenderPass) override;
    void BeginSwapchainOverlay() override;
    void EndFrame() override;
    void WaitIdle() override;

    const GCaps& GetCaps() const override;

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
    uint32_t GetCopyRowPitchAlignment() const override { return 256; }

    RHIRenderPass CreateRenderPass(const std::vector<Format>& colorFormats,
        Format depthFormat, bool overlay) override;
    void DestroyRenderPass(RHIRenderPass renderPass) override;

    RHIPassTemplate CreatePassTemplate(const RHIPassTemplateDesc& desc) override;
    void DestroyPassTemplate(RHIPassTemplate passTemplate) override;

    RHIFramebuffer CreateFramebuffer(RHIRenderPass renderPass,
        uint32_t width, uint32_t height,
        const std::vector<RHIImageView>& attachments) override;
    void DestroyFramebuffer(RHIFramebuffer framebuffer) override;

    void CmdBeginRenderPass(RHICommandBuffer cmd, RHIPassTemplate passTemplate,
        RHIFramebuffer framebuffer) override;
    void CmdEndRenderPass(RHICommandBuffer cmd) override;

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

    const char* GetShaderFileExtension() const override { return ".dxil"; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace RHI
} // namespace Leir
