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
// INFINITE and always surrounds the camera.
//
// RECURSIVE LOD BY SCREEN DENSITY (Unity-style): the transparency of every
// line is derived from the REAL pixels-per-world-unit of the projection at the
// line's nearest point to the camera, not from a fixed world distance. A line
// of level s (spacing 1/10/100/1000) plays two roles at once:
//   - "fine cell" of size s: visible while a cell of size s is >= ~6px on
//     screen, faded out below ~3px;
//   - "chunk boundary" of the cells s/10 below it: bright while those sub-cells
//     are readable, dimming to a plain fine line once they are too small.
// Because both ramps key off pxPerUnit, zooming out makes each level seamlessly
// hand its chunk role to the next coarser level: near the camera you see 1x1
// squares inside bright 10x10 chunks; zoom out and the 1u internals fade,
// leaving clean 10x10 chunks (which now behave as the "1x1" of the next level);
// zoom further and 100x100 chunks take over, then 1000x1000, etc. Each world
// line is generated exactly once, by its FINEST level (coords a coarser level
// also owns are skipped), so nothing is drawn twice on top of itself. Origin
// axes (red X / blue Z) are opaque and never fade. Lines are clipped at the
// near plane and sorted far-to-near so coplanar overlaps never zipper (same as
// GizmoRenderer).
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
                float viewportWidthPx, float viewportHeightPx,
                float densityOverride = -1.0f);

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
    void GenerateLines(const Leir::Vector3& cameraPos,
                       const Leir::Matrix4x4& viewProjection,
                       float viewportWidthPx, float viewportHeightPx,
                       float densityOverride);
    void EmitLevel(float spacing, const Leir::Matrix4x4& viewProjection,
                   float viewportWidthPx, float viewportHeightPx,
                   const Leir::Vector3& cameraPos, bool parallelToZ,
                   float densityOverride);
    void EmitUniformLevels(float baseSpacing, float pxPerUnit,
                           const Leir::Matrix4x4& viewProjection,
                           float viewportWidthPx, float viewportHeightPx,
                           const Leir::Vector3& cameraPos);

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