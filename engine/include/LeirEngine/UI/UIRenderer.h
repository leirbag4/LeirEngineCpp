#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace Leir {

class VulkanDevice;
class UICanvas;
class UIElement;
class Texture2D;
class RenderTexture;

struct LEIR_API UIVertex {
    Vector2 position;
    Vector2 texCoord;
    Vector4 color;
};

struct LEIR_API ViewportDraw {
    UIVertex verts[4];
    RenderTexture* texture;
    Vector4 clip; // logical clip rect; {0,0,w,h} = full canvas (no clip)
};

class LEIR_API UIRenderer {
public:
    UIRenderer(VulkanDevice* device);
    ~UIRenderer();

    void Render(VkCommandBuffer cmd, UICanvas* canvas);

    // Physical/logical ratio (1.0 when HiDPI disabled). Scissor rects are
    // logical clip rects scaled by this factor.
    void SetContentScale(float scale) { m_ContentScale = scale; }
    float GetContentScale() const { return m_ContentScale; }

private:
    void RenderElement(UIElement* elem, const Vector4* clip, bool isDebug);
    void BuildBatch(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    void BuildBatchDebug(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    void Flush(VkCommandBuffer cmd);
    void ApplyScissor(VkCommandBuffer cmd, const Vector4& logicalClip, VkRect2D& last, bool& valid);

    VulkanDevice* m_Device;

    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescPool = VK_NULL_HANDLE;
    VkBuffer m_VertexBuffers[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceMemory m_VertexMemories[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    int m_MaxVertices = 0;

    std::vector<UIVertex> m_Vertices;
    std::vector<Texture2D*> m_QuadTextures;
    std::vector<Vector4> m_QuadClips;
    std::unordered_map<Texture2D*, VkDescriptorSet> m_DescCache;
    Texture2D* m_FallbackTex = nullptr;

    std::vector<ViewportDraw> m_ViewportDraws;

    std::vector<UIVertex> m_DebugVertices;
    std::vector<Texture2D*> m_DebugQuadTextures;
    std::vector<Vector4> m_DebugQuadClips;

    // Active clip rect during the Render walk (nullptr = full canvas).
    const Vector4* m_CurrentClip = nullptr;
    Vector2 m_ScreenSize = {1280.0f, 720.0f}; // logical canvas size (px→NDC)
    float m_ContentScale = 1.0f;
};

} // namespace Leir
