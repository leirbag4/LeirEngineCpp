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

// Recursive LOD by SCREEN DENSITY (pixels per world unit), NOT fixed world
// distance: every line's alpha/color/width derives from the REAL projection
// pxPerUnit read at the line's nearest point to the camera. A line of level s
// (spacing 1/10/100/1000) plays two roles at once:
//   - fine cell boundary of size s        -> faded once a cell of size s drops
//     below ~3px on screen (ramp 3..6px);
//   - chunk boundary of the cells s/10    -> bright while those sub-cells are
//     readable, dimming to a plain fine line once too small.
// Zooming out makes each level hand its chunk role to the next coarser level
// seamlessly: near the camera 1x1 squares inside bright 10x10 chunks; zoom out
// and the 1u internals vanish leaving clean 10x10 chunks ("1x1 virtual" of the
// next level); zoom further and 100x100 take over, then 1000x1000, etc.
constexpr float kMinorMaxAlpha = 0.35f; // role: fine cell of its level
constexpr float kMajorMaxAlpha = 0.55f; // role: chunk boundary (added)
constexpr float kMinorWidth = 1.5f;     // px
constexpr float kMajorWidth = 3.0f;     // px
constexpr float kCellPxFadeStart = 3.0f; // below: line invisible in that role
constexpr float kCellPxFadeEnd = 6.0f;   // above: fully visible in that role

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

// Pixels that 1 world unit spans on screen at the given ground point, using the
// real view-projection scale but the EUCLIDEAN distance from the camera to the
// point (NOT the view depth clip.w):
//   pxPerUnit = (viewportHeightPx * 0.5 * f) / dist(camera, groundPt)
// Euclidean distance depends only on the camera POSITION, never its
// orientation, so rotating the camera in place never changes the LOD — this is
// how professional engines (Unity et al.) drive grid fades. Monotonic with
// distance, so it never blows up at grazing angles either. Returns 0 when the
// point is at or behind the near plane.
float PxPerUnit(const glm::mat4& vp, float viewportWidthPx, float viewportHeightPx,
                const glm::vec3& cameraPos, const glm::vec3& groundPt)
{
    const glm::vec4 clip = vp * glm::vec4(groundPt, 1.0f);
    constexpr float kNear = 0.1f; // matches the camera near plane
    if (clip.w <= kNear)
        return 0.0f;
    const float dist = glm::length(cameraPos - groundPt);
    if (dist <= kNear)
        return 0.0f;
    const float scale = viewportHeightPx * 0.5f * ProjectionScale(vp);
    return scale / dist;
}

// Max pixels-per-world-unit across sample points along a grid line (which spans
// camPerp +/- window on the perpendicular axis). Sampling avoids reading the
// density at a single point that may sit exactly on the camera plane (pitch ~0,
// where pxPerUnit is meaningless) or behind it: only points in front of the
// near plane contribute, and the closest one wins (the largest pxPerUnit along
// the line). Returns 0 when the whole line is behind the camera.
float LinePxPerUnit(const glm::mat4& vp, float viewportWidthPx, float viewportHeightPx,
                    const glm::vec3& cameraPos,
                    float coord, float camPerp, float window, bool parallelToZ)
{
    float best = 0.0f;
    const float samples[3] = { -window * 0.5f, 0.0f, window * 0.5f };
    for (float off : samples) {
        const glm::vec3 pt = parallelToZ
            ? glm::vec3(coord, 0.0f, camPerp + off)
            : glm::vec3(camPerp + off, 0.0f, coord);
        const float px = PxPerUnit(vp, viewportWidthPx, viewportHeightPx, cameraPos, pt);
        if (px > best)
            best = px;
    }
    return best;
}

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
        // transition can be verified. Two levels are active at once — the fine
        // grid (spacing L) and the chunk grid (spacing 10L) — and L jumps to
        // 10L as the density drops, so the 10x10 chunks hand their thick border
        // to 100x100, then 1000x1000, exactly like Unity.
        m_DebugRefPxPerUnit = densityOverride;
        EmitUniformLevels(1.0f, densityOverride, viewProjection,
                          viewportWidthPx, viewportHeightPx, cameraPos);
        m_DebugLineCount = (uint32_t)m_Lines.size();
        ComputeDebugSpacing();
        return;
    }

    // CAMERA mode: one pass per recursion level. Each world line belongs to
    // exactly one level (its FINEST: coords a coarser level also owns are
    // skipped inside EmitLevel), so nothing is drawn twice on top of itself.
    for (float spacing : kLevelSpacings) {
        EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                  cameraPos, true, densityOverride);  // lines parallel to Z (x = coord)
        EmitLevel(spacing, viewProjection, viewportWidthPx, viewportHeightPx,
                  cameraPos, false, densityOverride); // lines parallel to X (z = coord)
    }

    // Origin axes (opaque, never fade, reach the horizon at any zoom).
    DrawLine({ -kAxisExtent, kGridY, 0.0f }, { kAxisExtent, kGridY, 0.0f }, kAxisXColor, kAxisWidth);
    DrawLine({ 0.0f, kGridY, -kAxisExtent }, { 0.0f, kGridY, kAxisExtent }, kAxisZColor, kAxisWidth);

    m_DebugLineCount = (uint32_t)m_Lines.size();
    ComputeDebugSpacing();
}

// Derive the active fine/chunk recursion levels from the reference density so
// the HUD shows the same LOD numbers the grid actually picks. This mirrors
// EmitUniformLevels' baseSpacing walk (and EmitLevel's ramp): the fine spacing
// is the smallest power of 10 whose cells are still readable on screen.
void EditorGrid::ComputeDebugSpacing()
{
    float spacing = 1.0f;
    while (spacing < 10000.0f && spacing * m_DebugRefPxPerUnit < kCellPxFadeStart)
        spacing *= 10.0f;
    m_DebugFineSpacing = spacing;
    m_DebugChunkSpacing = spacing * 10.0f;
}

// Unity-style grid driven by a uniform pxPerUnit (manual LOD knob). Two roles
// crossfade seamlessly with the density:
//   L = smallest power of 10 whose cells are still readable on screen
//       (L*pxPerUnit >= kCellPxFadeStart). u = visibility of the current level
//       (1 fully readable, 0 unreadable). As the density drops (camera zooms
//       out) u -> 0 and the NEXT coarser level (10L) fades in with the SAME
//       geometry: the 10x10 chunk lines shed their thick border and become the
//       plain common lines of the next level, while 100x100 takes the thick
//       border — exactly Unity's recursive grid. No pop, no overdraw.
void EditorGrid::EmitUniformLevels(float baseSpacing, float pxPerUnit,
                                   const Leir::Matrix4x4& viewProjection,
                                   float viewportWidthPx, float viewportHeightPx,
                                   const Leir::Vector3& cameraPos)
{
    // Active fine level: smallest power of 10 still readable on screen.
    float spacing = baseSpacing;
    while (spacing < 10000.0f && spacing * pxPerUnit < kCellPxFadeStart)
        spacing *= 10.0f;
    const float nextSpacing = spacing * 10.0f;
    const float next2Spacing = nextSpacing * 10.0f;

    // u: current level visibility; v: next level visibility (effectively always
    // readable while u is fading, but keep the exact ramp for correctness).
    const float u = DensityAlpha(spacing * pxPerUnit, kCellPxFadeStart, kCellPxFadeEnd);
    const float v = DensityAlpha(nextSpacing * pxPerUnit, kCellPxFadeStart, kCellPxFadeEnd);
    const float fadeIn = (1.0f - u) * v; // next level's share as the current fades

    // Spacing L  -> plain fine lines that fade with the level (1.5px).
    // Spacing 10L -> old chunk (0.9, 3px) handing the border to the next level's
    //               fine role (0.35, 1.5px) — a single continuous alpha/width.
    // Spacing 100L -> the new chunk fading in (0.9, 3px).
    const float lAlpha = kMinorMaxAlpha * u;
    const float tenAlpha = (kMinorMaxAlpha + kMajorMaxAlpha) * u + kMinorMaxAlpha * fadeIn;
    const float hundredAlpha = (kMinorMaxAlpha + kMajorMaxAlpha) * fadeIn;
    const float lWidth = kMinorWidth;
    const float tenWidth = kMinorWidth + (kMajorWidth - kMinorWidth) * u;
    const float hundredWidth = kMajorWidth;

    Leir::Vector4 lColor = kMinorColor; lColor.w = lAlpha;
    // The 10L line morphs from chunk (near-white) to plain common line (gray)
    // as it sheds its thick border: blend the color with the same u ramp.
    Leir::Vector4 tenColor = Leir::Vector4::Lerp(kMajorColor, kMinorColor, 1.0f - u);
    tenColor.w = std::min(tenAlpha, 1.0f);
    Leir::Vector4 hundredColor = kMajorColor; hundredColor.w = std::min(hundredAlpha, 1.0f);

    for (bool parallelToZ : { true, false }) {
        const float camCoord = parallelToZ ? cameraPos.x : cameraPos.z;
        const float camPerp = parallelToZ ? cameraPos.z : cameraPos.x;

        // Level L: fine lines at spacing L (skip the axes and chunk lines).
        if (lAlpha >= 0.02f) {
            const float window = std::min(kWindowScale * spacing, kMaxWindow);
            const int i0 = (int)std::floor((camCoord - window) / spacing);
            const int i1 = (int)std::ceil((camCoord + window) / spacing);
            for (int i = i0; i <= i1; ++i) {
                const float coord = (float)i * spacing;
                if (coord == 0.0f)
                    continue; // the axis covers the origin line
                if (i % 10 == 0)
                    continue; // the chunk line covers it
                if (parallelToZ)
                    DrawLine({ coord, kGridY, camPerp - window },
                             { coord, kGridY, camPerp + window }, lColor, lWidth);
                else
                    DrawLine({ camPerp - window, kGridY, coord },
                             { camPerp + window, kGridY, coord }, lColor, lWidth);
            }
        }

        // Level 10L: the chunk border (thick while its sub-cells are readable),
        // smoothly turning into the next level's fine line as density drops.
        if (tenAlpha >= 0.02f) {
            const float window = std::min(kWindowScale * nextSpacing, kMaxWindow);
            const int i0 = (int)std::floor((camCoord - window) / nextSpacing);
            const int i1 = (int)std::ceil((camCoord + window) / nextSpacing);
            for (int i = i0; i <= i1; ++i) {
                const float coord = (float)i * nextSpacing;
                if (coord == 0.0f)
                    continue; // the axis covers the origin line
                if (i % 10 == 0)
                    continue; // the 100L chunk line covers it
                if (parallelToZ)
                    DrawLine({ coord, kGridY, camPerp - window },
                             { coord, kGridY, camPerp + window }, tenColor, tenWidth);
                else
                    DrawLine({ camPerp - window, kGridY, coord },
                             { camPerp + window, kGridY, coord }, tenColor, tenWidth);
            }
        }

        // Level 100L: the next chunk border fading in as the current fades.
        if (hundredAlpha >= 0.02f) {
            const float window = std::min(kWindowScale * next2Spacing, kMaxWindow);
            const int i0 = (int)std::floor((camCoord - window) / next2Spacing);
            const int i1 = (int)std::ceil((camCoord + window) / next2Spacing);
            for (int i = i0; i <= i1; ++i) {
                const float coord = (float)i * next2Spacing;
                if (coord == 0.0f)
                    continue; // the axis covers the origin line
                if (parallelToZ)
                    DrawLine({ coord, kGridY, camPerp - window },
                             { coord, kGridY, camPerp + window }, hundredColor, hundredWidth);
                else
                    DrawLine({ camPerp - window, kGridY, coord },
                             { camPerp + window, kGridY, coord }, hundredColor, hundredWidth);
            }
        }
    }

    // Origin axes (opaque, never fade, reach the horizon at any zoom).
    DrawLine({ -kAxisExtent, kGridY, 0.0f }, { kAxisExtent, kGridY, 0.0f }, kAxisXColor, kAxisWidth);
    DrawLine({ 0.0f, kGridY, -kAxisExtent }, { 0.0f, kGridY, kAxisExtent }, kAxisZColor, kAxisWidth);
}

void EditorGrid::EmitLevel(float spacing, const Leir::Matrix4x4& viewProjection,
                           float viewportWidthPx, float viewportHeightPx,
                           const Leir::Vector3& cameraPos, bool parallelToZ,
                           float densityOverride)
{
    // The coordinate that varies along the line (the line is drawn parallel to
    // the other axis, perpendicular to `coord`). `coord` steps by `spacing`.
    const float camCoord = parallelToZ ? cameraPos.x : cameraPos.z;
    const float camPerp = parallelToZ ? cameraPos.z : cameraPos.x;
    const float window = std::min(kWindowScale * spacing, kMaxWindow);
    const int i0 = (int)std::floor((camCoord - window) / spacing);
    const int i1 = (int)std::ceil((camCoord + window) / spacing);
    const glm::mat4 vp = viewProjection;

    for (int i = i0; i <= i1; ++i) {
        const float coord = (float)i * spacing;
        if (coord == 0.0f)
            continue; // the axis covers the origin line
        if (i % 10 == 0)
            continue; // a coarser level owns this line (drawn exactly once)

        // Screen density of 1 world unit at the closest visible part of the
        // line. Normally sampled from the real camera projection (the closest
        // part wins, so a line is fully visible while it approaches the camera
        // and fades as it recedes / as you zoom out). The Test2 panel can
        // OVERRIDE this with a uniform pxPerUnit (>= 0) to drive the LOD
        // transition manually, simulating the camera zooming out without
        // touching the camera.
        float pxPerUnit;
        if (densityOverride >= 0.0f) {
            pxPerUnit = densityOverride;
        } else {
            pxPerUnit = LinePxPerUnit(vp, viewportWidthPx, viewportHeightPx, cameraPos,
                                      coord, camPerp, window, parallelToZ);
            if (pxPerUnit <= 0.0f)
                continue;
        }

        // Role 1: fine cell boundary of size `spacing`.
        const float minorVis = DensityAlpha(spacing * pxPerUnit,
                                            kCellPxFadeStart, kCellPxFadeEnd);
        // Role 2: chunk boundary of the cells spacing/10 below it.
        const float chunkVis = (spacing >= 10.0f)
            ? DensityAlpha((spacing / 10.0f) * pxPerUnit,
                           kCellPxFadeStart, kCellPxFadeEnd)
            : 0.0f;

        const float alpha = kMinorMaxAlpha * minorVis + kMajorMaxAlpha * chunkVis;
        if (alpha < 0.02f)
            continue;

        Leir::Vector4 c = Leir::Vector4::Lerp(kMinorColor, kMajorColor, chunkVis);
        c.w = alpha;
        const float width = kMinorWidth + (kMajorWidth - kMinorWidth) * chunkVis;

        if (parallelToZ)
            DrawLine({ coord, kGridY, camPerp - window },
                     { coord, kGridY, camPerp + window }, c, width);
        else
            DrawLine({ camPerp - window, kGridY, coord },
                     { camPerp + window, kGridY, coord }, c, width);
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