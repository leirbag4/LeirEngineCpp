#pragma once
#include "LeirEngine/Core/Export.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

namespace Leir {

class VulkanDevice;
class Scene;
class MeshRenderer;
class Camera;
class Light;
class Material;
class SpriteRenderer;
class SpriteSheet;
class Texture2D;

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

struct LEIR_API SpriteVertex {
    glm::vec2 position;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

struct LEIR_API SpritePushConstants {
    glm::mat4 mvp;
    glm::vec4 color;
    glm::vec4 uvRect; // {u, v, w, h} in UV space
};

class LEIR_API RenderPipeline {
public:
    RenderPipeline(VulkanDevice* device);
    ~RenderPipeline();

    void Render(VkCommandBuffer cmd, Scene* scene);
    void RenderOverlay(VkCommandBuffer cmd, Scene* scene);

private:
    void RenderMeshRenderer(VkCommandBuffer cmd, MeshRenderer* renderer,
        const glm::mat4& viewProj, const glm::mat4& model,
        const PushConstants& push);

    void CreateSpriteResources();
    void DestroySpriteResources();
    void RenderSprite(VkCommandBuffer cmd, SpriteRenderer* renderer,
        const glm::mat4& mvp);

    VulkanDevice* m_Device;
    std::vector<UBOBuffer> m_UBOBuffers;
    std::vector<VkDescriptorSet> m_UBOSets;
    VkDescriptorPool m_UBODescriptorPool = VK_NULL_HANDLE;

    // 2D sprite pipeline
    struct {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descPool = VK_NULL_HANDLE;
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        int indexCount = 0;
        std::array<VkDescriptorSet, 2> descSets = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        Texture2D* fallbackTexture = nullptr;
    } m_Sprite;
};

} // namespace Leir
