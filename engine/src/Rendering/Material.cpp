#include "LeirEngine/Rendering/Material.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/RenderPipeline.h"

#include "LeirEngine/Core/Log.h"

namespace Leir {

Material::Material(RHI::RenderBackend* device, std::shared_ptr<Shader> shader)
    : m_Device(device)
    , m_Shader(std::move(shader))
{
    CreateSetLayouts();
}

Material::~Material()
{
    if (m_Pipeline.IsValid())
        m_Device->DestroyPipeline(m_Pipeline);
    if (m_PipelineLayout.IsValid())
        m_Device->DestroyPipelineLayout(m_PipelineLayout);
    for (auto& entry : m_SetLayouts) {
        if (entry.layout.IsValid())
            m_Device->DestroyDescriptorSetLayout(entry.layout);
    }
}

void Material::SetTexture(const std::string& name, std::shared_ptr<Texture2D> texture)
{
    (void)name;
    m_Texture = std::move(texture);
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
    // Set 1 is the bindless texture table (the backend's global bindless set).
    // All materials share it; the per-draw texture is selected via the
    // textureIndex push constant (see RenderPipeline::RenderMeshRenderer).
    m_Device->CmdBindDescriptorSets(cmd, m_PipelineLayout, 1,
        { m_Device->GetBindlessDescriptorSet() });
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
    // m_SetLayouts is created once in the ctor and reused: CreatePipeline()
    // rebuilds only the pipeline layout (which references the same set
    // layouts), so a repeated RecreatePipeline leaks nothing.
    CreatePipeline(renderPass);
}

void Material::CreateSetLayouts()
{
    if (m_Shader && m_Shader->HasReflection()) {
        // Derived from the shader's reflection sidecar (Plan B, Fase 2): the
        // pipeline layout then always matches the shader signature by
        // construction. Ascending set order: [set 0 (UBO), set 1 (bindless)].
        m_SetLayouts = CreateSetLayoutsFromReflection(m_Device, m_Shader->GetReflection());
    } else {
        // Legacy fallback (no sidecar present: engine running without a
        // shader compiler). Mirrors the pre-reflection hand-written layout.
        m_SetLayouts.clear();
        RHI::RHIDescriptorBinding ubo{};
        ubo.binding = 0;
        ubo.type = RHI::DescriptorType::UniformBuffer;
        ubo.count = 1;
        ubo.stage = RHI::ShaderStage::Vertex;
        m_SetLayouts.push_back({ 0, m_Device->CreateDescriptorSetLayout({ ubo }) }); // set 0

        RHI::RHIDescriptorBinding sampler{};
        sampler.binding = 0;
        sampler.type = RHI::DescriptorType::CombinedImageSampler;
        sampler.stage = RHI::ShaderStage::Fragment;
        sampler.bindless = true;
        m_SetLayouts.push_back({ 1, m_Device->CreateDescriptorSetLayout({ sampler }) }); // set 1
    }

    m_UBOSetLayout = m_SetLayouts.size() > 0 ? m_SetLayouts[0].layout : RHI::RHIDescriptorSetLayout{};
    m_DescriptorSetLayout = m_SetLayouts.size() > 1 ? m_SetLayouts[1].layout : RHI::RHIDescriptorSetLayout{};
}

void Material::CreatePipeline(RHI::RHIRenderPass renderPass)
{
    if (m_Shader && m_Shader->HasReflection()) {
        // Pipeline layout derived from the reflection sidecar: the descriptor
        // set layouts (one per set) + push ranges come straight from the
        // shader signature, so layout/shader mismatches are impossible.
        m_PipelineLayout = CreatePipelineLayoutFromReflection(
            m_Device, m_Shader->GetReflection(), m_SetLayouts);
    } else {
        RHI::RHIPushConstantRange pushRange{};
        pushRange.stage = RHI::ShaderStageMask::VertexFragment;
        pushRange.offset = 0;
        pushRange.size = (uint32_t)sizeof(PushConstants);

        std::vector<RHI::RHIDescriptorSetLayout> layouts;
        for (const auto& entry : m_SetLayouts)
            layouts.push_back(entry.layout);
        m_PipelineLayout = m_Device->CreatePipelineLayout(layouts, { pushRange });
    }

    BuildPipeline(renderPass);
}

void Material::ReloadShaders(RHI::RHIRenderPass renderPass)
{
    // Keep m_PipelineLayout / m_UBOSetLayout / m_DescriptorSetLayout: a shader
    // reload is assumed to keep the same bindings (Plan A), so the UBO/sampler
    // descriptor sets stay valid. Only the pipeline (built from the fresh
    // shader stages) is recreated.
    if (m_Pipeline.IsValid()) {
        m_Device->DestroyPipeline(m_Pipeline);
        m_Pipeline = RHI::RHIPipeline{};
    }
    BuildPipeline(renderPass);
    XConsole::Println("Material pipeline reloaded");
}

void Material::BuildPipeline(RHI::RHIRenderPass renderPass)
{
    auto binding = Vertex::GetBindingDescription();
    auto attrs = Vertex::GetAttributeDescriptions();
    // D3D12 input-layout semantic names (ignored by the Vulkan backend). Must
    // match the semantics Slang derives from the shader's varying fields.
    static const char* kBasicSemantics[] = { "POSITION", "NORMAL", "TEXCOORD" };
    for (size_t i = 0; i < attrs.size() && i < 3; ++i)
        attrs[i].semantic = kBasicSemantics[i];

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

uint32_t Material::GetTextureIndex() const
{
    return m_Texture ? m_Texture->GetBindlessIndex() : 0;
}

} // namespace Leir
