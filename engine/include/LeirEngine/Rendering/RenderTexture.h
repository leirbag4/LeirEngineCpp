#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RHI.h"

namespace Leir {

namespace RHI { class RenderBackend; }

class LEIR_API RenderTexture {
public:
    RenderTexture(RHI::RenderBackend* device, uint32_t width, uint32_t height);
    ~RenderTexture();

    void Resize(uint32_t width, uint32_t height);

    void BeginRender(RHI::RHICommandBuffer cmd, const RHI::RHIClearValue& clearColor, float depthClear = 1.0f);
    void EndRender(RHI::RHICommandBuffer cmd);

    RHI::RHIDescriptorImageInfo GetDescriptorInfo() const;
    RHI::RHIDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
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
    void CreateDescriptorResources();
    void UpdateDescriptor();
    void DestroyDescriptorResources();

    RHI::RenderBackend* m_Device;
    uint32_t m_Width, m_Height;

    RHI::RHIImage m_ColorImage;
    RHI::RHIDeviceMemory m_ColorMemory;
    RHI::RHIImageView m_ColorImageView;

    RHI::RHIImage m_DepthImage;
    RHI::RHIDeviceMemory m_DepthMemory;
    RHI::RHIImageView m_DepthImageView;

    RHI::RHIRenderPass m_RenderPass;
    RHI::RHIFramebuffer m_Framebuffer;
    RHI::RHISampler m_Sampler;

    RHI::RHIDescriptorSetLayout m_DescSetLayout;
    RHI::RHIDescriptorPool m_DescPool;
    RHI::RHIDescriptorSet m_DescriptorSet;
};

} // namespace Leir
