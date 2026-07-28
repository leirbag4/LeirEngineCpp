#pragma once
#include "LeirEngine/Core/Export.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Leir {

class VulkanDevice;
class UICanvas;
class Texture2D;

struct LEIR_API UIVertex {
    glm::vec2 position;
    glm::vec2 texCoord;
    glm::vec4 color;
};

class LEIR_API UIRenderer {
public:
    UIRenderer(VulkanDevice* device);
    ~UIRenderer();

    void Render(VkCommandBuffer cmd, UICanvas* canvas);

private:
    void BuildBatch(Texture2D* texture, const glm::vec4& rect, const glm::vec4& uv, const glm::vec4& color);
    void Flush(VkCommandBuffer cmd);

    VulkanDevice* m_Device;

    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescPool = VK_NULL_HANDLE;
    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexMemory = VK_NULL_HANDLE;
    int m_MaxVertices = 0;

    std::vector<UIVertex> m_Vertices;
    std::vector<Texture2D*> m_QuadTextures;
    std::unordered_map<Texture2D*, VkDescriptorSet> m_DescCache;
    Texture2D* m_FallbackTex = nullptr;
};

} // namespace Leir
