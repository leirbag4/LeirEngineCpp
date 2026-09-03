#pragma once

#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"

#include <algorithm>
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
// line fades with the REAL pixels-per-world-unit of the projection at the
// line's OWN view depth (clip.w), so each line FADES ALONG ITS LENGTH — bright
// near the camera, gone toward the horizon, in both orientations. The fade is
// computed PER PIXEL in the FRAGMENT shader ("fog by depth") from the
// interpolated view depth, so each line is a SINGLE quad (no CPU subdivision).
// The ROLE (thin/fine vs thick/chunk, i.e. width+color) is chosen ONCE PER
// LEVEL PER FRAME from the rotation-invariant reference density (scale / camera
// height) and never from the per-point density, so styles are stable under
// yaw/pitch. Only ~2 levels are visible at any zoom, chosen by a level's own
// cell size at the reference density:
//   - level 1u = "fine" cell: dim thin line while a 1u cell is >= ~30px on
//     screen, faded out below ~15px (the smallest visible square is ~20px and
//     already ultra faint, so sub-pixel micro-squares never become solid);
//   - the single "chunk" level (one coarser than the fine one): a bright thick
//     line delimiting 10x groups of the fine cells, staying thick along its
//     whole length and handing the chunk role to the next coarser level as the
//     camera zooms out. Levels 2+ steps coarser or finer than fine are not
//     drawn, so two bright chunks never stack in a quadrant.
// Because each level fades by its own cell density, zooming out just fades the
// fine internals inside persistent bright chunks and then the chunks themselves
// fade as they recede — no handoff gaps and no weak/strong alternation between
// chunk lines. Only ~2 spacings are visible at any screen location (the current
// fine level + the chunk grid). Each world line is generated exactly once, by
// its FINEST level (coords a coarser level also owns are skipped), so nothing
// is drawn twice on top of itself. Origin axes (red X / blue Z) are opaque and
// never fade. Lines are clipped at the near plane and sorted far-to-near so
// coplanar overlaps never zipper (same as GizmoRenderer).
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

    // Live-tunable cell-fade thresholds (Test2 panel): a line's role (fine cell
    // or chunk boundary) is fully hidden below fadeStartPx of cell size and
    // fully visible above fadeEndPx. Defaults are Unity-like (15/30 px) so the
    // smallest visible square is ~20px and already faint — sub-pixel micro
    // squares never turn into solid blocks and only 2 levels are ever visible.
    void SetFadeThresholds(float fadeStartPx, float fadeEndPx);
    float GetFadeStartPx() const { return m_FadeStartPx; }
    float GetFadeEndPx() const { return m_FadeEndPx; }

    // Live-tunable chunk-line width in pixels (Test2 panel knob). The fine
    // lines are always kMinorWidth; chunk lines lerp up to this value while
    // they hold the bright/thick role.
    void SetChunkWidth(float widthPx) { m_ChunkWidth = std::max(0.5f, widthPx); }
    float GetChunkWidth() const { return m_ChunkWidth; }

    // Horizon fade (Test2 knobs): a second, distance-based fade applied to
    // EVERY segment, `fade = min(cellSizeFade, horizonFade)`. It dissolves all
    // lines to alpha 0 as their view depth approaches `end` (kept below the
    // camera far plane 2000), so coarse chunk lines like the 100u — whose
    // natural cell-size fade band extends PAST the far plane — no longer reach
    // the horizon at full brightness and get hard-clipped into a solid. Fine
    // levels (1u/10u) fade out much earlier via their own cell-size fade, so
    // this min() is a no-op for them. View depths in world units.
    void SetHorizonFade(float start, float end)
    {
        if (start >= 0.0f && start < end) {
            m_HorizonFadeStart = start;
            m_HorizonFadeEnd = end;
        }
    }
    float GetHorizonFadeStart() const { return m_HorizonFadeStart; }
    float GetHorizonFadeEnd() const { return m_HorizonFadeEnd; }

    // Debug state for the viewport HUD (the "LOD debug" label): how the grid
    // reacts to camera motion. Cam height is the camera's height above the grid
    // plane; ref px/unit is the pixel density at the point directly below the
    // camera (rotation-invariant by construction: it uses the EUCLIDEAN camera
    // distance). Fine/chunk spacing are the active recursion levels derived
    // from that reference density; line count is the number of lines emitted
    // this frame.
    float GetDebugCamHeight() const { return m_DebugCamHeight; }
    float GetDebugRefPxPerUnit() const { return m_DebugRefPxPerUnit; }
    float GetDebugFineSpacing() const { return m_DebugFineSpacing; }
    float GetDebugChunkSpacing() const { return m_DebugChunkSpacing; }
    uint32_t GetDebugLineCount() const { return m_DebugLineCount; }
    // Per-level role alpha for the HUD readout (index 0=1u, 1=10u, 2=100u,
    // 3=1000u). This is the level's visibility alpha (fine 0.35 max / chunk
    // 0.55 max), computed from the rotation-invariant reference density, so it
    // is STABLE under yaw/pitch and only changes as the camera zooms.
    float GetDebugLevelAlpha(int index) const { return m_DebugLevelAlphas[index]; }

    // Diagnostic knobs (controlled from the GridPanel).
    void SetChunkOnly(bool on) { m_ChunkOnly = on; }
    bool IsChunkOnly() const { return m_ChunkOnly; }
    void SetDisableClip(bool on) { m_DisableClip = on; }
    bool IsDisableClip() const { return m_DisableClip; }
    void SetThinChunks(bool on) { m_ThinChunks = on; }
    bool IsThinChunks() const { return m_ThinChunks; }
    // Per-level visibility mask (bit li = level index 0..3: 1u,10u,100u,1000u).
    void SetLevelEnabled(int li, bool on);
    bool IsLevelEnabled(int li) const { return (m_LevelMask & (1u << li)) != 0; }
    uint32_t GetNanSkippedCount() const { return m_NanSkippedCount; }
    uint32_t GetQuadCount() const { return (uint32_t)m_Quads.size(); }
    uint32_t GetSegCount() const { return (uint32_t)m_SegCount; }
    static constexpr uint32_t kMaxQuads = 16384 * 6; // kMaxLines * 6

private:
    struct Line {
        Leir::Vector3 start;
        Leir::Vector3 end;
        Leir::Vector4 color;
        float width = 1.0f;
        // Level spacing of this line (world units, power of 10). Carried to the
        // fragment shader via the vertex to compute the per-pixel cell-size
        // fade. 0 = opaque reference line (origin axes): the shader skips the
        // distance fade for them.
        float spacing = 1.0f;
    };

    // Per-level LOD style, computed ONCE per frame from the rotation-invariant
    // reference density (see EmitAllLevels/ComputeLevelRole): the level's
    // constant width/color and a visibility alpha. `active` = alpha is above
    // the draw threshold, so the whole level (both orientations) is skipped
    // before any line is emitted.
    struct LevelRole {
        bool active = false;
        float alpha = 0.0f;
        float width = 1.0f;
        Leir::Vector4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    // Layout must match Grid.vert.slang's VSInput (stride 64).
    struct GridVertex {
        Leir::Vector3 start;  // 0
        Leir::Vector3 end;    // 12
        Leir::Vector4 color;  // 24
        float cornerX = 0.0f; // 40: 0=start / 1=end
        float cornerY = 0.0f; // 44: side +1/-1
        float width = 1.0f;   // 48: line width in pixels
        float pad = 0.0f;     // 52
        float spacing = 1.0f; // 56: level spacing for the fragment fade (0=opaque)
        float pad1 = 0.0f;    // 60
    };

    // Layout must match Grid.vert.slang's cbuffer UniformBufferObject.
    struct GridUBO {
        Leir::Matrix4x4 viewProjection;
    };

    // Layout must match Grid.vert.slang / Grid.frag.slang's GridPushConstants
    // (32 bytes). The fragment shader reads scale + the fade bands to compute
    // the per-pixel distance fade (fog by depth).
    struct GridPushConstants {
        float viewportWidth = 1.0f;
        float viewportHeight = 1.0f;
        float scale = 1.0f;            // px per unit at depth 1 (vh * 0.5 * f)
        float fadeStart = 15.0f;       // cell-size fade band (px of cell)
        float fadeEnd = 30.0f;
        float horizonStart = 1000.0f;  // horizon fade band (view depth, units)
        float horizonEnd = 1800.0f;
        float overrideDensity = -1.0f; // >=0: manual mode (uniform density)
    };

    static const int kFrames = 3;     // independent frame counter (not tied to the swapchain)
    int m_WriteFrame = 0;             // own frame counter, advances every Render() call
    // The grid is ~300-600 lines/frame (1 quad per line; the fade is per-pixel
    // in the shader, no CPU subdivision); the cap is generous for far/zoomed-out
    // windows. Each line = 4 strip corners + up to 2 degenerate closers.
    static const uint32_t kMaxLines = 16384;
    static const uint32_t kMaxVertices = kMaxLines * 6;

    void BeginFrame();
    void DrawLine(const Leir::Vector3& a, const Leir::Vector3& b,
                  const Leir::Vector4& color, float widthPx,
                  float spacing = 1.0f);
    void ComputeDebugSpacing();
    void GenerateLines(const Leir::Vector3& cameraPos,
                       const Leir::Matrix4x4& viewProjection,
                       float viewportHeightPx,
                       float densityOverride);
    void EmitAllLevels(float refDensity, float densityOverride,
                       const Leir::Matrix4x4& viewProjection,
                       float viewportHeightPx, const Leir::Vector3& cameraPos);
    LevelRole ComputeLevelRole(float spacing, float refDensity) const;
    void EmitLevel(float spacing, const Leir::Matrix4x4& viewProjection,
                   float viewportHeightPx, const Leir::Vector3& cameraPos,
                   bool parallelToZ, float densityOverride,
                   const LevelRole& role);

    void CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass);
    void DestroyResources();

    static Leir::RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<Leir::RHI::RHIVertexAttribute> GetAttributeDescriptions();

    Leir::RHI::RenderBackend* m_Device = nullptr;

    std::vector<Line> m_Lines;
    std::vector<GridVertex> m_Quads;

    float m_DebugCamHeight = 0.0f;
    float m_DebugRefPxPerUnit = 0.0f;
    float m_DebugFineSpacing = 0.0f;
    float m_DebugChunkSpacing = 0.0f;
    uint32_t m_DebugLineCount = 0;
    float m_DebugLevelAlphas[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    float m_FadeStartPx = 15.0f; // below: a line's role is invisible
    float m_FadeEndPx = 30.0f;   // above: fully visible in that role
    float m_ChunkWidth = 0.9f;   // px, chunk (thick) line width
    // Horizon fade band in view depth (world units): lines dissolve to alpha 0
    // from start to end, so nothing reaches the camera far plane (2000) bright.
    float m_HorizonFadeStart = 1000.0f;
    float m_HorizonFadeEnd = 1800.0f;

    // Diagnostic flags and counters (controlled from GridPanel).
    bool m_ChunkOnly = false;
    bool m_DisableClip = false;
    bool m_ThinChunks = false; // diagnostic: force chunk width = fine width
    uint32_t m_NanSkippedCount = 0;
    uint32_t m_LevelMask = 0xFu; // all levels on by default
    uint32_t m_SegCount = 0;

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