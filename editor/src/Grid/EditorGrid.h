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

// Unity-style ground grid (Y=0) drawn in the editor viewport using the SAME
// constant-screen-pixel-width line technique as the GizmoRenderer (the one
// validated with the "Test2" gizmo line): every grid line is expanded into a
// screen-width quad in the VERTEX shader (Grid.vert.slang), so one dynamic
// vertex buffer + ONE draw call draws the whole grid each frame.
//
// The lines are GENERATED ON THE CPU every frame, procedurally and centered on
// the camera's XZ (the window recenters each frame) so the grid is effectively
// INFINITE and always surrounds the camera. Minor lines every 1u are
// semi-transparent and fade with the distance from the camera (Unity-style:
// near the camera the fine 1x1 squares are visible inside each 10x10 chunk and
// grow more transparent as they recede / as you zoom out); chunk lines every
// 10u are brighter and reach the horizon. Origin axes (red X / blue Z) are
// opaque and never fade. Lines are clipped at the near plane and sorted
// far-to-near so coplanar overlaps never zipper (same as GizmoRenderer).
class EditorGrid {
public:
    EditorGrid(Leir::RHI::RenderBackend* device,
               Leir::RHI::RHIRenderPass viewportRenderPass);
    ~EditorGrid();

    EditorGrid(const EditorGrid&) = delete;
    EditorGrid& operator=(const EditorGrid&) = delete;

    void Render(Leir::RHI::GCommandGraph& graph,
                const Leir::Matrix4x4& viewProjection,
                const Leir::Vector3& cameraPos,
                float viewportWidthPx, float viewportHeightPx);

private:
    struct Line {
        Leir::Vector3 start;
        Leir::Vector3 end;
        Leir::Vector4 color;
        float width = 1.0f;
    };

    // Layout must match Grid.vert.slang's VSInput (stride 56).
    struct GridVertex {
        Leir::Vector3 start; // 0
        Leir::Vector3 end;   // 12
        Leir::Vector4 color; // 24
        float cornerX = 0.0f; // 40: 0=start / 1=end
        float cornerY = 0.0f; // 44: side +1/-1
        float width = 1.0f;   // 48: line width in pixels
        float pad = 0.0f;     // 52
    };

    // Layout must match Grid.vert.slang's cbuffer UniformBufferObject.
    struct GridUBO {
        Leir::Matrix4x4 viewProjection;
    };

    // Layout must match Grid.vert.slang's GridPushConstants (16 bytes).
    struct GridPushConstants {
        float viewportWidth = 1.0f;
        float viewportHeight = 1.0f;
        float pad0 = 0.0f;
        float pad1 = 0.0f;
    };

    static const int kFrames = 2;
    // The grid is ~300 lines/frame; the cap is generous for far/zoomed-out
    // windows. Each line = 4 strip corners + up to 2 degenerate closers.
    static const uint32_t kMaxLines = 16384;
    static const uint32_t kMaxVertices = kMaxLines * 6;

    void BeginFrame();
    void DrawLine(const Leir::Vector3& a, const Leir::Vector3& b,
                  const Leir::Vector4& color, float widthPx);
    void GenerateLines(const Leir::Vector3& cameraPos);

    void CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass);
    void DestroyResources();

    static Leir::RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<Leir::RHI::RHIVertexAttribute> GetAttributeDescriptions();

    Leir::RHI::RenderBackend* m_Device = nullptr;

    std::vector<Line> m_Lines;
    std::vector<GridVertex> m_Quads;

    Leir::RHI::RHIPipeline m_Pipeline;
    Leir::RHI::RHIPipelineLayout m_PipelineLayout;
    std::vector<Leir::RHISetLayoutEntry> m_SetLayouts;
    Leir::RHI::RHIDescriptorSetLayout m_UBOLayout;
    Leir::RHI::RHIDescriptorPool m_UBOPool;
    Leir::RHI::RHIDescriptorSet m_UBOSets[kFrames];
    Leir::RHI::RHIBuffer m_UBOBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_UBOMemories[kFrames];

    Leir::RHI::RHIBuffer m_VertexBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_VertexMemories[kFrames];
};