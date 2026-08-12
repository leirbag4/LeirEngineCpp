#include "LeirEngine/Rendering/Material.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/RenderPipeline.h"

#include "LeirEngine/Core/Log.h"

namespace Leir {

Material::Material(RHI::RenderBackend* device, std::shared_ptr<Shader> shader)
    : m_Device(device)
    , m_Shader(std::move(shader))
{
    CreateDescriptorPool();
    CreateDescriptorSetLayout();
    CreateDescriptorSet();
}

Material::~Material()
{
    if (m_Pipeline.IsValid())
        m_Device->DestroyPipeline(m_Pipeline);
    if (m_PipelineLayout.IsValid())
        m_Device->DestroyPipelineLayout(m_PipelineLayout);
    if (m_DescriptorSetLayout.IsValid())
        m_Device->DestroyDescriptorSetLayout(m_DescriptorSetLayout);
    if (m_UBOSetLayout.IsValid())
        m_Device->DestroyDescriptorSetLayout(m_UBOSetLayout);
    if (m_DescriptorPool.IsValid())
        m_Device->DestroyDescriptorPool(m_DescriptorPool);
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture)
{
    (void)name;
    m_Texture = std::move(texture);
    UpdateDescriptorSet();
}

void Material::SetFloat(const std::string& name, float value)
{
    (void)name;
    (void)value;
}

void Material::SetVec3(const std::string& name, const Vector3& value)
{
    (void)name;
    (void)value;
}

void Material::Bind(RHI::RHICommandBuffer cmd, RHI::RHIPipelineLayout layout) const
{
    (void)layout;
    m_Device->CmdBindPipeline(cmd, m_Pipeline);
    if (m_DescriptorSet.IsValid())
        m_Device->CmdBindDescriptorSets(cmd, m_PipelineLayout, 1, { m_DescriptorSet });
}

void Material::RecreatePipeline(RHI::RHIRenderPass renderPass)
{
    if (m_Pipeline.IsValid()) {
        m_Device->DestroyPipeline(m_Pipeline);
        m_Pipeline = RHI::RHIPipeline{};
    }
    if (m_PipelineLayout.IsValid()) {
        m_Device->DestroyPipelineLayout(m_PipelineLayout);
        m_PipelineLayout = RHI::RHIPipelineLayout{};
    }
    // CreatePipeline() re-allocates m_UBOSetLayout; free the previous one so a
    // repeated RecreatePipeline doesn't leak a descriptor set layout.
    if (m_UBOSetLayout.IsValid()) {
        m_Device->DestroyDescriptorSetLayout(m_UBOSetLayout);
        m_UBOSetLayout = RHI::RHIDescriptorSetLayout{};
    }
    CreatePipeline(renderPass);
}

void Material::CreateDescriptorPool()
{
    std::vector<RHI::RHIDescriptorBinding> poolBindings = {
        { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment }
    };
    m_DescriptorPool = m_Device->CreateDescriptorPool(poolBindings, 1);
}

void Material::CreateDescriptorSetLayout()
{
    RHI::RHIDescriptorBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.type = RHI::DescriptorType::CombinedImageSampler;
    samplerBinding.count = 1;
    samplerBinding.stage = RHI::ShaderStage::Fragment;

    m_DescriptorSetLayout = m_Device->CreateDescriptorSetLayout({ samplerBinding });
}

void Material::CreateDescriptorSet()
{
    m_DescriptorSet = m_Device->AllocateDescriptorSet(m_DescriptorPool, m_DescriptorSetLayout);
}

void Material::UpdateDescriptorSet()
{
    if (!m_Texture)
        return;

    RHI::RHIDescriptorWrite write{};
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 0;
    write.count = 1;
    write.type = RHI::DescriptorType::CombinedImageSampler;
    write.imageInfo = m_Texture->GetDescriptorInfo();
    m_Device->WriteDescriptorSets({ write });
}

void Material::CreatePipeline(RHI::RHIRenderPass renderPass)
{
    RHI::RHIPushConstantRange pushRange{};
    pushRange.stage = RHI::ShaderStageMask::VertexFragment;
    pushRange.offset = 0;
    pushRange.size = (uint32_t)sizeof(PushConstants);

    m_UBOSetLayout = m_Device->CreateDescriptorSetLayout({
        { 0, RHI::DescriptorType::UniformBuffer, 1, RHI::ShaderStage::Vertex }
    });

    m_PipelineLayout = m_Device->CreatePipelineLayout(
        { m_UBOSetLayout, m_DescriptorSetLayout },
        { pushRange });

    auto binding = Vertex::GetBindingDescription();
    auto attrs = Vertex::GetAttributeDescriptions();

    RHI::RHIPipelineDesc desc{};
    desc.layout = m_PipelineLayout;
    desc.renderPass = renderPass;
    desc.stages = m_Shader->GetStageInfos();
    desc.vertexBinding = binding;
    desc.vertexAttributes = attrs;
    desc.topology = RHI::Topology::TriangleList;
    desc.polygonMode = RHI::PolygonMode::Fill;
    desc.cullMode = RHI::CullMode::Back;
    desc.depthTestEnable = true;
    desc.blend.enable = false;

    m_Pipeline = m_Device->CreateGraphicsPipeline(desc);
}

RHI::RHIDescriptorSetLayout Material::GetUBOSetLayout() const
{
    return m_UBOSetLayout;
}

} // namespace Leir
