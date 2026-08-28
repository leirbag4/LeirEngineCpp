#include "LeirEngine/Rendering/RenderPipeline.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/Material.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/SpriteSheet.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
#include "LeirEngine/Components/SpriteRenderer.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"
#include "LeirEngine/Scene/Scene.h"

#include "LeirEngine/Core/Log.h"
#include <cstring>


namespace Leir {

static const int RENDER_FRAMES_IN_FLIGHT = 2;
static const size_t UBO_SIZE = sizeof(Matrix4x4);

// ---- SpriteVertex ----

RHI::RHIVertexInputBinding SpriteVertex::GetBindingDescription()
{
    RHI::RHIVertexInputBinding desc{};
    desc.binding = 0;
    desc.stride = sizeof(SpriteVertex);
    desc.inputRate = RHI::VertexInputRate::Vertex;
    return desc;
}

std::vector<RHI::RHIVertexAttribute> SpriteVertex::GetAttributeDescriptions()
{
    std::vector<RHI::RHIVertexAttribute> attrs(2);
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = RHI::Format::R32G32_SFLOAT;
    attrs[0].offset = offsetof(SpriteVertex, position);
    attrs[0].semantic = "POSITION";
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = RHI::Format::R32G32_SFLOAT;
    attrs[1].offset = offsetof(SpriteVertex, texCoord);
    attrs[1].semantic = "TEXCOORD";
    return attrs;
}

// ---- RenderPipeline ----

RenderPipeline::RenderPipeline(RHI::RenderBackend* device)
    : m_Device(device)
{
    std::vector<RHI::RHIDescriptorBinding> poolBindings = {
        { 0, RHI::DescriptorType::UniformBuffer, (uint32_t)RENDER_FRAMES_IN_FLIGHT, RHI::ShaderStage::Vertex }
    };
    m_UBODescriptorPool = m_Device->CreateDescriptorPool(poolBindings, RENDER_FRAMES_IN_FLIGHT);

    m_UBOBuffers.resize(RENDER_FRAMES_IN_FLIGHT);
    m_UBOSets.resize(RENDER_FRAMES_IN_FLIGHT);

    CreateSpriteResources();
}

RenderPipeline::~RenderPipeline()
{
    DestroySpriteResources();
    if (m_UBODescriptorPool.IsValid())
        m_Device->DestroyDescriptorPool(m_UBODescriptorPool);
    for (auto& buf : m_UBOBuffers) {
        if (buf.buffer.IsValid()) m_Device->DestroyBuffer(buf.buffer);
        if (buf.memory.IsValid()) m_Device->DestroyMemory(buf.memory);
    }
}

void RenderPipeline::CreateSpriteResources()
{
    // Shared quad: centered unit quad with UVs (texcoord Y flipped for correct orientation)
    std::vector<SpriteVertex> verts = {
        { {-0.5f, -0.5f}, {0.0f, 0.0f} },
        { { 0.5f, -0.5f}, {1.0f, 0.0f} },
        { { 0.5f,  0.5f}, {1.0f, 1.0f} },
        { {-0.5f,  0.5f}, {0.0f, 1.0f} },
    };
    std::vector<uint32_t> idxs = { 0, 1, 2, 0, 2, 3 };
    m_Sprite.indexCount = (int)idxs.size();

    size_t vbSize = verts.size() * sizeof(SpriteVertex);
    size_t ibSize = idxs.size() * sizeof(uint32_t);

    // Separate staging buffers for vertex and index data
    RHI::RHIDeviceMemory vertStagingMem;
    RHI::RHIBuffer vertStagingBuf = m_Device->CreateBuffer((uint32_t)vbSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        vertStagingMem);

    RHI::RHIDeviceMemory idxStagingMem;
    RHI::RHIBuffer idxStagingBuf = m_Device->CreateBuffer((uint32_t)ibSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        idxStagingMem);

    void* data;
    m_Device->MapMemory(vertStagingMem, 0, (uint32_t)vbSize, &data);
    memcpy(data, verts.data(), vbSize);
    m_Device->UnmapMemory(vertStagingMem);

    m_Device->MapMemory(idxStagingMem, 0, (uint32_t)ibSize, &data);
    memcpy(data, idxs.data(), ibSize);
    m_Device->UnmapMemory(idxStagingMem);

    // Vertex buffer (device local)
    m_Sprite.vertexBuffer = m_Device->CreateBuffer((uint32_t)vbSize,
        RHI::BufferUsage::Vertex | RHI::BufferUsage::TransferDst,
        RHI::MemoryProperty::DeviceLocal, m_Sprite.vertexMemory);

    // Index buffer (device local)
    m_Sprite.indexBuffer = m_Device->CreateBuffer((uint32_t)ibSize,
        RHI::BufferUsage::Index | RHI::BufferUsage::TransferDst,
        RHI::MemoryProperty::DeviceLocal, m_Sprite.indexMemory);

    m_Device->CopyBuffer(vertStagingBuf, m_Sprite.vertexBuffer, (uint32_t)vbSize);
    m_Device->CopyBuffer(idxStagingBuf, m_Sprite.indexBuffer, (uint32_t)ibSize);

    m_Device->DestroyBuffer(vertStagingBuf);
    m_Device->DestroyMemory(vertStagingMem);
    m_Device->DestroyBuffer(idxStagingBuf);
    m_Device->DestroyMemory(idxStagingMem);

    // Descriptor set + pipeline layouts derived from the Sprite shader
    // reflection sidecars (Plan B, Fase 2). Falls back to the hand-written
    // layout when the sidecars are missing (engine running without a compiler).
    const std::string vertPath = std::string(LEIR_SHADER_DIR) + "/Sprite.vert" + m_Device->GetShaderFileExtension();
    const std::string fragPath = std::string(LEIR_SHADER_DIR) + "/Sprite.frag" + m_Device->GetShaderFileExtension();
    const RHI::ShaderReflection spriteReflection = LoadShaderReflectionFromSidecars({ vertPath, fragPath });
    if (!spriteReflection.bindings.empty()) {
        m_Sprite.setLayouts = CreateSetLayoutsFromReflection(m_Device, spriteReflection);
        if (!m_Sprite.setLayouts.empty())
            m_Sprite.descSetLayout = m_Sprite.setLayouts[0].layout; // set 0: sampler
        m_Sprite.pipelineLayout = CreatePipelineLayoutFromReflection(
            m_Device, spriteReflection, m_Sprite.setLayouts);
    } else {
        // Descriptor set layout (set=0: bindless combined image sampler)
        RHI::RHIDescriptorBinding sampler{};
        sampler.binding = 0;
        sampler.type = RHI::DescriptorType::CombinedImageSampler;
        sampler.stage = RHI::ShaderStage::Fragment;
        sampler.bindless = true;
        m_Sprite.descSetLayout = m_Device->CreateDescriptorSetLayout({ sampler });
        m_Sprite.setLayouts = { { 0, m_Sprite.descSetLayout } };

        RHI::RHIPushConstantRange pushRange{};
        pushRange.stage = RHI::ShaderStageMask::VertexFragment;
        pushRange.offset = 0;
        pushRange.size = (uint32_t)sizeof(SpritePushConstants);

        m_Sprite.pipelineLayout = m_Device->CreatePipelineLayout(
            { m_Sprite.descSetLayout }, { pushRange });
    }

    CreateSpritePipeline();

    // Create a 1x1 white fallback texture for sprites without a texture
    Image fallbackImage(1, 1, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    m_Sprite.fallbackTexture = new Texture2D(m_Device, fallbackImage);

    XConsole::Println("Sprite pipeline created");
}

void RenderPipeline::CreateSpritePipeline()
{
    // Load sprite shaders
    auto vertCode = Shader::ReadFile(
        std::string(LEIR_SHADER_DIR) + "/Sprite.vert" + m_Device->GetShaderFileExtension());
    auto fragCode = Shader::ReadFile(
        std::string(LEIR_SHADER_DIR) + "/Sprite.frag" + m_Device->GetShaderFileExtension());
    RHI::RHIShaderModule vertMod = m_Device->CreateShaderModule(vertCode);
    RHI::RHIShaderModule fragMod = m_Device->CreateShaderModule(fragCode);

    RHI::RHIShaderStageInfo stages[2];
    stages[0].stage = RHI::ShaderStage::Vertex;
    stages[0].module = vertMod;
    stages[0].entryPoint = "main";
    stages[1].stage = RHI::ShaderStage::Fragment;
    stages[1].module = fragMod;
    stages[1].entryPoint = "main";

    auto bindingDesc = SpriteVertex::GetBindingDescription();
    auto attrDescs = SpriteVertex::GetAttributeDescriptions();

    // Enable blending for transparency, no depth test
    RHI::RHIPipelineDesc desc{};
    desc.layout = m_Sprite.pipelineLayout;
    desc.renderPass = m_Device->GetOverlayRenderPass();
    desc.stages = { stages[0], stages[1] };
    desc.vertexBinding = bindingDesc;
    desc.vertexAttributes = attrDescs;
    desc.topology = RHI::Topology::TriangleList;
    desc.polygonMode = RHI::PolygonMode::Fill;
    desc.cullMode = RHI::CullMode::None;
    desc.depthTestEnable = false;
    desc.blend.enable = true;
    m_Sprite.pipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);
}

void RenderPipeline::ReloadSpritePipeline()
{
    if (!m_Sprite.pipeline.IsValid())
        return;
    m_Device->DestroyPipeline(m_Sprite.pipeline);
    m_Sprite.pipeline = RHI::RHIPipeline{};
    CreateSpritePipeline();
    XConsole::Println("Sprite pipeline reloaded");
}

void RenderPipeline::DestroySpriteResources()
{
    if (m_Sprite.pipeline.IsValid()) m_Device->DestroyPipeline(m_Sprite.pipeline);
    if (m_Sprite.pipelineLayout.IsValid()) m_Device->DestroyPipelineLayout(m_Sprite.pipelineLayout);
    for (auto& entry : m_Sprite.setLayouts) {
        if (entry.layout.IsValid())
            m_Device->DestroyDescriptorSetLayout(entry.layout);
    }
    if (m_Sprite.vertexBuffer.IsValid()) m_Device->DestroyBuffer(m_Sprite.vertexBuffer);
    if (m_Sprite.vertexMemory.IsValid()) m_Device->DestroyMemory(m_Sprite.vertexMemory);
    if (m_Sprite.indexBuffer.IsValid()) m_Device->DestroyBuffer(m_Sprite.indexBuffer);
    if (m_Sprite.indexMemory.IsValid()) m_Device->DestroyMemory(m_Sprite.indexMemory);
    delete m_Sprite.fallbackTexture;
    m_Sprite.fallbackTexture = nullptr;
}

void RenderPipeline::Render(RHI::GCommandGraph& graph, ISceneStorage* scene)
{
    if (!scene)
        return;

    // Camera/light/renderables via the journal-synced ECS query groups: the
    // data-oriented render path (no per-frame GetObjects/GetComponent scans).
    Camera* primaryCamera = nullptr;
    scene->GetCameraGroup().ForEach([&](auto& hc, auto& active, auto&, ECS::Entity) {
        if (!active.value || primaryCamera)
            return;
        Camera* cam = hc.instance.get();
        if (cam && cam->IsPrimary())
            primaryCamera = cam;
    });
    if (!primaryCamera) {
        scene->GetCameraGroup().ForEach([&](auto& hc, auto& active, auto&, ECS::Entity) {
            if (!active.value || primaryCamera)
                return;
            Camera* cam = hc.instance.get();
            if (cam)
                primaryCamera = cam;
        });
    }
    if (!primaryCamera)
        return;

    Light* primaryLight = nullptr;
    scene->GetLightGroup().ForEach([&](auto& hc, auto& active, auto&, ECS::Entity) {
        if (!active.value || primaryLight)
            return;
        Light* light = hc.instance.get();
        if (light)
            primaryLight = light;
    });

    primaryCamera->RecalculateViewMatrix();
    Matrix4x4 viewProj = primaryCamera->GetViewProjectionMatrix();

    PushConstants push{};
    if (primaryLight) {
        push.lightDir = primaryLight->GetDirection();
        push.lightColor = primaryLight->GetColor() * primaryLight->GetIntensity();
    }

    scene->GetRenderGroup().ForEach([&](auto& hc, auto& active, auto& wt, ECS::Entity) {
        if (!active.value)
            return;
        MeshRenderer* renderer = hc.instance.get();
        if (!renderer)
            return;
        auto mesh = renderer->GetMesh();
        auto material = renderer->GetMaterial();
        if (!mesh || !material)
            return;
        push.color = material ? material->GetColor() : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        RenderMeshRenderer(graph, renderer, viewProj, wt.worldMatrix, push);
    });
}

void RenderPipeline::RenderOverlay(RHI::GCommandGraph& graph, ISceneStorage* scene)
{
    if (!scene)
        return;

    // Build sorted list of visible sprites (from the ECS sprite group).
    struct SpriteDraw {
        SpriteRenderer* renderer;
        Matrix4x4 world;
        int sortingLayer;
        int orderInLayer;
    };
    std::vector<SpriteDraw> draws;
    scene->GetSpriteGroup().ForEach([&](auto& hc, auto& active, auto& wt, ECS::Entity) {
        if (!active.value)
            return;
        SpriteRenderer* spr = hc.instance.get();
        if (!spr)
            return;
        draws.push_back({ spr, wt.worldMatrix, 0, 0 });
    });

    if (draws.empty()) {
        XConsole::PrintWarning("RenderOverlay: no sprites to draw");
        return;
    }

    // Orthographic projection: maps pixel coords to clip space (y-down)
    float w = (float)m_Device->GetSwapchainWidth();
    float h = (float)m_Device->GetSwapchainHeight();
    Matrix4x4 ortho = Matrix4x4::Ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    graph.BindPipeline(m_Sprite.pipeline);
    graph.BindVertexBuffer(m_Sprite.vertexBuffer);
    graph.BindIndexBuffer(m_Sprite.indexBuffer);

    for (auto& draw : draws) {
        Matrix4x4 mvp = ortho * draw.world;
        RenderSprite(graph, draw.renderer, mvp);
    }
}

void RenderPipeline::RenderSprite(RHI::GCommandGraph& graph, SpriteRenderer* renderer,
    const Matrix4x4& mvp)
{
    // Determine texture and UV rect
    auto* tex = renderer->GetTexture();
    auto* sheet = renderer->GetSpriteSheet();
    Vector4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f};

    if (sheet) {
        tex = sheet->GetTexture();
        uvRect = sheet->GetUVRect(renderer->GetFrameIndex());
    }

    if (!tex) tex = m_Sprite.fallbackTexture;

    // Bindless: bind the global bindless set once (set 0); the texture is
    // selected by textureIndex in the push constants below.
    graph.BindDescriptorSets(m_Sprite.pipelineLayout, 0,
        { m_Device->GetBindlessDescriptorSet() });

    SpritePushConstants push;
    push.mvp = mvp;
    push.color = renderer->GetColor();
    push.uvRect = uvRect;
    push.textureIndex = tex->GetBindlessIndex();
    graph.PushConstants(m_Sprite.pipelineLayout,
        RHI::ShaderStageMask::VertexFragment, 0, sizeof(SpritePushConstants), &push);

    // Last-use tracking: this draw samples the sprite texture.
    graph.SetSampledTextures({ tex->GetBindlessIndex() });
    graph.DrawIndexed(m_Sprite.indexCount, 1, 0);
}

void RenderPipeline::RenderMeshRenderer(RHI::GCommandGraph& graph, MeshRenderer* renderer,
    const Matrix4x4& viewProj, const Matrix4x4& model,
    const PushConstants& push)
{
    auto material = renderer->GetMaterial();
    auto mesh = renderer->GetMesh();
    if (!material || !mesh)
        return;

    RHI::RHIDescriptorSetLayout uboLayout = material->GetUBOSetLayout();
    uint32_t frame = m_Device->GetCurrentFrameIndex();

    if (!m_UBOBuffers[frame].buffer.IsValid()) {
        m_UBOBuffers[frame].buffer = m_Device->CreateBuffer((uint32_t)UBO_SIZE,
            RHI::BufferUsage::Uniform,
            RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
            m_UBOBuffers[frame].memory);
    }

    if (!m_UBOSets[frame].IsValid()) {
        m_UBOSets[frame] = m_Device->AllocateDescriptorSet(m_UBODescriptorPool, uboLayout);

        RHI::RHIDescriptorWrite write{};
        write.dstSet = m_UBOSets[frame];
        write.dstBinding = 0;
        write.count = 1;
        write.type = RHI::DescriptorType::UniformBuffer;
        write.bufferInfo.buffer = m_UBOBuffers[frame].buffer;
        write.bufferInfo.offset = 0;
        write.bufferInfo.range = (uint32_t)UBO_SIZE;
        write.bufferInfo.valid = true;
        m_Device->WriteDescriptorSets({ write });
    }

    auto& uboBuf = m_UBOBuffers[frame];

    void* data;
    m_Device->MapMemory(uboBuf.memory, 0, (uint32_t)UBO_SIZE, &data);
    memcpy(data, &viewProj, UBO_SIZE);
    m_Device->UnmapMemory(uboBuf.memory);

    material->Bind(graph, material->GetPipelineLayout());

    graph.BindDescriptorSets(material->GetPipelineLayout(), 0, { m_UBOSets[frame] });

    PushConstants pushWithModel = push;
    pushWithModel.model = model;
    pushWithModel.textureIndex = material->GetTextureIndex();
    graph.PushConstants(material->GetPipelineLayout(),
        RHI::ShaderStageMask::VertexFragment, 0, sizeof(PushConstants), &pushWithModel);

    // Last-use tracking: this draw samples the material's texture.
    graph.SetSampledTextures({ material->GetTextureIndex() });

    mesh->Bind(graph);
    mesh->Draw(graph);
}

} // namespace Leir
