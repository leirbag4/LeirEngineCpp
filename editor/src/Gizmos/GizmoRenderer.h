#pragma once

#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"

#include <cstdint>
#include <vector>

namespace Leir {
struct RHISetLayoutEntry;
namespace RHI {
class RenderBackend;
}
} // namespace Leir

// Immediate-mode gizmo renderer: procedural 3D primitives (lines, wireframe
// boxes, circles, spheres) with a constant SCREEN-PIXEL line width.
//
// Every primitive is decomposed into line segments on the CPU; each line is
// expanded into a screen-width quad in the VERTEX shader (Gizmo.vert.slang),
// so one dynamic vertex buffer + ONE draw call per frame draws every gizmo.
class GizmoRenderer {
public:
    GizmoRenderer(Leir::RHI::RenderBackend* device,
                  Leir::RHI::RHIRenderPass viewportRenderPass);
    ~GizmoRenderer();

    GizmoRenderer(const GizmoRenderer&) = delete;
    GizmoRenderer& operator=(const GizmoRenderer&) = delete;

    void BeginFrame();

    void DrawLine(const Leir::Vector3& a, const Leir::Vector3& b,
                  const Leir::Vector4& color, float widthPx = 1.0f);
    void DrawBox(const Leir::Vector3& center, const Leir::Vector3& size,
                 const Leir::Vector4& color, float widthPx = 1.0f);
    void DrawCircle(const Leir::Vector3& center, float radius,
                    const Leir::Vector3& normal, const Leir::Vector4& color,
                    int segments = 48, float widthPx = 1.0f);
    void DrawSphere(const Leir::Vector3& center, float radius,
                    const Leir::Vector4& color, int segments = 32,
                    float widthPx = 1.0f);

    // ---- Solid (filled) primitives ----
    void DrawTriangle(const Leir::Vector3& a, const Leir::Vector3& b,
                      const Leir::Vector3& c, const Leir::Vector4& color);
    void DrawQuadFilled(const Leir::Vector3& a, const Leir::Vector3& b,
                        const Leir::Vector3& c, const Leir::Vector3& d,
                        const Leir::Vector4& color);
    // Wireframe box drawn with LINES (kept for the selection highlight).
    // Filled box (6 faces, mild per-face shading for a readable 3D look).
    void DrawCubeFilled(const Leir::Vector3& center, const Leir::Vector3& size,
                        const Leir::Vector4& color);
    // Filled box oriented by `rotation` (scale-gizmo handle cubes follow the
    // object's rotation, like the translate cones do).
    void DrawCubeFilledOriented(const Leir::Vector3& center, const Leir::Vector3& size,
                                const Leir::Quaternion& rotation,
                                const Leir::Vector4& color);
    // Closed cone (base disc + side), used for the translate arrowheads.
    void DrawCone(const Leir::Vector3& baseCenter, float baseRadius,
                  const Leir::Vector3& tip, const Leir::Vector4& color,
                  int segments = 12);

    int GetLineCount() const { return (int)m_Lines.size(); }
    int GetSolidTriangleCount() const { return (int)(m_SolidVerts.size() / 3); }

    void Render(Leir::RHI::GCommandGraph& graph,
                const Leir::Matrix4x4& viewProjection,
                float viewportWidthPx, float viewportHeightPx);

private:
    struct Line {
        Leir::Vector3 start;
        Leir::Vector3 end;
        Leir::Vector4 color;
        float width = 1.0f;
    };

    // Layout must match Gizmo.vert.slang's VSInput (stride 56).
    struct GizmoVertex {
        Leir::Vector3 start; // 0
        Leir::Vector3 end;   // 12
        Leir::Vector4 color; // 24
        float cornerX = 0.0f; // 40: 0=start / 1=end
        float cornerY = 0.0f; // 44: side +1/-1
        float width = 1.0f;   // 48: line width in pixels
        float pad = 0.0f;     // 52
    };

    // Layout must match Gizmo.vert.slang's cbuffer UniformBufferObject.
    struct GizmoUBO {
        Leir::Matrix4x4 viewProjection;
    };

    // Layout must match Gizmo.vert.slang's GizmoPushConstants (16 bytes).
    struct GizmoPushConstants {
        float viewportWidth = 1.0f;
        float viewportHeight = 1.0f;
        float pad0 = 0.0f;
        float pad1 = 0.0f;
    };

    // Solid vertex: position + color only (GizmoSolid.vert.slang, stride 28).
    struct SolidVertex {
        Leir::Vector3 position;
        Leir::Vector4 color;
    };

    static const int kFrames = 2;
    static const uint32_t kMaxLines = 16384;
    // Each line = 4 strip corners + up to 2 degenerate strip-closers.
    static const uint32_t kMaxVertices = kMaxLines * 6;
    static const uint32_t kMaxSolidVertices = 32768;

    void CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass);
    void CreateSolidPipeline(Leir::RHI::RHIRenderPass viewportRenderPass);
    void DestroyResources();

    static Leir::RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<Leir::RHI::RHIVertexAttribute> GetAttributeDescriptions();
    static Leir::RHI::RHIVertexInputBinding GetSolidBindingDescription();
    static std::vector<Leir::RHI::RHIVertexAttribute> GetSolidAttributeDescriptions();

    Leir::RHI::RenderBackend* m_Device = nullptr;

    std::vector<Line> m_Lines;
    std::vector<GizmoVertex> m_Quads;
    std::vector<SolidVertex> m_SolidVerts;
    bool m_OverflowLogged = false;

    Leir::RHI::RHIPipeline m_Pipeline;
    Leir::RHI::RHIPipelineLayout m_PipelineLayout;
    std::vector<Leir::RHISetLayoutEntry> m_SetLayouts;
    Leir::RHI::RHIDescriptorSetLayout m_UBOLayout;
    Leir::RHI::RHIDescriptorPool m_UBOPool;
    Leir::RHI::RHIDescriptorSet m_UBOSets[kFrames];
    Leir::RHI::RHIBuffer m_UBOBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_UBOMemories[kFrames];

    Leir::RHI::RHIPipeline m_SolidPipeline;
    Leir::RHI::RHIPipelineLayout m_SolidPipelineLayout;

    Leir::RHI::RHIBuffer m_VertexBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_VertexMemories[kFrames];
    Leir::RHI::RHIBuffer m_SolidVertexBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_SolidVertexMemories[kFrames];
};
