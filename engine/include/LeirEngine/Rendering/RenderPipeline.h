#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include <vulkan/vulkan.h>
#include <array>
#include <memory>
#include <unordered_map>
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
    Vector3 lightDir = {0.0f, -1.0f, 0.0f};
    float pad0 = 0.0f;
    Vector3 lightColor = {1.0f, 1.0f, 1.0f};
    float pad1 = 0.0f;
    Vector3 ambientColor = {0.2f, 0.2f, 0.3f};
    float pad2 = 0.0f;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Matrix4x4 model;
};

struct LEIR_API PerMeshUBO {
    Matrix4x4 viewProjection;
};

struct UBOBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

struct LEIR_API SpriteVertex {
    Vector2 position;
    Vector2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

struct LEIR_API SpritePushConstants {
    Matrix4x4 mvp;
    Vector4 color;
    Vector4 uvRect; // {u, v, w, h} in UV space
};

class LEIR_API RenderPipeline {
public:
    RenderPipeline(VulkanDevice* device);
    ~RenderPipeline();

    void Render(VkCommandBuffer cmd, Scene* scene);
    void RenderOverlay(VkCommandBuffer cmd, Scene* scene);

private:
    void RenderMeshRenderer(VkCommandBuffer cmd, MeshRenderer* renderer,
        const Matrix4x4& viewProj, const Matrix4x4& model,
        const PushConstants& push);

    void CreateSpriteResources();
    void DestroySpriteResources();
    void RenderSprite(VkCommandBuffer cmd, SpriteRenderer* renderer,
        const Matrix4x4& mvp);

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
        Texture2D* fallbackTexture = nullptr;
        std::unordered_map<Texture2D*, VkDescriptorSet> descSetCache;
    } m_Sprite;
};

} // namespace Leir
