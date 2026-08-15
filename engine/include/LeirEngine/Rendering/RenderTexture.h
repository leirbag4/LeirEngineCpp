#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"

namespace Leir {

namespace RHI { class RenderBackend; }

class LEIR_API RenderTexture {
public:
    RenderTexture(RHI::RenderBackend* device, uint32_t width, uint32_t height);
    ~RenderTexture();

    void Resize(uint32_t width, uint32_t height);

    // Records a render-pass begin/end into the graph (attachments + transitions
    // are handled by the backend's CmdExecuteGraph last-use tracking — the old
    // manual CmdTransitionImageLayout calls are gone).
    void BeginRender(RHI::GCommandGraph& graph, const RHI::RHIClearValue& clearColor, float depthClear = 1.0f);
    void EndRender(RHI::GCommandGraph& graph);

    RHI::RHIDescriptorImageInfo GetDescriptorInfo() const;
    // Stable index into the backend's global bindless texture table (updated
    // in place on Resize, so the descriptor heaps never grow).
    uint32_t GetBindlessIndex() const { return m_BindlessIndex; }
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    RHI::RHIFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    RHI::RHIRenderPass GetRenderPass() const { return m_RenderPass; }
    RHI::RHIImageView GetImageView() const { return m_ColorImageView; }
    RHI::RHISampler GetSampler() const { return m_Sampler; }

private:
    void CreateRenderPass();
    void CreateSampler();
    void CreateResources();
    void DestroyResources();

    // Builds the persistent pass template (clears + full-rect viewport/scissor).
    // Rebuilt on Resize and when the requested clears change.
    void BuildPassTemplate(const RHI::RHIClearValue& clearColor, float depthClear);
    void DestroyPassTemplate();

    RHI::RenderBackend* m_Device;
    uint32_t m_Width, m_Height;

    RHI::RHIImage m_ColorImage;
    RHI::RHIDeviceMemory m_ColorMemory;
    RHI::RHIImageView m_ColorImageView;

    RHI::RHIImage m_DepthImage;
    RHI::RHIDeviceMemory m_DepthMemory;
    RHI::RHIImageView m_DepthImageView;

    RHI::RHIRenderPass m_RenderPass;
    RHI::RHIPassTemplate m_PassTemplate;
    RHI::RHIClearValue m_BakedClearColor;
    float m_BakedDepthClear = 1.0f;
    RHI::RHIFramebuffer m_Framebuffer;
    RHI::RHISampler m_Sampler;

    uint32_t m_BindlessIndex = 0;
};

} // namespace Leir
