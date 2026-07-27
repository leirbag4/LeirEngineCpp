#include "LeirEngine/Rendering/RenderPipeline.h"
#include "LeirEngine/Rendering/VulkanDevice.h"
#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/Material.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"
#include "LeirEngine/Scene/Scene.h"

#include <spdlog/spdlog.h>
#include <cstring>

namespace Leir {

static const int RENDER_FRAMES_IN_FLIGHT = 2;
static const size_t UBO_SIZE = sizeof(PerMeshUBO);

RenderPipeline::RenderPipeline(VulkanDevice* device)
    : m_Device(device)
{
    // Pre-allocate UBO descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RENDER_FRAMES_IN_FLIGHT }
    };
    m_UBODescriptorPool = m_Device->CreateDescriptorPool(poolSizes, RENDER_FRAMES_IN_FLIGHT);

    // Pre-allocate per-frame UBO buffers and descriptor sets
    m_UBOBuffers.resize(RENDER_FRAMES_IN_FLIGHT);
    m_UBOSets.resize(RENDER_FRAMES_IN_FLIGHT);
}

RenderPipeline::~RenderPipeline()
{
    auto dev = m_Device->GetDevice();
    if (m_UBODescriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(dev, m_UBODescriptorPool, nullptr);
    for (auto& buf : m_UBOBuffers) {
        if (buf.buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(dev, buf.buffer, nullptr);
        if (buf.memory != VK_NULL_HANDLE)
            vkFreeMemory(dev, buf.memory, nullptr);
    }
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

    // Create UBO buffer for this frame if not already created
    if (m_UBOBuffers[frame].buffer == VK_NULL_HANDLE) {
        m_UBOBuffers[frame].buffer = m_Device->CreateBuffer(UBO_SIZE,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_UBOBuffers[frame].memory);
    }

    // Allocate UBO descriptor set for this frame (once)
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

    PerMeshUBO ubo{};
    ubo.viewProjection = viewProj;
    ubo.model = model;

    void* data;
    vkMapMemory(m_Device->GetDevice(), uboBuf.memory, 0, UBO_SIZE, 0, &data);
    memcpy(data, &ubo, UBO_SIZE);
    vkUnmapMemory(m_Device->GetDevice(), uboBuf.memory);

    material->Bind(cmd, material->GetPipelineLayout());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        material->GetPipelineLayout(), 0, 1, &m_UBOSets[frame], 0, nullptr);

    vkCmdPushConstants(cmd, material->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &push);

    mesh->Bind(cmd);
    mesh->Draw(cmd);
}

} // namespace Leir
