#pragma once

#include "LeirEngine/Core/Export.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Leir {

class VulkanDevice;
class Scene;
class MeshRenderer;
class Camera;
class Light;
class Material;

struct LEIR_API PushConstants {
    glm::vec3 lightDir = {0.0f, -1.0f, 0.0f};
    float pad0 = 0.0f;
    glm::vec3 lightColor = {1.0f, 1.0f, 1.0f};
    float pad1 = 0.0f;
    glm::vec3 ambientColor = {0.2f, 0.2f, 0.3f};
    float pad2 = 0.0f;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    glm::mat4 model = glm::mat4(1.0f);
};

struct LEIR_API PerMeshUBO {
    glm::mat4 viewProjection;
};

struct UBOBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

class LEIR_API RenderPipeline {
public:
    RenderPipeline(VulkanDevice* device);
    ~RenderPipeline();

    void Render(VkCommandBuffer cmd, Scene* scene);

private:
    void RenderMeshRenderer(VkCommandBuffer cmd, MeshRenderer* renderer,
        const glm::mat4& viewProj, const glm::mat4& model,
        const PushConstants& push);

    VulkanDevice* m_Device;

    std::vector<UBOBuffer> m_UBOBuffers;
    std::vector<VkDescriptorSet> m_UBOSets;
    VkDescriptorPool m_UBODescriptorPool = VK_NULL_HANDLE;
};

} // namespace Leir
