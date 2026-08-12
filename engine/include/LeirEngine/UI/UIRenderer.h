#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/RHI/RHI.h"
#include <unordered_map>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }
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

// Per-frame render statistics, read by UIDebugOverlay.
struct LEIR_API UIRenderStats {
    uint32_t quads = 0;        // quads submitted last frame
    uint32_t vertices = 0;     // vertices submitted last frame
    uint32_t drawCalls = 0;    // actual draw calls (after batching)
    uint32_t batches = 0;      // number of merged batches (same texture + scissor)
};

class LEIR_API UIRenderer {
public:
    UIRenderer(RHI::RenderBackend* device);
    ~UIRenderer();

    void Render(RHI::RHICommandBuffer cmd, UICanvas* canvas);

    // Physical/logical ratio (1.0 when HiDPI disabled). Scissor rects are
    // logical clip rects scaled by this factor.
    void SetContentScale(float scale) { m_ContentScale = scale; }
    float GetContentScale() const { return m_ContentScale; }

    // Stats from the last Flush (0 if never rendered).
    const UIRenderStats& GetLastStats() const { return m_LastStats; }

private:
    void RenderElement(UIElement* elem, const Vector4* clip, bool isDebug);
    void BuildBatch(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    void BuildBatchDebug(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    RHI::RHIDescriptorSet GetOrCreateDesc(Texture2D* texture);
    void Flush(RHI::RHICommandBuffer cmd);
    void ApplyScissor(RHI::RHICommandBuffer cmd, const Vector4& logicalClip, RHI::RHIRect2D& last, bool& valid);

    RHI::RenderBackend* m_Device;

    RHI::RHIPipelineLayout m_PipelineLayout;
    RHI::RHIPipeline m_Pipeline;
    RHI::RHIDescriptorSetLayout m_DescSetLayout;
    RHI::RHIDescriptorPool m_DescPool;
    RHI::RHIBuffer m_VertexBuffers[2];
    RHI::RHIDeviceMemory m_VertexMemories[2];
    int m_MaxVertices = 0;

    std::vector<UIVertex> m_Vertices;
    std::vector<Texture2D*> m_QuadTextures;
    std::vector<Vector4> m_QuadClips;
    std::unordered_map<Texture2D*, RHI::RHIDescriptorSet> m_DescCache;
    Texture2D* m_FallbackTex = nullptr;

    std::vector<ViewportDraw> m_ViewportDraws;

    std::vector<UIVertex> m_DebugVertices;
    std::vector<Texture2D*> m_DebugQuadTextures;
    std::vector<Vector4> m_DebugQuadClips;

    // Active clip rect during the Render walk (nullptr = full canvas).
    const Vector4* m_CurrentClip = nullptr;
    Vector2 m_ScreenSize = {1280.0f, 720.0f}; // logical canvas size (px→NDC)
    float m_ContentScale = 1.0f;
    UIRenderStats m_LastStats;
};

} // namespace Leir
