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

    // Descriptor set layout (set=0, binding=0: combined image sampler)
    m_Sprite.descSetLayout = m_Device->CreateDescriptorSetLayout({
        { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment }
    });

    // Descriptor pool for sampler (256 max sets for texture caching)
    std::vector<RHI::RHIDescriptorBinding> poolBindings = {
        { 0, RHI::DescriptorType::CombinedImageSampler, 256, RHI::ShaderStage::Fragment }
    };
    m_Sprite.descPool = m_Device->CreateDescriptorPool(poolBindings, 256);

    // Pipeline layout
    RHI::RHIPushConstantRange pushRange{};
    pushRange.stage = RHI::ShaderStageMask::VertexFragment;
    pushRange.offset = 0;
    pushRange.size = (uint32_t)sizeof(SpritePushConstants);

    m_Sprite.pipelineLayout = m_Device->CreatePipelineLayout(
        { m_Sprite.descSetLayout }, { pushRange });

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
    if (m_Sprite.descSetLayout.IsValid()) m_Device->DestroyDescriptorSetLayout(m_Sprite.descSetLayout);
    if (m_Sprite.descPool.IsValid()) m_Device->DestroyDescriptorPool(m_Sprite.descPool);
    if (m_Sprite.vertexBuffer.IsValid()) m_Device->DestroyBuffer(m_Sprite.vertexBuffer);
    if (m_Sprite.vertexMemory.IsValid()) m_Device->DestroyMemory(m_Sprite.vertexMemory);
    if (m_Sprite.indexBuffer.IsValid()) m_Device->DestroyBuffer(m_Sprite.indexBuffer);
    if (m_Sprite.indexMemory.IsValid()) m_Device->DestroyMemory(m_Sprite.indexMemory);
    delete m_Sprite.fallbackTexture;
    m_Sprite.fallbackTexture = nullptr;
    m_Sprite.descSetCache.clear();
}

void RenderPipeline::Render(RHI::RHICommandBuffer cmd, Scene* scene)
{
    if (!scene)
        return;

    auto& objects = scene->GetObjects();
    Camera* primaryCamera = nullptr;
    Light* primaryLight = nullptr;

    for (auto& obj : objects) {
        if (!obj->IsActive())
            continue;
        if (!primaryCamera) {
            Camera* cam = obj->GetComponent<Camera>();
            if (cam && cam->IsPrimary())
                primaryCamera = cam;
        }
        if (!primaryLight) {
            Light* light = obj->GetComponent<Light>();
            if (light)
                primaryLight = light;
        }
        if (primaryCamera && primaryLight)
            break;
    }

    if (!primaryCamera) {
        for (auto& obj : objects) {
            if (!obj->IsActive())
                continue;
            Camera* cam = obj->GetComponent<Camera>();
            if (cam) {
                primaryCamera = cam;
                break;
            }
        }
    }

    if (!primaryCamera)
        return;

    primaryCamera->RecalculateViewMatrix();
    Matrix4x4 viewProj = primaryCamera->GetViewProjectionMatrix();

    PushConstants push{};
    if (primaryLight) {
        push.lightDir = primaryLight->GetDirection();
        push.lightColor = primaryLight->GetColor() * primaryLight->GetIntensity();
    }

    for (auto& obj : objects) {
        if (!obj->IsActive())
            continue;

        MeshRenderer* renderer = obj->GetComponent<MeshRenderer>();
        if (!renderer)
            continue;

        auto mesh = renderer->GetMesh();
        auto material = renderer->GetMaterial();
        if (!mesh || !material)
            continue;

        push.color = material ? material->GetColor() : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        RenderMeshRenderer(cmd, renderer, viewProj,
            obj->GetTransform().GetLocalToWorldMatrix(), push);
    }
}

void RenderPipeline::RenderOverlay(RHI::RHICommandBuffer cmd, Scene* scene)
{
    if (!scene)
        return;

    auto& objects = scene->GetObjects();

    // Build sorted list of visible sprites
    struct SpriteDraw {
        SpriteRenderer* renderer;
        Matrix4x4 world;
        int sortingLayer;
        int orderInLayer;
    };
    std::vector<SpriteDraw> draws;

    for (auto& obj : objects) {
        if (!obj->IsActive())
            continue;
        auto* spr = obj->GetComponent<SpriteRenderer>();
        if (!spr)
            continue;
        draws.push_back({ spr, obj->GetTransform().GetLocalToWorldMatrix(), 0, 0 });
    }

    if (draws.empty()) {
        XConsole::PrintWarning("RenderOverlay: no sprites to draw");
        return;
    }

    // Orthographic projection: maps pixel coords to clip space (y-down)
    float w = (float)m_Device->GetSwapchainWidth();
    float h = (float)m_Device->GetSwapchainHeight();
    Matrix4x4 ortho = Matrix4x4::Ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    m_Device->CmdBindPipeline(cmd, m_Sprite.pipeline);
    m_Device->CmdBindVertexBuffer(cmd, m_Sprite.vertexBuffer);
    m_Device->CmdBindIndexBuffer(cmd, m_Sprite.indexBuffer);

    for (auto& draw : draws) {
        Matrix4x4 mvp = ortho * draw.world;
        RenderSprite(cmd, draw.renderer, mvp);
    }
}

void RenderPipeline::RenderSprite(RHI::RHICommandBuffer cmd, SpriteRenderer* renderer,
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

    // Cache one descriptor set per unique texture — write once, never update
    auto it = m_Sprite.descSetCache.find(tex);
    if (it == m_Sprite.descSetCache.end()) {
        RHI::RHIDescriptorSet newSet = m_Device->AllocateDescriptorSet(m_Sprite.descPool, m_Sprite.descSetLayout);

        RHI::RHIDescriptorWrite write{};
        write.dstSet = newSet;
        write.dstBinding = 0;
        write.count = 1;
        write.type = RHI::DescriptorType::CombinedImageSampler;
        write.imageInfo = tex->GetDescriptorInfo();
        m_Device->WriteDescriptorSets({ write });

        m_Sprite.descSetCache[tex] = newSet;
        it = m_Sprite.descSetCache.find(tex);
    }

    m_Device->CmdBindDescriptorSets(cmd, m_Sprite.pipelineLayout, 0, { it->second });

    SpritePushConstants push;
    push.mvp = mvp;
    push.color = renderer->GetColor();
    push.uvRect = uvRect;
    m_Device->CmdPushConstants(cmd, m_Sprite.pipelineLayout,
        RHI::ShaderStageMask::VertexFragment, 0, sizeof(SpritePushConstants), &push);

    m_Device->CmdDrawIndexed(cmd, m_Sprite.indexCount, 1, 0);
}

void RenderPipeline::RenderMeshRenderer(RHI::RHICommandBuffer cmd, MeshRenderer* renderer,
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

    material->Bind(cmd, material->GetPipelineLayout());

    m_Device->CmdBindDescriptorSets(cmd, material->GetPipelineLayout(), 0, { m_UBOSets[frame] });

    PushConstants pushWithModel = push;
    pushWithModel.model = model;
    m_Device->CmdPushConstants(cmd, material->GetPipelineLayout(),
        RHI::ShaderStageMask::VertexFragment, 0, sizeof(PushConstants), &pushWithModel);

    mesh->Bind(cmd);
    mesh->Draw(cmd);
}

} // namespace Leir
