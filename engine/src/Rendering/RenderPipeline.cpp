#include "LeirEngine/Rendering/RenderPipeline.h"
#include "LeirEngine/Rendering/VulkanDevice.h"
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

#include <spdlog/spdlog.h>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace Leir {

static const int RENDER_FRAMES_IN_FLIGHT = 2;
static const size_t UBO_SIZE = sizeof(glm::mat4);

// ---- SpriteVertex ----

VkVertexInputBindingDescription SpriteVertex::GetBindingDescription()
{
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(SpriteVertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::vector<VkVertexInputAttributeDescription> SpriteVertex::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attrs(2);
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = offsetof(SpriteVertex, position);
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = offsetof(SpriteVertex, texCoord);
    return attrs;
}

// ---- RenderPipeline ----

RenderPipeline::RenderPipeline(VulkanDevice* device)
    : m_Device(device)
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RENDER_FRAMES_IN_FLIGHT }
    };
    m_UBODescriptorPool = m_Device->CreateDescriptorPool(poolSizes, RENDER_FRAMES_IN_FLIGHT);

    m_UBOBuffers.resize(RENDER_FRAMES_IN_FLIGHT);
    m_UBOSets.resize(RENDER_FRAMES_IN_FLIGHT);

    CreateSpriteResources();
}

RenderPipeline::~RenderPipeline()
{
    auto dev = m_Device->GetDevice();
    DestroySpriteResources();
    if (m_UBODescriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(dev, m_UBODescriptorPool, nullptr);
    for (auto& buf : m_UBOBuffers) {
        if (buf.buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(dev, buf.buffer, nullptr);
        if (buf.memory != VK_NULL_HANDLE)
            vkFreeMemory(dev, buf.memory, nullptr);
    }
}

void RenderPipeline::CreateSpriteResources()
{
    auto dev = m_Device->GetDevice();

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
    VkDeviceMemory vertStagingMem;
    VkBuffer vertStagingBuf = m_Device->CreateBuffer(vbSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertStagingMem);

    VkDeviceMemory idxStagingMem;
    VkBuffer idxStagingBuf = m_Device->CreateBuffer(ibSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        idxStagingMem);

    void* data;
    vkMapMemory(dev, vertStagingMem, 0, vbSize, 0, &data);
    memcpy(data, verts.data(), vbSize);
    vkUnmapMemory(dev, vertStagingMem);

    vkMapMemory(dev, idxStagingMem, 0, ibSize, 0, &data);
    memcpy(data, idxs.data(), ibSize);
    vkUnmapMemory(dev, idxStagingMem);

    // Vertex buffer (device local)
    m_Sprite.vertexBuffer = m_Device->CreateBuffer(vbSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Sprite.vertexMemory);

    // Index buffer (device local)
    m_Sprite.indexBuffer = m_Device->CreateBuffer(ibSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Sprite.indexMemory);

    m_Device->CopyBuffer(vertStagingBuf, m_Sprite.vertexBuffer, vbSize);
    m_Device->CopyBuffer(idxStagingBuf, m_Sprite.indexBuffer, ibSize);

    vkDestroyBuffer(dev, vertStagingBuf, nullptr);
    vkFreeMemory(dev, vertStagingMem, nullptr);
    vkDestroyBuffer(dev, idxStagingBuf, nullptr);
    vkFreeMemory(dev, idxStagingMem, nullptr);

    // Descriptor set layout (set=0, binding=0: combined image sampler)
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    m_Sprite.descSetLayout = m_Device->CreateDescriptorSetLayout({ samplerBinding });

    // Descriptor pool for sampler (256 max sets for texture caching)
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 }
    };
    m_Sprite.descPool = m_Device->CreateDescriptorPool(poolSizes, 256);

    // Pipeline layout
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(SpritePushConstants);

    m_Sprite.pipelineLayout = m_Device->CreatePipelineLayout(
        { m_Sprite.descSetLayout }, { pushRange });

    // Load sprite shaders
    auto vertCode = Shader::ReadFile(LEIR_SHADER_DIR "/Sprite.vert.spv");
    auto fragCode = Shader::ReadFile(LEIR_SHADER_DIR "/Sprite.frag.spv");
    VkShaderModule vertMod = m_Device->CreateShaderModule(vertCode);
    VkShaderModule fragMod = m_Device->CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2];
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    auto bindingDesc = SpriteVertex::GetBindingDescription();
    auto attrDescs = SpriteVertex::GetAttributeDescriptions();

    // Enable blending for transparency, no depth test
    m_Sprite.pipeline = m_Device->CreateGraphicsPipeline(
        m_Sprite.pipelineLayout,
        m_Device->GetOverlayRenderPass(),
        { stages[0], stages[1] },
        bindingDesc, attrDescs,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        false);

    vkDestroyShaderModule(dev, vertMod, nullptr);
    vkDestroyShaderModule(dev, fragMod, nullptr);

    // Create a 1x1 white fallback texture for sprites without a texture
    Image fallbackImage(1, 1, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_Sprite.fallbackTexture = new Texture2D(m_Device, fallbackImage);

    spdlog::info("Sprite pipeline created");
}

void RenderPipeline::DestroySpriteResources()
{
    auto dev = m_Device->GetDevice();
    if (m_Sprite.pipeline) vkDestroyPipeline(dev, m_Sprite.pipeline, nullptr);
    if (m_Sprite.pipelineLayout) vkDestroyPipelineLayout(dev, m_Sprite.pipelineLayout, nullptr);
    if (m_Sprite.descSetLayout) vkDestroyDescriptorSetLayout(dev, m_Sprite.descSetLayout, nullptr);
    if (m_Sprite.descPool) vkDestroyDescriptorPool(dev, m_Sprite.descPool, nullptr);
    if (m_Sprite.vertexBuffer) vkDestroyBuffer(dev, m_Sprite.vertexBuffer, nullptr);
    if (m_Sprite.vertexMemory) vkFreeMemory(dev, m_Sprite.vertexMemory, nullptr);
    if (m_Sprite.indexBuffer) vkDestroyBuffer(dev, m_Sprite.indexBuffer, nullptr);
    if (m_Sprite.indexMemory) vkFreeMemory(dev, m_Sprite.indexMemory, nullptr);
    delete m_Sprite.fallbackTexture;
    m_Sprite.fallbackTexture = nullptr;
    m_Sprite.descSetCache.clear();
}

void RenderPipeline::Render(VkCommandBuffer cmd, Scene* scene)
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
    glm::mat4 viewProj = primaryCamera->GetViewProjectionMatrix();

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

        push.color = material ? material->GetColor() : glm::vec4(1.0f);

        RenderMeshRenderer(cmd, renderer, viewProj,
            obj->GetTransform().GetLocalToWorldMatrix(), push);
    }
}

void RenderPipeline::RenderOverlay(VkCommandBuffer cmd, Scene* scene)
{
    if (!scene)
        return;

    auto& objects = scene->GetObjects();

    // Build sorted list of visible sprites
    struct SpriteDraw {
        SpriteRenderer* renderer;
        glm::mat4 world;
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
        spdlog::warn("RenderOverlay: no sprites to draw");
        return;
    }

    // Sort by layer then order (not needed yet, but ready)
    // std::sort(draws.begin(), draws.end(), ...);

    // Orthographic projection: maps pixel coords to clip space (y-down)
    float w = (float)m_Device->GetSwapchainExtent().width;
    float h = (float)m_Device->GetSwapchainExtent().height;
    glm::mat4 ortho = glm::ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Sprite.pipeline);

    VkBuffer vb[] = { m_Sprite.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, m_Sprite.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto& draw : draws) {
        glm::mat4 mvp = ortho * draw.world;
        RenderSprite(cmd, draw.renderer, mvp);
    }
}

void RenderPipeline::RenderSprite(VkCommandBuffer cmd, SpriteRenderer* renderer,
    const glm::mat4& mvp)
{
    // Determine texture and UV rect
    auto* tex = renderer->GetTexture();
    auto* sheet = renderer->GetSpriteSheet();
    glm::vec4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f};

    if (sheet) {
        tex = sheet->GetTexture();
        uvRect = sheet->GetUVRect(renderer->GetFrameIndex());
    }

    if (!tex) tex = m_Sprite.fallbackTexture;

    // Cache one descriptor set per unique texture — write once, never update
    auto it = m_Sprite.descSetCache.find(tex);
    if (it == m_Sprite.descSetCache.end()) {
        VkDescriptorSet newSet;
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_Sprite.descPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_Sprite.descSetLayout;
        if (vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo, &newSet) != VK_SUCCESS) {
            spdlog::error("RenderSprite: failed to allocate descriptor set");
            return;
        }

        VkDescriptorImageInfo imgInfo = tex->GetDescriptorInfo();
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = newSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);

        m_Sprite.descSetCache[tex] = newSet;
        it = m_Sprite.descSetCache.find(tex);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Sprite.pipelineLayout, 0, 1, &it->second, 0, nullptr);

    SpritePushConstants push;
    push.mvp = mvp;
    push.color = renderer->GetColor();
    push.uvRect = uvRect;
    vkCmdPushConstants(cmd, m_Sprite.pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(SpritePushConstants), &push);

    vkCmdDrawIndexed(cmd, m_Sprite.indexCount, 1, 0, 0, 0);
}

void RenderPipeline::RenderMeshRenderer(VkCommandBuffer cmd, MeshRenderer* renderer,
    const glm::mat4& viewProj, const glm::mat4& model,
    const PushConstants& push)
{
    auto material = renderer->GetMaterial();
    auto mesh = renderer->GetMesh();
    if (!material || !mesh)
        return;

    VkDescriptorSetLayout uboLayout = material->GetUBOSetLayout();
    uint32_t frame = m_Device->GetCurrentFrameIndex();

    if (m_UBOBuffers[frame].buffer == VK_NULL_HANDLE) {
        m_UBOBuffers[frame].buffer = m_Device->CreateBuffer(UBO_SIZE,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_UBOBuffers[frame].memory);
    }

    if (m_UBOSets[frame] == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_UBODescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &uboLayout;

        if (vkAllocateDescriptorSets(m_Device->GetDevice(), &allocInfo, &m_UBOSets[frame]) != VK_SUCCESS) {
            spdlog::error("Failed to allocate UBO descriptor set");
            return;
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UBOBuffers[frame].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = UBO_SIZE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_UBOSets[frame];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Device->GetDevice(), 1, &write, 0, nullptr);
    }

    auto& uboBuf = m_UBOBuffers[frame];

    void* data;
    vkMapMemory(m_Device->GetDevice(), uboBuf.memory, 0, UBO_SIZE, 0, &data);
    memcpy(data, &viewProj, UBO_SIZE);
    vkUnmapMemory(m_Device->GetDevice(), uboBuf.memory);

    material->Bind(cmd, material->GetPipelineLayout());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        material->GetPipelineLayout(), 0, 1, &m_UBOSets[frame], 0, nullptr);

    PushConstants pushWithModel = push;
    pushWithModel.model = model;
    vkCmdPushConstants(cmd, material->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushWithModel);

    mesh->Bind(cmd);
    mesh->Draw(cmd);
}

} // namespace Leir
