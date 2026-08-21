#include "EditorGrid.h"

#include "LeirEngine/Core/Log.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// Standard strip order for one line quad:
//   corner 0 = start, +side   corner 1 = start, -side
//   corner 2 = end,   +side   corner 3 = end,   -side
// TriangleStrip topology: (0,1,2)(1,2,3) cover the full quad.
constexpr float kCornerX[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
constexpr float kCornerY[4] = { 1.0f, -1.0f, 1.0f, -1.0f };

// The grid lies slightly below Y=0 so scene objects standing on the floor
// occlude the lines underneath them (the pipeline depth-tests against them).
constexpr float kGridY = -0.01f;

// Camera near plane; view depth (clip.w) below this is behind the camera.
constexpr float kNear = 0.1f;

// Recursive LOD by SCREEN DENSITY (pixels per world unit), NOT fixed world
// distance. Each line FADES ALONG ITS LENGTH: it is subdivided and every
// segment dissolves with the density at its depth (clip.w, the distance along
// the camera's forward axis), so a line is bright near the camera and fades
// toward the horizon — in BOTH orientations (the old single-alpha-per-line
// model read one density at the nearest visible point and made vertical lines
// stay bright to the horizon).
// The ROLE of a line (thin/fine vs thick/chunk, i.e. width+color) is chosen
// ONCE PER LEVEL PER FRAME from the ROTATION-INVARIANT reference density
// (scale / camera height, the same value the HUD shows), NEVER from the
// per-point density — otherwise a single chunk line would change style along
// its length (thick that becomes thin mid-line) and the whole chunk set would
// flicker as the camera turns. Only ~2 levels are visible at any zoom, chosen
// by a level's own cell size at the reference density (cellRef = spacing *
// refDensity; live-tunable via SetFadeThresholds):
//   - FINE (minor, dim/thin): cellRef in [fadeStart, 10*fadeStart) — the
//     smallest readable level (Unity keeps it ~20px, already ultra faint);
//   - CHUNK (major, bright/thick): cellRef in [10*fadeStart, 100*fadeStart) —
//     exactly ONE level coarser than the fine one, delimiting its 10x groups.
// A level finer than fine (cellRef < fadeStart) or 2+ steps coarser
// (cellRef >= 100*fadeStart) is not drawn. Both band edges use smoothstep
// ramps so the levels crossfade with zoom: as the fine level recedes it hands
// off to the chunk role while the NEXT-coarser level takes over — no handoff
// gaps, no weak/strong alternation between chunk lines, and never two bright
// chunks stacked in a quadrant (the old per-point model also made EVERY
// readable coarse level a chunk, so at medium zoom the 10u lines stayed
// thick-white next to the camera instead of becoming soft 10u + thick 100u).
constexpr float kMinorMaxAlpha = 0.35f; // role: fine cell of its level
constexpr float kMajorMaxAlpha = 0.55f; // role: chunk line (added)
constexpr float kMinorWidth = 1.5f;     // px; the chunk width is m_ChunkWidth

// Per-line subdivision for the along-length fade (segments in w-space).
constexpr int kSeg = 16;

// Generation window scales with the spacing so coarse levels cover the horizon
// without emitting millions of fine lines; capped near 2x the camera far plane
// (2000) so nothing beyond the visible horizon is ever emitted.
constexpr float kWindowScale = 60.0f; // per-level half-window = 60 * spacing
constexpr float kMaxWindow = 4000.0f;

// Level spacings generated (the recursion): 1u fine, 10u chunks, then 100u...
constexpr float kLevelSpacings[] = { 1.0f, 10.0f, 100.0f, 1000.0f };

// Origin axes reach the horizon at any zoom (>= 2x the far plane).
constexpr float kAxisExtent = 4000.0f;
constexpr float kAxisWidth = 2.0f;      // px

const Leir::Vector4 kMinorColor(0.30f, 0.32f, 0.36f, 1.0f);   // base grid lines
const Leir::Vector4 kMajorColor(0.85f, 0.88f, 0.96f, 1.0f);  // chunk lines, near-white
const Leir::Vector4 kAxisXColor(0.95f, 0.25f, 0.25f, 1.0f);  // red: X axis
const Leir::Vector4 kAxisZColor(0.30f, 0.55f, 1.0f, 1.0f);   // blue: Z axis

// Smooth density ramp (inverse smoothstep): 0 below pxStart, 1 above pxEnd.
float DensityAlpha(float px, float pxStart, float pxEnd)
{
    if (px <= pxStart)
        return 0.0f;
    if (px >= pxEnd)
        return 1.0f;
    const float t = (px - pxStart) / (pxEnd - pxStart);
    return t * t * (3.0f - 2.0f * t);
}

// Rotation-invariant projection scale f = 1/tan(fovY/2), recovered from the
// combined view-projection: row 1 of (P*V) is f * (V's row 1) and V's rotation
// part is orthonormal, so the norm of that row's first three components is
// exactly f regardless of camera yaw/pitch. (The raw element vp[1][1] alone is
// f * V[1][1], which collapses to ~0 when the camera looks straight down at
// pitch -90° — that made the whole grid vanish except the axes.)
float ProjectionScale(const glm::mat4& vp)
{
    return std::sqrt(vp[0][1] * vp[0][1] +
                     vp[1][1] * vp[1][1] +
                     vp[2][1] * vp[2][1]);
}

// Pixels that 1 world unit spans on screen at view depth w (clip.w, the
// distance along the camera's forward axis):
//   pxPerUnit = (viewportHeightPx * 0.5 * f) / w
// View depth is the TRUE perspective depth (glm's w = -z_view in front of the
// camera), so it is rotation-correct at every pitch: straight down (-90°) every
// ground point shares the same depth (= camera height), giving the correct
// UNIFORM density across the whole viewport, and at grazing pitches the
// foreshortened cells right in front of the camera get the large density they
// actually occupy on screen. (The old EUCLIDEAN distance was wrong at grazing
// angles: it underestimated density near the camera and made vertical lines
// vanish.) clip.w is LINEAR along a straight grid line, so EmitLevel can
// subdivide a line's visible w-interval and fade each segment by its own depth.

} // namespace

namespace {

using namespace Leir;

} // namespace

EditorGrid::EditorGrid(Leir::RHI::RenderBackend* device,
                       Leir::RHI::RHIRenderPass viewportRenderPass)
    : m_Device(device)
{
    // Double-buffered dynamic vertex buffer (same pattern as GizmoRenderer): the
    // CPU writes frame N's buffer while the GPU reads frame N-1's.
    const uint32_t vbSize = kMaxVertices * (uint32_t)sizeof(GridVertex);
    for (int f = 0; f < kFrames; ++f) {
        m_VertexBuffers[f] = m_Device->CreateBuffer(vbSize,
            Leir::RHI::BufferUsage::Vertex,
            Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
            m_VertexMemories[f]);
    }
    m_Quads.reserve(4096);

    CreatePipeline(viewportRenderPass);
}

EditorGrid::~EditorGrid()
{
    DestroyResources();
}

void EditorGrid::BeginFrame()
{
    m_Lines.clear();
}

void EditorGrid::SetFadeThresholds(float fadeStartPx, float fadeEndPx)
{
    if (fadeStartPx >= 0.0f && fadeStartPx < fadeEndPx) {
        m_FadeStartPx = fadeStartPx;
        m_FadeEndPx = fadeEndPx;
    }
}

void EditorGrid::DrawLine(const Leir::Vector3& a, const Leir::Vector3& b,
                          const Leir::Vector4& color, float widthPx)
{
    if ((b - a).SqrLength() < 1e-12f)
        return; // degenerate segment: normalize() would divide by zero
    if (m_Lines.size() >= kMaxLines)
        return;
    Line l;
    l.start = a;
    l.end = b;
    l.color = color;
    l.width = widthPx;
    m_Lines.push_back(l);
}

void EditorGrid::GenerateLines(const Leir::Vector3& cameraPos,
                               const Leir::Matrix4x4& viewProjection,
                               float viewportWidthPx, float viewportHeightPx,
                               float densityOverride)
{
    // Debug state for the viewport HUD. Reference px/unit at the point directly
    // below the camera (Euclidean distance = camera height above the grid), so
    // it is rotation-invariant by construction: rotating the camera in place
    // leaves camH, refPxPerUnit and the derived fine/chunk spacing unchanged.
    m_DebugCamHeight = std::max(cameraPos.y - kGridY, 0.1f);
    const glm::mat4 vp = viewProjection;
    m_DebugRefPxPerUnit = viewportHeightPx * 0.5f * ProjectionScale(vp) / m_DebugCamHeight;

    if (densityOverride >= 0.0f) {
        // MANUAL mode (Test2 panel knob): drive the recursive LOD with a single
        // uniform pxPerUnit, decoupled from the camera, so the Unity-style
        // transition can be verified. Identical to camera mode but with every
        // line reading the same density (EmitLevel's override path), so the
        // fine (1u) grid and the chunk levels (10u/100u/1000u) crossfade
        // exactly like the real camera projection. Only the levels whose cells
        // are still readable on screen emit lines (L, 10L, 100L, ...).
        m_DebugRefPxPerUnit = densityOverride;
        for (float spacing : kLevelSpacings) {
            EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                      cameraPos, true, densityOverride, densityOverride);  // lines parallel to Z
            EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                      cameraPos, false, densityOverride, densityOverride); // lines parallel to X
        }
        m_DebugLineCount = (uint32_t)m_Lines.size();
        ComputeDebugSpacing();
        return;
    }

    // CAMERA mode: one pass per recursion level. Each world line belongs to
    // exactly one level (its FINEST: coords a coarser level also owns are
    // skipped inside EmitLevel), so nothing is drawn twice on top of itself.
    for (float spacing : kLevelSpacings) {
        EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                  cameraPos, true, densityOverride, m_DebugRefPxPerUnit);  // lines parallel to Z (x = coord)
        EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                  cameraPos, false, densityOverride, m_DebugRefPxPerUnit); // lines parallel to X (z = coord)
    }

    // Origin axes (opaque, never fade, reach the horizon at any zoom).
    DrawLine({ -kAxisExtent, kGridY, 0.0f }, { kAxisExtent, kGridY, 0.0f }, kAxisXColor, kAxisWidth);
    DrawLine({ 0.0f, kGridY, -kAxisExtent }, { 0.0f, kGridY, kAxisExtent }, kAxisZColor, kAxisWidth);

    m_DebugLineCount = (uint32_t)m_Lines.size();
    ComputeDebugSpacing();
}

// Derive the active fine/chunk recursion levels from the reference density so
// the HUD shows the same LOD numbers the grid actually picks. This mirrors
// EmitLevel's ramp: the fine spacing is the smallest power of 10 whose cells
// are still readable on screen.
void EditorGrid::ComputeDebugSpacing()
{
    float spacing = 1.0f;
    while (spacing < 10000.0f && spacing * m_DebugRefPxPerUnit < m_FadeStartPx)
        spacing *= 10.0f;
    m_DebugFineSpacing = spacing;
    m_DebugChunkSpacing = spacing * 10.0f;
}

void EditorGrid::EmitLevel(float spacing, const Leir::Matrix4x4& viewProjection,
                           float viewportWidthPx, float viewportHeightPx,
                           const Leir::Vector3& cameraPos, bool parallelToZ,
                           float densityOverride, float refDensity)
{
    // The coordinate that varies along the line (the line is drawn parallel to
    // the other axis, perpendicular to `coord`). `coord` steps by `spacing`.
    const float camCoord = parallelToZ ? cameraPos.x : cameraPos.z;
    const float camPerp = parallelToZ ? cameraPos.z : cameraPos.x;
    const float window = std::min(kWindowScale * spacing, kMaxWindow);
    const int i0 = (int)std::floor((camCoord - window) / spacing);
    const int i1 = (int)std::ceil((camCoord + window) / spacing);
    const glm::mat4 vp = viewProjection;
    (void)viewportWidthPx;

    const float scale = viewportHeightPx * 0.5f * ProjectionScale(vp);

    // ROLE of this level, computed ONCE per frame from the ROTATION-INVARIANT
    // reference density (scale / camera height) — see the model comment at the
    // top. This is the level's width/color/alpha, constant for every line of
    // the level; the per-segment density below only fades each line's alpha.
    const float cellRef = spacing * refDensity;
    const float minorVis =
        DensityAlpha(cellRef, m_FadeStartPx, m_FadeEndPx)
        * (1.0f - DensityAlpha(cellRef, 10.0f * m_FadeStartPx,
                               10.0f * m_FadeEndPx));
    const float chunkVis = (spacing >= 10.0f)
        ? DensityAlpha(cellRef, 10.0f * m_FadeStartPx,
                       10.0f * m_FadeEndPx)
          * (1.0f - DensityAlpha(cellRef, 100.0f * m_FadeStartPx,
                                 100.0f * m_FadeEndPx))
        : 0.0f;
    const float levelAlpha = kMinorMaxAlpha * minorVis + kMajorMaxAlpha * chunkVis;

    int levelIndex = 0;
    for (float s = spacing; s >= 10.0f; s /= 10.0f)
        ++levelIndex;
    m_DebugLevelAlphas[levelIndex] = levelAlpha;
    if (levelAlpha < 0.02f)
        return; // this level is not part of the current LOD

    const float levelWidth = kMinorWidth + (m_ChunkWidth - kMinorWidth) * chunkVis;
    const Leir::Vector4 levelColor = Leir::Vector4::Lerp(kMinorColor, kMajorColor, chunkVis);

    // Beyond wMax the cell size is below fadeStartPx (the line is fully faded),
    // so nothing past it needs emitting; within wFull everything is visible.
    const float wMax = scale * spacing / m_FadeStartPx;
    const float wFull = scale * spacing / m_FadeEndPx;

    for (int i = i0; i <= i1; ++i) {
        const float coord = (float)i * spacing;
        if (coord == 0.0f)
            continue; // the axis covers the origin line
        if (i % 10 == 0)
            continue; // a coarser level owns this line (drawn exactly once)

        const glm::vec3 p0 = parallelToZ
            ? glm::vec3(coord, kGridY, camPerp - window)
            : glm::vec3(camPerp - window, kGridY, coord);
        const glm::vec3 p1 = parallelToZ
            ? glm::vec3(coord, kGridY, camPerp + window)
            : glm::vec3(camPerp + window, kGridY, coord);

        // clip.w (view depth) is LINEAR along the straight span, so the part in
        // front of the near plane is one contiguous w-interval. `scale / w` is
        // the px-per-unit density at that depth (view depth is rotation-correct
        // at every pitch: straight down every ground point shares depth = cam
        // height, giving a uniform density; at grazing pitches the foreshortened
        // cells right in front get the large density they really occupy).
        const glm::vec4 clip0 = vp * glm::vec4(p0, 1.0f);
        const glm::vec4 clip1 = vp * glm::vec4(p1, 1.0f);
        const float w0 = clip0.w;
        const float w1 = clip1.w;
        const float wNear = std::max(std::min(w0, w1), kNear);
        float wFar = std::max(w0, w1);
        if (densityOverride < 0.0f)
            wFar = std::min(wFar, wMax); // camera mode: fade out at this depth
        if (wFar <= wNear)
            continue; // behind the near plane or past the fade-out

        // Emit one segment of the line, mapped back to world space from its
        // w-window (w is linear along the span). The segment only FADES the
        // alpha by the density at its midpoint (the line dissolves as it
        // recedes); the style (width/color) is the per-level role computed
        // above and is CONSTANT along the line, so a chunk line never turns
        // into a plain line mid-way or with the view direction.
        auto emitSegment = [&](float segStartW, float segEndW, float density) {
            const float fade = DensityAlpha(spacing * density,
                                            m_FadeStartPx, m_FadeEndPx);
            const float alpha = levelAlpha * fade;
            if (alpha < 0.02f)
                return;
            Leir::Vector4 c = levelColor;
            c.w = alpha;
            float t0;
            float t1;
            if (std::abs(w1 - w0) < 1e-6f) {
                t0 = 0.0f; // constant-depth line: the span maps to the whole line
                t1 = 1.0f;
            } else {
                t0 = (segStartW - w0) / (w1 - w0);
                t1 = (segEndW - w0) / (w1 - w0);
            }
            const glm::vec3 a = glm::mix(p0, p1, t0);
            const glm::vec3 b = glm::mix(p0, p1, t1);
            DrawLine({ a.x, a.y, a.z }, { b.x, b.y, b.z }, c, levelWidth);
        };

        if (densityOverride >= 0.0f) {
            // MANUAL mode (Test2 panel knob): uniform density over the whole
            // line, decoupled from the camera, so the Unity-style transition
            // can be verified without touching the camera.
            emitSegment(wNear, wFar, densityOverride);
            continue;
        }

        // Fully-bright shortcut: the whole visible part is beyond fadeEndPx of
        // cell size -> one draw, no subdivision needed.
        if (wFar <= wFull) {
            emitSegment(wNear, wFar, scale / (0.5f * (wNear + wFar)));
            continue;
        }

        // Constant-depth line (w does not vary along the span, e.g. a line
        // parallel to the camera's forward axis — the common horizontal case):
        // subdivision in w-space is meaningless, draw it at that single depth.
        if (std::abs(w1 - w0) < 1e-6f) {
            emitSegment(wNear, wFar, scale / wNear);
            continue;
        }

        // Fade along the line: subdivide the visible w-interval so each segment
        // is styled by the density at ITS depth — near the camera the bright
        // chunk role, fading out toward the horizon.
        const float dw = (wFar - wNear) / kSeg;
        for (int s = 0; s < kSeg; ++s) {
            const float aW = wNear + (float)s * dw;
            const float bW = aW + dw;
            emitSegment(aW, bW, scale / (0.5f * (aW + bW)));
        }
    }
}

void EditorGrid::Render(Leir::RHI::GCommandGraph& graph,
                        const Leir::Matrix4x4& viewProjection,
                        const Leir::Vector3& cameraPos,
                        float viewportWidthPx, float viewportHeightPx,
                        float densityOverride)
{
    if (!m_Device || !m_Pipeline.IsValid())
        return;

    const uint32_t frame = m_Device->GetCurrentFrameIndex();

    // Procedural grid, regenerated every frame and recentered on the camera.
    BeginFrame();
    GenerateLines(cameraPos, viewProjection, viewportWidthPx, viewportHeightPx,
                  densityOverride);
    if (m_Lines.empty())
        return;

    GridUBO ubo;
    ubo.viewProjection = viewProjection;
    void* data = nullptr;
    m_Device->MapMemory(m_UBOMemories[frame], 0, (uint32_t)sizeof(GridUBO), &data);
    std::memcpy(data, &ubo, sizeof(GridUBO));
    m_Device->UnmapMemory(m_UBOMemories[frame]);

    // Expand lines into quads (identical to GizmoRenderer::Render): clip
    // segments at the near plane, sort far-to-near (the pipeline has depth
    // WRITE disabled, so draw order decides overlaps and the NEAREST coplanar
    // line must win the shared pixels), then emit each quad + strip-restart
    // degenerates so consecutive quads don't stitch a spurious triangle.
    constexpr float kNearClip = 0.1f; // matches the camera near plane
    struct DrawnSeg {
        glm::vec3 s;
        glm::vec3 e;
        Leir::Vector4 color;
        float width;
        float key; // closest clip.w (= -view z) of the clipped segment
    };
    std::vector<DrawnSeg> segs;
    segs.reserve(m_Lines.size());
    const glm::mat4 vp = ubo.viewProjection;
    for (const auto& line : m_Lines) {
        const glm::vec3 s(line.start);
        const glm::vec3 e(line.end);
        const glm::vec4 clipS = vp * glm::vec4(s, 1.0f);
        const glm::vec4 clipE = vp * glm::vec4(e, 1.0f);
        glm::vec3 sClipped = s;
        glm::vec3 eClipped = e;
        if (clipS.w <= kNearClip && clipE.w <= kNearClip)
            continue;
        if (clipS.w <= kNearClip) {
            const float t = (kNearClip - clipS.w) / (clipE.w - clipS.w);
            sClipped = glm::mix(s, e, t);
        } else if (clipE.w <= kNearClip) {
            const float t = (kNearClip - clipS.w) / (clipE.w - clipS.w);
            eClipped = glm::mix(s, e, t);
        }
        DrawnSeg seg;
        seg.s = sClipped;
        seg.e = eClipped;
        seg.color = line.color;
        seg.width = line.width;
        seg.key = std::min(clipS.w, clipE.w); // w = -view z: smaller = nearer
        segs.push_back(seg);
    }
    std::sort(segs.begin(), segs.end(),
        [](const DrawnSeg& a, const DrawnSeg& b) { return a.key < b.key; });

    m_Quads.clear();
    for (size_t li = 0; li < segs.size(); ++li) {
        const auto& seg = segs[li];
        if (m_Quads.size() + 6 > kMaxVertices)
            break;
        for (int c = 0; c < 4; ++c) {
            GridVertex v;
            v.start = seg.s;
            v.end = seg.e;
            v.color = seg.color;
            v.cornerX = kCornerX[c];
            v.cornerY = kCornerY[c];
            v.width = seg.width;
            m_Quads.push_back(v);
        }
        if (li + 1 < segs.size()) {
            // Strip restart: repeat this quad's last corner (A3), then the next
            // quad's first corner (B0). Both are zero-area connectors.
            const auto& next = segs[li + 1];
            GridVertex a3;
            a3.start = seg.s;
            a3.end = seg.e;
            a3.color = seg.color;
            a3.cornerX = kCornerX[3];
            a3.cornerY = kCornerY[3];
            a3.width = seg.width;
            m_Quads.push_back(a3);

            GridVertex b0;
            b0.start = next.s;
            b0.end = next.e;
            b0.color = next.color;
            b0.cornerX = kCornerX[0];
            b0.cornerY = kCornerY[0];
            b0.width = next.width;
            m_Quads.push_back(b0);
        }
    }
    if (m_Quads.empty())
        return;

    const uint32_t vbBytes = (uint32_t)(m_Quads.size() * sizeof(GridVertex));
    m_Device->MapMemory(m_VertexMemories[frame], 0, vbBytes, &data);
    std::memcpy(data, m_Quads.data(), vbBytes);
    m_Device->UnmapMemory(m_VertexMemories[frame]);

    graph.BindPipeline(m_Pipeline);
    graph.BindDescriptorSets(m_PipelineLayout, 0, { m_UBOSets[frame] });
    graph.BindVertexBuffer(m_VertexBuffers[frame]);

    GridPushConstants push;
    push.viewportWidth = viewportWidthPx;
    push.viewportHeight = viewportHeightPx;
    graph.PushConstants(m_PipelineLayout, Leir::RHI::ShaderStageMask::Vertex,
        0, (uint32_t)sizeof(GridPushConstants), &push);

    graph.Draw((uint32_t)m_Quads.size(), 0);
}

void EditorGrid::CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass)
{
    const std::string shaderDir = LEIR_SHADER_DIR;
    const std::string ext = m_Device->GetShaderFileExtension();
    const std::string vertPath = shaderDir + "/Grid.vert" + ext;
    const std::string fragPath = shaderDir + "/Grid.frag" + ext;

    // Pipeline layouts derived from the reflection sidecars (written by the
    // editor shader tooling in OnInit). Falls back to a hand-written layout
    // when the sidecars are missing.
    const Leir::RHI::ShaderReflection reflection =
        Leir::LoadShaderReflectionFromSidecars({ vertPath, fragPath });
    if (!reflection.bindings.empty()) {
        m_SetLayouts = Leir::CreateSetLayoutsFromReflection(m_Device, reflection);
        if (!m_SetLayouts.empty())
            m_UBOLayout = m_SetLayouts[0].layout; // set 0: UBO
        m_PipelineLayout = Leir::CreatePipelineLayoutFromReflection(
            m_Device, reflection, m_SetLayouts);
    } else {
        Leir::RHI::RHIDescriptorBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.type = Leir::RHI::DescriptorType::UniformBuffer;
        uboBinding.count = 1;
        uboBinding.stage = Leir::RHI::ShaderStage::Vertex;
        m_UBOLayout = m_Device->CreateDescriptorSetLayout({ uboBinding });
        m_SetLayouts = { { 0, m_UBOLayout } };

        Leir::RHI::RHIPushConstantRange pushRange{};
        pushRange.stage = Leir::RHI::ShaderStageMask::Vertex;
        pushRange.offset = 0;
        pushRange.size = (uint32_t)sizeof(GridPushConstants);
        m_PipelineLayout = m_Device->CreatePipelineLayout({ m_UBOLayout }, { pushRange });
    }

    // UBO descriptor pool + per-frame sets.
    Leir::RHI::RHIDescriptorBinding poolBinding{};
    poolBinding.binding = 0;
    poolBinding.type = Leir::RHI::DescriptorType::UniformBuffer;
    poolBinding.count = kFrames;
    poolBinding.stage = Leir::RHI::ShaderStage::Vertex;
    m_UBOPool = m_Device->CreateDescriptorPool({ poolBinding }, kFrames);

    for (int i = 0; i < kFrames; ++i) {
        m_UBOBuffers[i] = m_Device->CreateBuffer((uint32_t)sizeof(GridUBO),
            Leir::RHI::BufferUsage::Uniform,
            Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
            m_UBOMemories[i]);
        m_UBOSets[i] = m_Device->AllocateDescriptorSet(m_UBOPool, m_UBOLayout);

        Leir::RHI::RHIDescriptorWrite write{};
        write.dstSet = m_UBOSets[i];
        write.dstBinding = 0;
        write.count = 1;
        write.type = Leir::RHI::DescriptorType::UniformBuffer;
        write.bufferInfo.buffer = m_UBOBuffers[i];
        write.bufferInfo.offset = 0;
        write.bufferInfo.range = (uint32_t)sizeof(GridUBO);
        write.bufferInfo.valid = true;
        m_Device->WriteDescriptorSets({ write });
    }

    // Graphics pipeline against the viewport RenderTexture render pass.
    auto vertCode = Leir::Shader::ReadFile(vertPath);
    auto fragCode = Leir::Shader::ReadFile(fragPath);
    Leir::RHI::RHIShaderModule vertMod = m_Device->CreateShaderModule(vertCode);
    Leir::RHI::RHIShaderModule fragMod = m_Device->CreateShaderModule(fragCode);

    Leir::RHI::RHIShaderStageInfo stages[2];
    stages[0].stage = Leir::RHI::ShaderStage::Vertex;
    stages[0].module = vertMod;
    stages[0].entryPoint = "main";
    stages[1].stage = Leir::RHI::ShaderStage::Fragment;
    stages[1].module = fragMod;
    stages[1].entryPoint = "main";

    Leir::RHI::RHIPipelineDesc desc{};
    desc.layout = m_PipelineLayout;
    desc.renderPass = viewportRenderPass;
    desc.stages = { stages[0], stages[1] };
    desc.vertexBinding = GetBindingDescription();
    desc.vertexAttributes = GetAttributeDescriptions();
    desc.topology = Leir::RHI::Topology::TriangleStrip;
    desc.polygonMode = Leir::RHI::PolygonMode::Fill;
    desc.cullMode = Leir::RHI::CullMode::None;
    // Depth test against the scene (objects occlude grid lines underneath);
    // no depth WRITE so grid lines never hide the scene or each other
    // (later-drawn wins overlaps -> the far-to-near sort decides).
    desc.depthTestEnable = true;
    desc.depthWriteEnable = false;
    desc.blend.enable = true;
    m_Pipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);

    Leir::XConsole::Println("Editor grid pipeline created ({})", ext);
}

Leir::RHI::RHIVertexInputBinding EditorGrid::GetBindingDescription()
{
    Leir::RHI::RHIVertexInputBinding binding;
    binding.binding = 0;
    binding.stride = sizeof(GridVertex); // 56
    binding.inputRate = Leir::RHI::VertexInputRate::Vertex;
    return binding;
}

std::vector<Leir::RHI::RHIVertexAttribute> EditorGrid::GetAttributeDescriptions()
{
    std::vector<Leir::RHI::RHIVertexAttribute> attrs(6);

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(GridVertex, start);
    attrs[0].semantic = "POSITION";
    attrs[0].semanticIndex = 0;

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(GridVertex, end);
    attrs[1].semantic = "TEXCOORD";
    attrs[1].semanticIndex = 0;

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = Leir::RHI::Format::R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(GridVertex, color);
    attrs[2].semantic = "COLOR";
    attrs[2].semanticIndex = 0;

    attrs[3].binding = 0;
    attrs[3].location = 3;
    attrs[3].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[3].offset = offsetof(GridVertex, cornerX);
    attrs[3].semantic = "TEXCOORD";
    attrs[3].semanticIndex = 1;

    attrs[4].binding = 0;
    attrs[4].location = 4;
    attrs[4].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[4].offset = offsetof(GridVertex, cornerY);
    attrs[4].semantic = "TEXCOORD";
    attrs[4].semanticIndex = 2;

    attrs[5].binding = 0;
    attrs[5].location = 5;
    attrs[5].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[5].offset = offsetof(GridVertex, width);
    attrs[5].semantic = "TEXCOORD";
    attrs[5].semanticIndex = 3;

    return attrs;
}

void EditorGrid::DestroyResources()
{
    if (m_Pipeline.IsValid())
        m_Device->DestroyPipeline(m_Pipeline);
    if (m_PipelineLayout.IsValid())
        m_Device->DestroyPipelineLayout(m_PipelineLayout);
    for (auto& entry : m_SetLayouts) {
        if (entry.layout.IsValid())
            m_Device->DestroyDescriptorSetLayout(entry.layout);
    }
    if (m_UBOPool.IsValid())
        m_Device->DestroyDescriptorPool(m_UBOPool);

    for (int i = 0; i < kFrames; ++i) {
        if (m_UBOBuffers[i].IsValid())
            m_Device->DestroyBuffer(m_UBOBuffers[i]);
        if (m_UBOMemories[i].IsValid())
            m_Device->DestroyMemory(m_UBOMemories[i]);
        if (m_VertexBuffers[i].IsValid())
            m_Device->DestroyBuffer(m_VertexBuffers[i]);
        if (m_VertexMemories[i].IsValid())
            m_Device->DestroyMemory(m_VertexMemories[i]);
    }
}