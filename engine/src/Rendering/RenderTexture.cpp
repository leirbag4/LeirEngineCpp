#include "LeirEngine/Rendering/RenderTexture.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include <stdexcept>
#include <vector>

namespace Leir {

RenderTexture::RenderTexture(RHI::RenderBackend* device, uint32_t width, uint32_t height)
    : m_Device(device), m_Width(width), m_Height(height)
{
    CreateRenderPass();
    CreateSampler();
    CreateResources();
    CreateDescriptorResources();
}

RenderTexture::~RenderTexture()
{
    // Drain the GPU before destroying the resources: the last frame may still
    // be rendering to/from them (the editor destroys the viewport RT in
    // OnShutdown, before the backend's own WaitIdle runs). Mirrors Resize().
    // Without this the D3D12 debug layer raises 0x87d (resource destroyed while
    // in GPU use) → unhandled break → WER → multi-second close.
    m_Device->WaitIdle();
    DestroyDescriptorResources();
    DestroyResources();
    if (m_PassTemplate.IsValid()) m_Device->DestroyPassTemplate(m_PassTemplate);
    if (m_Sampler.IsValid()) m_Device->DestroySampler(m_Sampler);
    if (m_RenderPass.IsValid()) m_Device->DestroyRenderPass(m_RenderPass);
}

void RenderTexture::Resize(uint32_t width, uint32_t height)
{
    if (width == m_Width && height == m_Height)
        return;

    // Wait for the GPU to finish using the current resources before destroying
    // them (they are recreated at the new size below). Without this wait,
    // destroying the image/framebuffer while the previous frame still renders
    // to it causes flicker/glitches during splitter drags (restored from the
    // pre-RHI implementation).
    m_Device->WaitIdle();

    DestroyResources();
    m_Width = width;
    m_Height = height;
    CreateResources();
    UpdateDescriptor();
    // The template viewport/scissor/renderArea depend on the size.
    BuildPassTemplate(m_BakedClearColor, m_BakedDepthClear);
}

void RenderTexture::CreateRenderPass()
{
    m_RenderPass = m_Device->CreateRenderPass(
        { RHI::Format::B8G8R8A8_SRGB },
        RHI::Format::D32_SFLOAT,
        false);
    BuildPassTemplate(m_BakedClearColor, m_BakedDepthClear);
}

void RenderTexture::BuildPassTemplate(const RHI::RHIClearValue& clearColor, float depthClear)
{
    if (m_PassTemplate.IsValid()) {
        m_Device->DestroyPassTemplate(m_PassTemplate);
        m_PassTemplate = RHI::RHIPassTemplate{};
    }

    RHI::RHIClearValue depthClearValue;
    depthClearValue.depth = depthClear;
    depthClearValue.isDepth = true;

    RHI::RHIPassTemplateDesc desc;
    desc.renderPass = m_RenderPass;
    desc.clearValues = { clearColor, depthClearValue };
    desc.viewport.x = 0.0f;
    desc.viewport.y = 0.0f;
    desc.viewport.width = (float)m_Width;
    desc.viewport.height = (float)m_Height;
    desc.viewport.minDepth = 0.0f;
    desc.viewport.maxDepth = 1.0f;
    desc.scissor.x = 0;
    desc.scissor.y = 0;
    desc.scissor.width = m_Width;
    desc.scissor.height = m_Height;

    m_BakedClearColor = clearColor;
    m_BakedDepthClear = depthClear;
    m_PassTemplate = m_Device->CreatePassTemplate(desc);
}

void RenderTexture::CreateSampler()
{
    m_Sampler = m_Device->CreateSampler(RHI::Filter::Linear, RHI::SamplerAddressMode::ClampToEdge);
}

void RenderTexture::CreateResources()
{
    RHI::Format colorFormat = RHI::Format::B8G8R8A8_SRGB;
    RHI::Format depthFormat = RHI::Format::D32_SFLOAT;

    // Color image (color attachment + sampled)
    m_ColorImage = m_Device->CreateImage(m_Width, m_Height, colorFormat,
        RHI::ImageUsage::ColorAttachment | RHI::ImageUsage::Sampled,
        RHI::MemoryProperty::DeviceLocal,
        m_ColorMemory);
    m_ColorImageView = m_Device->CreateImageView(m_ColorImage, colorFormat, RHI::Aspect::Color);

    // Depth image
    m_DepthImage = m_Device->CreateImage(m_Width, m_Height, depthFormat,
        RHI::ImageUsage::DepthStencilAttachment,
        RHI::MemoryProperty::DeviceLocal,
        m_DepthMemory);
    m_DepthImageView = m_Device->CreateImageView(m_DepthImage, depthFormat, RHI::Aspect::Depth);

    // Framebuffer
    m_Framebuffer = m_Device->CreateFramebuffer(m_RenderPass, m_Width, m_Height,
        { m_ColorImageView, m_DepthImageView });
}

void RenderTexture::DestroyResources()
{
    if (m_Framebuffer.IsValid()) m_Device->DestroyFramebuffer(m_Framebuffer);
    m_Framebuffer = RHI::RHIFramebuffer{};
    if (m_DepthImageView.IsValid()) m_Device->DestroyImageView(m_DepthImageView);
    if (m_DepthImage.IsValid()) m_Device->DestroyImage(m_DepthImage);
    if (m_DepthMemory.IsValid()) m_Device->DestroyMemory(m_DepthMemory);
    m_DepthImageView = RHI::RHIImageView{};
    m_DepthImage = RHI::RHIImage{};
    m_DepthMemory = RHI::RHIDeviceMemory{};
    if (m_ColorImageView.IsValid()) m_Device->DestroyImageView(m_ColorImageView);
    if (m_ColorImage.IsValid()) m_Device->DestroyImage(m_ColorImage);
    if (m_ColorMemory.IsValid()) m_Device->DestroyMemory(m_ColorMemory);
    m_ColorImageView = RHI::RHIImageView{};
    m_ColorImage = RHI::RHIImage{};
    m_ColorMemory = RHI::RHIDeviceMemory{};
}

void RenderTexture::BeginRender(RHI::RHICommandBuffer cmd, const RHI::RHIClearValue& clearColor, float depthClear)
{
    // Rebuild the persistent template only when the requested clears change
    // (steady state: no per-frame re-encode of the render-pass state).
    if (clearColor.color != m_BakedClearColor.color || depthClear != m_BakedDepthClear)
        BuildPassTemplate(clearColor, depthClear);

    // Transition the color image to COLOR_ATTACHMENT_OPTIMAL before the render
    // pass. The render pass initialLayout is COLOR_ATTACHMENT_OPTIMAL, but the
    // previous frame's EndRender left it in SHADER_READ_ONLY — a mismatch the
    // validation layer rejects (VUID-vkCmdDraw-None-09600) and that reads back
    // garbage (magenta viewport). Restored from the pre-RHI implementation.
    m_Device->CmdTransitionImageLayout(cmd, m_ColorImage, RHI::Format::B8G8R8A8_SRGB,
        RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment, RHI::Aspect::Color);

    m_Device->CmdBeginRenderPass(cmd, m_PassTemplate, m_Framebuffer);
}

void RenderTexture::EndRender(RHI::RHICommandBuffer cmd)
{
    m_Device->CmdEndRenderPass(cmd);

    // Sync so the sampled read (UI viewport) sees the rendered content.
    m_Device->CmdTransitionImageLayout(cmd, m_ColorImage, RHI::Format::B8G8R8A8_SRGB,
        RHI::ImageLayout::ShaderReadOnly, RHI::ImageLayout::ShaderReadOnly, RHI::Aspect::Color);
}

RHI::RHIDescriptorImageInfo RenderTexture::GetDescriptorInfo() const
{
    RHI::RHIDescriptorImageInfo info;
    info.imageView = m_ColorImageView;
    info.sampler = m_Sampler;
    info.valid = true;
    return info;
}

void RenderTexture::CreateDescriptorResources()
{
    RHI::RHIDescriptorBinding binding{};
    binding.binding = 0;
    binding.type = RHI::DescriptorType::CombinedImageSampler;
    binding.count = 1;
    binding.stage = RHI::ShaderStage::Fragment;
    m_DescSetLayout = m_Device->CreateDescriptorSetLayout({ binding });

    std::vector<RHI::RHIDescriptorBinding> poolBindings = {
        { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment }
    };
    m_DescPool = m_Device->CreateDescriptorPool(poolBindings, 1);

    m_DescriptorSet = m_Device->AllocateDescriptorSet(m_DescPool, m_DescSetLayout);

    UpdateDescriptor();
}

void RenderTexture::UpdateDescriptor()
{
    RHI::RHIDescriptorWrite write{};
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 0;
    write.count = 1;
    write.type = RHI::DescriptorType::CombinedImageSampler;
    write.imageInfo = GetDescriptorInfo();
    m_Device->WriteDescriptorSets({ write });
}

void RenderTexture::DestroyDescriptorResources()
{
    if (m_DescPool.IsValid()) m_Device->DestroyDescriptorPool(m_DescPool);
    if (m_DescSetLayout.IsValid()) m_Device->DestroyDescriptorSetLayout(m_DescSetLayout);
    m_DescriptorSet = RHI::RHIDescriptorSet{};
    m_DescPool = RHI::RHIDescriptorPool{};
    m_DescSetLayout = RHI::RHIDescriptorSetLayout{};
}

} // namespace Leir
