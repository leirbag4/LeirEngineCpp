#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include <memory>
#include <string>
#include <vector>

// WebGPU implementation of the RenderBackend interface.
//
// Native WebGPU via wgpu-native (v29, webgpu.h) — a first-class backend like
// Vulkan/D3D12, used by the editor on Windows and later as the base for the
// Emscripten/web export (TODO_RHI_SLANG.md Fase 5).
//
// Design notes (see TODO_RHI_SLANG.md "Fase 5 — Backend WebGPU"):
//   - wgpu_native.dll is loaded DYNAMICALLY (LoadLibrary + GetProcAddress) so
//     Vulkan/D3D12 users never need the DLL; the backend is inert unless
//     settings.graphics.backend == "webgpu".
//   - Handles are pointers to internal records cast to uint64 (like D3D12);
//     buffer/image records share their refcount with the memory record so a
//     buffer or image can own its memory (engine pattern).
//   - Push constants do not exist in WebGPU -> a per-layout UBO at
//     group index = setLayouts.size() (the WGSL shaders declare the push
//     uniform at that group; see engine/shaders/*.wgsl).
//   - Bindless: one shared cached bind group layout (binding 0 = texture
//     binding_array<N>, binding 1 = sampler binding_array<N>) whose bind group
//     holds N real-or-dummy entries and is rebuilt on register/update/unregister.
//     WebGPU requires the bind group layout of a bind group to EXACTLY match the
//     pipeline layout's at the same index, so all pipelines share the same
//     WGPUBindGroupLayout object.
//   - NDC is y-up (D3D12/Metal convention, gpuweb#416 "Solution 1"): the
//     viewport transform maps NDC +Y to the top of the framebuffer, so the
//     pass templates use POSITIVE-height viewports — no flip, exactly like the
//     D3D12 backend. CmdSetViewport/CmdSetScissor do NOT flip either. (Fixed
//     2026-08-15: an earlier design note said "y-down like Vulkan / negative
//     flip" which was wrong — that is Vulkan's convention, not WebGPU's.)
//   - The overlay pass uses loadOp=Load when the swapchain was written this
//     frame (BeginFrame(false), PhysicsDemo) else Clear (editor BeginFrame(true)
//     clears to the frame color; the first swapchain image is undefined).
//   - Image transitions are a no-op (wgpu synchronizes automatically); the
//     render pass is derived per frame from pass templates + attachment views.
//
// The public header must not include webgpu.h / wgpu types; everything is
// behind a PIMPL.

namespace Leir {
namespace RHI {

class LEIR_API WebGPUBackend : public RenderBackend {
public:
    // window: native GLFWwindow*.
    WebGPUBackend(void* window, int width, int height, bool vsync,
                  const std::string& appName);
    ~WebGPUBackend() override;

    // RenderBackend
    bool BeginFrame(bool skipRenderPass) override;
    void BeginSwapchainOverlay() override;
    void EndFrame() override;
    void WaitIdle() override;

    RHIFence CreateFence(bool signaled = true) override { (void)signaled; return {}; }
    void DestroyFence(RHIFence) override {}
    void WaitFence(RHIFence, uint64_t) override {}
    void ResetFence(RHIFence) override {}

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

    Leir::ISwapchainTarget* CreateSwapchainTarget(void* window) override;

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

    #if defined(__EMSCRIPTEN__)
    const char* GetShaderFileExtension() const override { return ".web.wgsl"; }
#else
    const char* GetShaderFileExtension() const override { return ".wgsl"; }
#endif
    const char* GetBackendName() const override { return "webgpu"; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

    // The per-window swapchain target accesses Impl internals (device/queue/
    // instance/proc pointers) to share them across external windows.
    friend class WebGPUSwapchainTarget;
};

} // namespace RHI
} // namespace Leir