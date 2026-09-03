#include "LeirEngine/UI/UIRenderer.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIImage.h"
#include "LeirEngine/UI/UISlider.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UITextArea.h"
#include "LeirEngine/UI/UIViewportPanel.h"
#include "LeirEngine/UI/UIWindow.h"
#include "LeirEngine/UI/Dock/DockTabBar.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Rendering/RenderTexture.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/Image.h"
#include "LeirEngine/Core/Settings.h"

#include <algorithm>
#include <cmath>
#include "LeirEngine/Core/Log.h"

// ============================================================================
// CAPA DE DIBUJO DE LOS OUTLINES DE DEBUG (debug.ui_outlines)
// ----------------------------------------------------------------------------
// Los outlines verdes son una ayuda de depuración: muestran los bounds lógicos
// de cada elemento. Hay dos modos de dibujarlos, elegidos por este define:
//
//   1 = (DEFAULT) ARRIBA DE TODO  -> usa BuildBatchDebug (capa debug overlay)
//       Flush dibuja en orden regular -> viewport -> debug, así que el outline
//       queda SIEMPRE por encima de cualquier contenido (incluido el quad del
//       RenderTexture del viewport, que vive en la capa media y de lo contrario
//       tapa el borde del propio viewport). Nada puede ocultar una línea.
//       Costo: los outlines de elementos ocultos/tapados también se ven (eso
//       suele ser justo lo que se quiere al depurar bounds).
//
//   0 = COMO ANTES  -> usa Batch (capa regular)
//       El outline va en la misma capa que los rellenos y textos. No pisa nada
//       que esté encima, pero cualquier contenido (viewport, paneles superpuestos)
//       puede TAPARLO y "desaparecen" líneas (el bug del borde del viewport).
//
// Para comparar: cambiá el valor, recompilá y activá debug.ui_outlines.
// ============================================================================
#define LEIR_UI_OUTLINES_ON_TOP 1

// ============================================================================
// SNAP DEL RECT DE LOS OUTLINES DE DEBUG (debug.ui_outlines)
// ----------------------------------------------------------------------------
// Trilema a DPI fraccionario (125%): el texto se dibuja sub-pixel SIN snapnear,
// pero el outline se deriva de un rect snapneado. Según cómo se snapnee, se
// cumplen 2 de 3 cosas (imposible las 3 a la vez):
//   (a) el contenido NUNCA asoma afuera del outline,
//   (b) las líneas dobles de elementos adyacentes se ven como 2,
//   (c) las líneas individuales son nítidas de 1px.
//
//   0 = ROUND   -> usa SnapToPhysicalPixels (round). Cumple (b) y (c); NO (a):
//       el round desplaza el outline hasta ~0.5px lógico y un texto pegado al
//       borde puede asomar ~1px a la izquierda de la línea.
//
//   1 = (DEFAULT) ENCLOSE -> usa SnapToPhysicalPixelsEnclose (floor/ceil, la
//       misma convención conservadora del scissor). Cumple (a) y (c); NO (b):
//       el outline SIEMPRE encierra el contenido (nada asoma), pero dos
//       elementos que comparten borde fusionan su línea en 1px en 125% (el
//       borde compartido, mostrado una vez — discutiblemente más fiel).
//       Es el comportamiento profesional: un outline de debug debe mostrar los
//       bounds reales del elemento con el contenido adentro.
//
//   2 = HYBRID -> ENCLOSE para elementos que dibujan texto (UILabel,
//       UITextInput, UITextArea) + ROUND para el resto (dock, tabs, botones,
//       paneles). Lo mejor de ambos en la práctica: arregla el texto sin
//       fusionar los tabs. Costo: otra heurística implícita (dynamic_cast).
//
//   3 = ORIGINAL (el que estaba en el último commit/push) -> SIN snap: se usa
//       el rect lógico exacto (cr) y grosor t = 1.0 px LÓGICO (no 1/scale).
//       Es el comportamiento previo a toda esta saga de snapping. A 125% una
//       línea de 1px lógico son 1.25 físicos, así que se ve de 1 o 2px según
//       la alineación (inconsistente), pero es 100% fiel al rect lógico real.
//       La capa sigue la controla LEIR_UI_OUTLINES_ON_TOP (Batch/BuildBatchDebug).
//
// Para comparar: cambiá el valor, recompilá y activá debug.ui_outlines.
// ============================================================================
#define LEIR_UI_OUTLINE_SNAP 1

namespace Leir {

namespace {

// Convert a logical clip rect to a physical scissor rect. Uses floor for the
// offset and ceil for the opposite edge so the scissor always covers every
// pixel that a fractional-edge quad touches — truncation used to drop the
// last row/column of children (e.g. the bottom edge of the horizontal
// scrollbar track cut to 9px).
void ScissorFromLogicalClip(const Vector4& c, float scale, float pw, float ph, RHI::RHIRect2D& s)
{
    const float x0 = std::clamp(c.x, 0.0f, pw);
    const float y0 = std::clamp(c.y, 0.0f, ph);
    const float x1 = std::clamp(c.x + c.z, 0.0f, pw);
    const float y1 = std::clamp(c.y + c.w, 0.0f, ph);
    s.x = (int32_t)std::floor(x0 * scale);
    s.y = (int32_t)std::floor(y0 * scale);
    s.width = (uint32_t)std::max(0.0f, std::ceil(x1 * scale) - std::floor(x0 * scale));
    s.height = (uint32_t)std::max(0.0f, std::ceil(y1 * scale) - std::floor(y0 * scale));
}

// Pixel snapping: the UI is laid out and batched in LOGICAL units, but at
// fractional DPI scales (e.g. 125% = 1.25) a logical coordinate maps to a
// FRACTIONAL physical pixel. A 1-logical-px line (or a texture's 1px border)
// then renders 1 or 2 physical px depending on where its edge lands on the
// pixel grid — and which side is "lucky" flips with position/window size.
// Snap every quad's edges to the physical pixel grid and convert back to
// logical (the vertex shader then maps those back to integer physical pixels).
// At scale <= 1 (100%) the coordinates are already 1:1 and the rect is
// returned untouched.
//
// ROUND, not floor/ceil — why: adjacent elements share a logical boundary
// (tab A ends where tab B starts). With floor/ceil the shared value becomes
// tab A's ceil-edge (137.5 -> 138) but tab B's floor-edge (137.5 -> 137), a
// 1-px mismatch that OVERLAPS the two snapped rects; their 1-px outlines then
// land on the SAME pixel and visually merge into one line (the "double lines
// collapse to 1 at 125%" bug). Round is a consistent function: the same
// logical boundary always maps to the same physical pixel, so adjacent
// elements stay adjacent and their outlines render as distinct 1-px lines.
// Trade-off vs floor/ceil: a fill can lose 0.5 px at an outer edge (a faint
// sliver against the background), which is acceptable; interior edges are
// shared and stay seamless.
//
// PRECONDITION (the implicit heuristic — see BuildBatch): this helper must
// ONLY be applied to quads that sample their FULL texture ({0,0,1,1} UV).
// Text glyphs pass a PARTIAL UV into the shared font atlas; snapping their
// quad WITHOUT snapping the UV stretches the glyph and overshoots into the
// neighbouring glyphs packed next to it (ghost/split letters). Standalone
// textures (icons) and flat fills have nothing adjacent to bleed into, so
// snapping is safe there. See BuildBatch for the full rationale.
Vector4 SnapToPhysicalPixels(const Vector4& r, float scale)
{
    if (scale <= 1.0f)
        return r;
    // kSnapEps guards the logical<->physical float round-trip: dividing by
    // scale and multiplying back can land 1 ULP off an integer (7/1.25*1.25 ==
    // 6.9999999...). The epsilon (>> float noise, << half a pixel) keeps an
    // edge that is exactly on a pixel boundary on it.
    constexpr float kSnapEps = 1e-3f;
    const float x0 = std::round(r.x * scale + kSnapEps) / scale;
    const float y0 = std::round(r.y * scale + kSnapEps) / scale;
    const float x1 = std::round((r.x + r.z) * scale - kSnapEps) / scale;
    const float y1 = std::round((r.y + r.w) * scale - kSnapEps) / scale;
    return { x0, y0, x1 - x0, y1 - y0 };
}

// Conservative enclosure variant of SnapToPhysicalPixels (floor min / ceil
// max), the same convention as ScissorFromLogicalClip. The returned rect is
// guaranteed to cover EVERY physical pixel that the logical rect touches, so
// anything rendered at the logical rect — including sub-pixel-positioned text
// glyphs, which are NOT snapped — is always INSIDE the result.
//
// Use for DEBUG OUTLINES when the content must not stick out (the round variant
// can shift the outline up to ~0.5 logical px away from the content, leaving a
// 1-px sliver of text outside the line). Trade-off: adjacent elements that
// share a boundary both claim the boundary pixel, so their 1-px outlines merge
// into one line at fractional DPI (see LEIR_UI_OUTLINE_SNAP).
Vector4 SnapToPhysicalPixelsEnclose(const Vector4& r, float scale)
{
    if (scale <= 1.0f)
        return r;
    constexpr float kSnapEps = 1e-3f;
    const float x0 = std::floor(r.x * scale + kSnapEps) / scale;
    const float y0 = std::floor(r.y * scale + kSnapEps) / scale;
    const float x1 = std::ceil((r.x + r.z) * scale - kSnapEps) / scale;
    const float y1 = std::ceil((r.y + r.w) * scale - kSnapEps) / scale;
    return { x0, y0, x1 - x0, y1 - y0 };
}

// True when a quad samples its ENTIRE texture (UV spans [0,0]..[1,1]).
// Used as the pixel-snap gate in BuildBatch/BuildBatchDebug.
bool IsFullUv(const Vector4& uv)
{
    return uv.x <= 0.0f && uv.y <= 0.0f && uv.z >= 1.0f && uv.w >= 1.0f;
}

} // namespace

UIRenderer::UIRenderer(RHI::RenderBackend* device)
    : m_Device(device)
{
    // Descriptor set + pipeline layouts derived from the UI shader reflection
    // sidecars (Plan B, Fase 2). Falls back to the hand-written layout when
    // the sidecars are missing (engine running without a shader compiler).
    const std::string vertPath = std::string(LEIR_SHADER_DIR) + "/UI.vert" + m_Device->GetShaderFileExtension();
    const std::string fragPath = std::string(LEIR_SHADER_DIR) + "/UI.frag" + m_Device->GetShaderFileExtension();
    const RHI::ShaderReflection uiReflection = LoadShaderReflectionFromSidecars({ vertPath, fragPath });
    if (!uiReflection.bindings.empty()) {
        m_SetLayouts = CreateSetLayoutsFromReflection(m_Device, uiReflection);
        if (!m_SetLayouts.empty())
            m_DescSetLayout = m_SetLayouts[0].layout; // set 0: combined image sampler
        m_PipelineLayout = CreatePipelineLayoutFromReflection(m_Device, uiReflection, m_SetLayouts);
    } else {
        m_DescSetLayout = m_Device->CreateDescriptorSetLayout({
            { 0, RHI::DescriptorType::CombinedImageSampler, 1, RHI::ShaderStage::Fragment, true }
        });
        m_SetLayouts = { { 0, m_DescSetLayout } };

        RHI::RHIPushConstantRange pushRange{};
        pushRange.stage = RHI::ShaderStageMask::Vertex;
        pushRange.offset = 0;
        pushRange.size = sizeof(Vector2);
        m_PipelineLayout = m_Device->CreatePipelineLayout({ m_DescSetLayout }, { pushRange });
    }

    CreatePipeline();

    // White fallback texture for untextured quads
    Image whiteImg(1, 1, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    m_FallbackTex = new Texture2D(m_Device, whiteImg);

    m_MaxVertices = 65536;
    uint32_t vbSize = m_MaxVertices * sizeof(UIVertex);
    for (int f = 0; f < 3; ++f) {
        m_VertexBuffers[f] = m_Device->CreateBuffer(vbSize,
            RHI::BufferUsage::Vertex,
            RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
            m_VertexMemories[f]);
    }

    XConsole::Println("UIRenderer created");
}

void UIRenderer::CreatePipeline()
{
    auto vertCode = Shader::ReadFile(
        std::string(LEIR_SHADER_DIR) + "/UI.vert" + m_Device->GetShaderFileExtension());
    auto fragCode = Shader::ReadFile(
        std::string(LEIR_SHADER_DIR) + "/UI.frag" + m_Device->GetShaderFileExtension());
    RHI::RHIShaderModule vertMod = m_Device->CreateShaderModule(vertCode);
    RHI::RHIShaderModule fragMod = m_Device->CreateShaderModule(fragCode);

    RHI::RHIShaderStageInfo stages[2];
    stages[0].stage = RHI::ShaderStage::Vertex;
    stages[0].module = vertMod;
    stages[0].entryPoint = "main";
    stages[1].stage = RHI::ShaderStage::Fragment;
    stages[1].module = fragMod;
    stages[1].entryPoint = "main";

    RHI::RHIVertexInputBinding bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(UIVertex);
    bindingDesc.inputRate = RHI::VertexInputRate::Vertex;

    std::vector<RHI::RHIVertexAttribute> attrs(4);
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = RHI::Format::R32G32_SFLOAT;
    attrs[0].offset = offsetof(UIVertex, position);
    attrs[0].semantic = "POSITION";
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = RHI::Format::R32G32_SFLOAT;
    attrs[1].offset = offsetof(UIVertex, texCoord);
    attrs[1].semantic = "TEXCOORD";
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = RHI::Format::R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(UIVertex, color);
    attrs[2].semantic = "COLOR";
    attrs[3].location = 3;
    attrs[3].binding = 0;
    attrs[3].format = RHI::Format::R32_SFLOAT;
    attrs[3].offset = offsetof(UIVertex, textureIndex);
    attrs[3].semantic = "TEXCOORD";
    attrs[3].semanticIndex = 1; // matches UI.vert's `: TEXCOORD1`

    RHI::RHIPipelineDesc desc{};
    desc.layout = m_PipelineLayout;
    desc.renderPass = m_Device->GetOverlayRenderPass();
    desc.stages = { stages[0], stages[1] };
    desc.vertexBinding = bindingDesc;
    desc.vertexAttributes = attrs;
    desc.topology = RHI::Topology::TriangleStrip;
    desc.polygonMode = RHI::PolygonMode::Fill;
    desc.cullMode = RHI::CullMode::None;
    desc.depthTestEnable = false;
    desc.blend.enable = true;
    m_Pipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);
}

void UIRenderer::ReloadShaders()
{
    if (!m_Pipeline.IsValid())
        return;
    m_Device->DestroyPipeline(m_Pipeline);
    m_Pipeline = RHI::RHIPipeline{};
    CreatePipeline();
    XConsole::Println("UI pipeline reloaded");
}

UIRenderer::~UIRenderer()
{
    if (m_Pipeline.IsValid()) m_Device->DestroyPipeline(m_Pipeline);
    if (m_PipelineLayout.IsValid()) m_Device->DestroyPipelineLayout(m_PipelineLayout);
    for (auto& entry : m_SetLayouts) {
        if (entry.layout.IsValid())
            m_Device->DestroyDescriptorSetLayout(entry.layout);
    }
    for (int f = 0; f < 3; ++f) {
        if (m_VertexBuffers[f].IsValid()) m_Device->DestroyBuffer(m_VertexBuffers[f]);
        if (m_VertexMemories[f].IsValid()) m_Device->DestroyMemory(m_VertexMemories[f]);
    }
    delete m_FallbackTex;
}

void UIRenderer::BuildBatch(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color)
{
    // Pixel-snap gate — IMPLICIT HEURISTIC (documented on purpose):
    //   * FULL UV ({0,0,1,1})  -> the quad samples its whole texture (flat
    //     fills, standalone images/icons). Snap to the physical pixel grid:
    //     floor/ceil is safe because the texture has no neighbours — a 1px
    //     stretch just linearly resamples the same image, and it FIXES the
    //     clipped border / 1-or-2px-line instability at fractional DPI.
    //   * PARTIAL UV (glyphs)  -> the quad samples a sub-rect of the SHARED
    //     font atlas, where neighbouring glyphs are packed right next to the
    //     glyph. Snapping the quad WITHOUT snapping the UV would stretch it
    //     and overshoot into those neighbours ("ghost"/split letters). Text is
    //     left at its exact logical rect: sub-pixel positioned + antialiased,
    //     which is how browsers/Qt/Windows render text at fractional DPI.
    //
    // This is a pragmatic, localized rule, not the textbook architecture (snap
    // position AND UV together, or snap in layout). It was chosen because the
    // layout stays 100% logical (smooth scroll, dock math, hit-testing) and the
    // cost/risk is tiny. If a non-atlas texture is ever drawn with a partial UV
    // sub-rect (e.g. a sprite sheet) and mis-renders at 125%, revisit here.
    const Vector4 r = IsFullUv(uv) ? SnapToPhysicalPixels(rect, m_ContentScale) : rect;
    float x0 = r.x;
    float y0 = r.y;
    float x1 = r.x + r.z;
    float y1 = r.y + r.w;
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.z, v1 = uv.y + uv.w;

    float idx = (float)(texture ? texture : m_FallbackTex)->GetBindlessIndex();

    m_Vertices.push_back({{x0, y0}, {u0, v0}, color, idx});
    m_Vertices.push_back({{x1, y0}, {u1, v0}, color, idx});
    m_Vertices.push_back({{x0, y1}, {u0, v1}, color, idx});
    m_Vertices.push_back({{x1, y1}, {u1, v1}, color, idx});

    m_QuadTextures.push_back(texture ? texture : m_FallbackTex);
    if (m_CurrentClip)
        m_QuadClips.push_back(*m_CurrentClip);
    else
        m_QuadClips.push_back({0.0f, 0.0f, m_ScreenSize.x, m_ScreenSize.y});
}

void UIRenderer::BuildBatchDebug(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color)
{
    // Same pixel-snap gate as BuildBatch (full-UV quads snap; glyph sub-rects
    // don't). See BuildBatch for the full rationale.
    const Vector4 r = IsFullUv(uv) ? SnapToPhysicalPixels(rect, m_ContentScale) : rect;
    float x0 = r.x;
    float y0 = r.y;
    float x1 = r.x + r.z;
    float y1 = r.y + r.w;
    float u0 = uv.x, v0 = uv.y, u1 = uv.x + uv.z, v1 = uv.y + uv.w;

    float idx = (float)(texture ? texture : m_FallbackTex)->GetBindlessIndex();

    m_DebugVertices.push_back({{x0, y0}, {u0, v0}, color, idx});
    m_DebugVertices.push_back({{x1, y0}, {u1, v0}, color, idx});
    m_DebugVertices.push_back({{x0, y1}, {u0, v1}, color, idx});
    m_DebugVertices.push_back({{x1, y1}, {u1, v1}, color, idx});

    m_DebugQuadTextures.push_back(texture ? texture : m_FallbackTex);
    if (m_CurrentClip)
        m_DebugQuadClips.push_back(*m_CurrentClip);
    else
        m_DebugQuadClips.push_back({0.0f, 0.0f, m_ScreenSize.x, m_ScreenSize.y});
}

void UIRenderer::Flush(RHI::GCommandGraph& graph)
{
    size_t regCount = m_QuadTextures.size();
    size_t vpCount = m_ViewportDraws.size();
    size_t dbgCount = m_DebugQuadTextures.size();

    size_t totalQuads = regCount + vpCount + dbgCount;
    if (totalQuads == 0) {
        m_LastStats = {};
        return;
    }

    // Per-frame stats: reset before this frame's draw loop. drawCalls is
    // accumulated by flushBatch below; without this reset it kept growing
    // across frames (showed 16000+ in the stats overlay instead of ~dozens).
    m_LastStats = {};

    // Batching with a TRIANGLE_STRIP pipeline needs 2 degenerate vertices
    // between consecutive quads so strips never bridge across elements.
    const size_t slotPerQuad = 6;
    size_t maxQuads = (size_t)m_MaxVertices / slotPerQuad;
    if (totalQuads > maxQuads) {
        XConsole::Debug("UIRenderer: overflow, truncating {} -> {} quads",
            (int)totalQuads, (int)maxQuads);
        size_t fixed = vpCount;
        while (regCount > 0 && regCount + fixed + dbgCount > maxQuads) {
            m_Vertices.resize(m_Vertices.size() - 4);
            m_QuadTextures.pop_back();
            m_QuadClips.pop_back();
            --regCount;
        }
        while (dbgCount > 0 && regCount + fixed + dbgCount > maxQuads) {
            m_DebugVertices.resize(m_DebugVertices.size() - 4);
            m_DebugQuadTextures.pop_back();
            m_DebugQuadClips.pop_back();
            --dbgCount;
        }
        totalQuads = regCount + fixed + dbgCount;
    }

    // Collect quads in draw order: regular → viewport → debug. Each quad
    // carries the bindless texture index (read back per-vertex by UI.frag).
    struct FlushQuad {
        const UIVertex* src; // 4 source vertices
        Vector4 clip;
        uint32_t texIndex;
    };
    std::vector<FlushQuad> quads;
    quads.reserve(totalQuads);
    for (size_t qi = 0; qi < regCount; ++qi)
        quads.push_back({ &m_Vertices[qi * 4], m_QuadClips[qi], m_QuadTextures[qi]->GetBindlessIndex() });
    for (size_t i = 0; i < vpCount; ++i)
        quads.push_back({ m_ViewportDraws[i].verts, m_ViewportDraws[i].clip,
                          m_ViewportDraws[i].texture->GetBindlessIndex() });
    for (size_t qi = 0; qi < dbgCount; ++qi)
        quads.push_back({ &m_DebugVertices[qi * 4], m_DebugQuadClips[qi], m_DebugQuadTextures[qi]->GetBindlessIndex() });

    int frame = (int)m_Device->GetCurrentFrameIndex();

    size_t totalBytes = totalQuads * slotPerQuad * sizeof(UIVertex);

    void* data;
    m_Device->MapMemory(m_VertexMemories[frame], 0, (uint32_t)totalBytes, &data);

    // Interleave degenerate vertices between quads to break the strip.
    UIVertex* dst = (UIVertex*)data;
    for (size_t qi = 0; qi < totalQuads; ++qi) {
        const UIVertex* src = quads[qi].src;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[3];                                     // repeat last vertex
        dst[5] = (qi + 1 < totalQuads) ? quads[qi + 1].src[0] : src[3]; // next quad's first (or repeat)
        dst += slotPerQuad;
    }

    m_Device->UnmapMemory(m_VertexMemories[frame]);

    // Logical canvas size maps vertices (in logical units) to NDC.
    Vector2 screenSize = m_ScreenSize;

    graph.BindPipeline(m_Pipeline);
    graph.BindVertexBuffer(m_VertexBuffers[frame]);
    graph.PushConstants(m_PipelineLayout, RHI::ShaderStageMask::Vertex, 0, sizeof(Vector2), &screenSize);

    // Bindless: all quads index the same global bindless set (set 0); the
    // texture is selected per-vertex via UI.frag's NonUniformResourceIndex.
    graph.BindDescriptorSets(m_PipelineLayout, 0,
        { m_Device->GetBindlessDescriptorSet() });

    // Batched draw: consecutive quads that share the same texture (bindless
    // index) AND scissor are merged into a single draw call.
    RHI::RHIRect2D lastScissor{};
    bool lastScissorValid = false;

    uint32_t batchTexIndex = 0;
    RHI::RHIRect2D batchScissor{};
    uint32_t batchStart = 0;   // first vertex index of the current batch
    uint32_t batchCount = 0;   // quads in the current batch
    uint32_t drawnQuads = 0;

    auto sameScissor = [](const RHI::RHIRect2D& a, const RHI::RHIRect2D& b) {
        return a.x == b.x && a.y == b.y &&
               a.width == b.width && a.height == b.height;
    };

    auto flushBatch = [&]() {
        if (batchCount == 0) return;
        // (count*6 - 2) vertices: count quads minus the trailing degenerate pair.
        // Last-use tracking: this batch samples the batch's texture.
        graph.SetSampledTextures({ batchTexIndex });
        graph.Draw(batchCount * (uint32_t)slotPerQuad - 2, batchStart);
        ++m_LastStats.drawCalls;
        batchCount = 0;
    };

    auto pushQuad = [&](uint32_t texIndex, const Vector4& logicalClip, uint32_t quadIdx) {
        RHI::RHIRect2D scissor{};
        {
            const float pw = m_ScreenSize.x * m_ContentScale;
            const float ph = m_ScreenSize.y * m_ContentScale;
            ScissorFromLogicalClip(logicalClip, m_ContentScale, pw, ph, scissor);
        }
        if (batchCount > 0 && texIndex == batchTexIndex && sameScissor(scissor, batchScissor)) {
            ++batchCount;
            ++drawnQuads;
            return;
        }
        flushBatch();
        ApplyScissor(graph, logicalClip, lastScissor, lastScissorValid);
        batchTexIndex = texIndex;
        batchScissor = scissor;
        batchStart = quadIdx * (uint32_t)slotPerQuad;
        batchCount = 1;
        ++drawnQuads;
    };

    for (size_t qi = 0; qi < totalQuads; ++qi)
        pushQuad(quads[qi].texIndex, quads[qi].clip, (uint32_t)qi);

    flushBatch();

    m_LastStats.quads = drawnQuads;
    m_LastStats.vertices = drawnQuads * (uint32_t)slotPerQuad;
    m_LastStats.batches = m_LastStats.drawCalls;
}

void UIRenderer::Render(RHI::GCommandGraph& graph, UICanvas* canvas)
{
    m_Vertices.clear();
    m_QuadTextures.clear();
    m_QuadClips.clear();
    m_ViewportDraws.clear();
    m_DebugVertices.clear();
    m_DebugQuadTextures.clear();
    m_DebugQuadClips.clear();

    if (!canvas || !canvas->IsActive()) return;

    m_ScreenSize = {canvas->GetScreenWidth(), canvas->GetScreenHeight()};
    m_CurrentClip = nullptr;

    // Walk tree depth-first. clip = active clip rect (nullptr = full canvas).
    RenderElement(canvas, nullptr, false);

    Flush(graph);
}

void UIRenderer::RenderElement(UIElement* elem, const Vector4* clip, bool isDebug)
{
    if (!elem->IsActive()) return;
    const auto& cr = elem->GetComputedRect();

    if (elem->IsOverlayLayer()) isDebug = true;

    // Effective clip: intersect this element's rect with the active clip when
    // clipping is enabled. Elements fully outside the clip are culled.
    Vector4 localClip;
    const Vector4* effClip = clip;
    if (elem->IsClipEnabled()) {
        if (clip) {
            localClip.x = std::max(clip->x, cr.x);
            localClip.y = std::max(clip->y, cr.y);
            localClip.z = std::min(clip->x + clip->z, cr.x + cr.z) - localClip.x;
            localClip.w = std::min(clip->y + clip->w, cr.y + cr.w) - localClip.y;
        } else {
            localClip = cr;
        }
        if (localClip.z <= 0.0f || localClip.w <= 0.0f) return;
        effClip = &localClip;
    } else if (effClip) {
        // Fast reject: fully outside the active clip.
        if (cr.x + cr.z <= effClip->x || cr.x >= effClip->x + effClip->z ||
            cr.y + cr.w <= effClip->y || cr.y >= effClip->y + effClip->w)
            return;
    }

    m_CurrentClip = effClip;

    auto Batch = [&](Texture2D* t, const Vector4& r, const Vector4& u, const Vector4& c) {
        if (isDebug) BuildBatchDebug(t, r, u, c);
        else BuildBatch(t, r, u, c);
    };

    const bool showQuadDebug = LeirSettings::Get().debug.show_glyph_quads;
    static const Vector4 quadColors[] = {
        {0.0f, 1.0f, 1.0f, 0.35f},
        {1.0f, 1.0f, 0.0f, 0.35f},
        {1.0f, 0.0f, 1.0f, 0.35f},
        {0.5f, 1.0f, 0.0f, 0.35f},
    };

    if (auto* panel = dynamic_cast<UIPanel*>(elem)) {
        Batch(nullptr, cr, {0, 0, 1, 1}, panel->GetColor());
    } else if (auto* img = dynamic_cast<UIImage*>(elem)) {
        Texture2D* tex = img->GetTexture();
        Batch(tex, cr, {0, 0, 1, 1}, img->GetColor());
    } else if (auto* btn = dynamic_cast<UIButton*>(elem)) {
        Vector4 bgColor;
        switch (btn->GetState()) {
            case ButtonState::Normal:  bgColor = btn->GetBgNormal(); break;
            case ButtonState::Hovered: bgColor = btn->GetBgHover(); break;
            case ButtonState::Pressed: bgColor = btn->GetBgPressed(); break;
        }
        Batch(nullptr, cr, {0, 0, 1, 1}, bgColor);

        if (btn->GetFont() && !btn->GetText().empty()) {
            float lineH = btn->GetFont()->GetLineHeight();
            float ascender = btn->GetFont()->GetAscender();
            float baselineY = cr.y + (cr.w - lineH) * 0.5f + ascender;
            auto rawQuads = btn->GetFont()->LayoutText(btn->GetText(), cr.z - 12.0f);
            // Horizontal alignment (default Left = legacy 6px inset). Compute the
            // text block width so Center/Right can position it correctly.
            float blockW = 0.0f;
            for (size_t i = 0; i < rawQuads.size(); i += 2) {
                const auto& r = rawQuads[i];
                if (r.x + r.z > blockW) blockW = r.x + r.z;
            }
            float startX = cr.x + 6.0f;
            switch (btn->GetTextAlign()) {
                case ButtonTextAlign::Center: startX = cr.x + (cr.z - blockW) * 0.5f; break;
                case ButtonTextAlign::Right:  startX = cr.x + cr.z - blockW - 6.0f; break;
                default: break; // Left
            }
            for (size_t i = 0; i < rawQuads.size(); i += 2) {
                const auto& r = rawQuads[i];
                const auto& uv = rawQuads[i + 1];
                Vector4 textRect = {startX + r.x, baselineY + r.y, r.z, r.w};
                Batch(btn->GetFont()->GetAtlasTexture(), textRect, uv, btn->GetTextColor());
            }
        }
    } else if (auto* slider = dynamic_cast<UISlider*>(elem)) {
        Batch(nullptr, cr, {0, 0, 1, 1}, {0.2f, 0.2f, 0.2f, 1.0f});

        float handleT = slider->HandlePos();
        float fillW = cr.z * handleT;
        if (fillW > 0) {
            Vector4 fillRect = {cr.x, cr.y, fillW, cr.w};
            Batch(nullptr, fillRect, {0, 0, 1, 1}, {0.4f, 0.6f, 1.0f, 1.0f});
        }

        float hx = cr.x + cr.z * handleT - 4.0f;
        Vector4 handleRect = {hx, cr.y - 2.0f, 8.0f, cr.w + 4.0f};
        Batch(nullptr, handleRect, {0, 0, 1, 1}, slider->IsDragging()
            ? Vector4{1.0f, 1.0f, 1.0f, 1.0f}
            : Vector4{0.8f, 0.8f, 0.8f, 1.0f});
    } else if (auto* vp = dynamic_cast<UIViewportPanel*>(elem)) {
        if (vp->GetRenderTexture()) {
            ViewportDraw vd;
            float x0 = cr.x, y0 = cr.y, x1 = cr.x + cr.z, y1 = cr.y + cr.w;
            float idx = (float)vp->GetRenderTexture()->GetBindlessIndex();
            vd.verts[0] = {{x0, y0}, {0, 0}, {1,1,1,1}, idx};
            vd.verts[1] = {{x1, y0}, {1, 0}, {1,1,1,1}, idx};
            vd.verts[2] = {{x0, y1}, {0, 1}, {1,1,1,1}, idx};
            vd.verts[3] = {{x1, y1}, {1, 1}, {1,1,1,1}, idx};
            vd.texture = vp->GetRenderTexture();
            vd.clip = m_CurrentClip ? *m_CurrentClip : Vector4{0.0f, 0.0f, m_ScreenSize.x, m_ScreenSize.y};
            m_ViewportDraws.push_back(vd);
        }
    } else if (auto* tab = dynamic_cast<DockTab*>(elem)) {
        const bool active = tab->IsActive();
        Batch(nullptr, cr, {0, 0, 1, 1},
            active ? Vector4{0.23f, 0.23f, 0.27f, 1.0f}
                   : Vector4{0.15f, 0.15f, 0.18f, 1.0f});

        Font* f = tab->GetFont();
        DockPanel* tabPanel = tab->GetPanel();
        if (f && f->GetAtlasTexture() && tabPanel) {
            float lineH = f->GetLineHeight();
            float ascender = f->GetAscender();
            float baselineY = cr.y + (cr.w - lineH) * 0.5f + ascender;
            Vector4 textColor = active
                ? Vector4{1.0f, 1.0f, 1.0f, 1.0f}
                : Vector4{0.72f, 0.72f, 0.72f, 1.0f};

            auto rawQuads = f->LayoutText(tabPanel->title, cr.z - 32.0f);
            for (size_t i = 0; i < rawQuads.size(); i += 2) {
                const auto& r = rawQuads[i];
                const auto& uv = rawQuads[i + 1];
                Vector4 textRect = {cr.x + 8.0f + r.x, baselineY + r.y, r.z, r.w};
                Batch(f->GetAtlasTexture(), textRect, uv, textColor);
            }

            if (tabPanel->closeable) {
                auto xq = f->LayoutText("x", 0.0f);
                float closeX = cr.x + cr.z - 16.0f;
                for (size_t i = 0; i < xq.size(); i += 2) {
                    const auto& r = xq[i];
                    const auto& uv = xq[i + 1];
                    Vector4 textRect = {closeX + r.x, baselineY + r.y, r.z, r.w};
                    Batch(f->GetAtlasTexture(), textRect, uv, {0.85f, 0.55f, 0.55f, 1.0f});
                }
            }
        }
    } else if (auto* label = dynamic_cast<UILabel*>(elem)) {
        if (label->GetFont() && label->GetFont()->GetAtlasTexture()) {
            int quadIdx = 0;
            for (const auto& gq : label->GetGlyphQuads()) {
                Vector4 r = gq.rect;
                r.x += cr.x;
                r.y += cr.y;
                if (showQuadDebug)
                    Batch(nullptr, r, {0, 0, 1, 1}, quadColors[quadIdx % 4]);
                Batch(label->GetFont()->GetAtlasTexture(), r, gq.uv, gq.color);
                ++quadIdx;
            }
        }
    } else if (auto* input = dynamic_cast<UITextInput*>(elem)) {
        Batch(nullptr, cr, {0, 0, 1, 1}, {0.15f, 0.15f, 0.15f, 1.0f});

        input->TickCaret();

        if (input->GetFont()) {
            std::string displayText = input->GetText().empty() ? input->GetPlaceholder() : input->GetText();
            Vector4 textColor = input->GetText().empty()
                ? Vector4{0.5f, 0.5f, 0.5f, 1.0f}
                : input->GetTextColor();

            float lineH = input->GetFont()->GetLineHeight();
            float ascender = input->GetFont()->GetAscender();
            float textX0 = cr.x + 4.0f;
            float baselineY = 0.0f;
            float caretY = 0.0f;

            auto* textArea = dynamic_cast<UITextArea*>(elem);
            if (textArea)
                textX0 -= textArea->GetScrollOffset().x;

            if (input->IsFocused() && input->HasSelection()) {
                int selB = input->GetSelBegin();
                int selE = input->GetSelEnd();
                if (textArea) {
                    for (int line = 0; line < textArea->GetLineCount(); ++line) {
                        int lStart = textArea->GetLineStart(line);
                        int lEnd = textArea->GetLineEnd(line);
                        if (selB < lEnd && selE > lStart) {
                            int rs = std::max(selB, lStart);
                            int re = std::min(selE, lEnd);
                            float sx = textX0 + input->GetCursorXAt(rs);
                            float ex = textX0 + input->GetCursorXAt(re);
                            float ly = cr.y + 4.0f + line * lineH - textArea->GetScrollOffset().y;
                            Vector4 selColor = {0.3f, 0.5f, 1.0f, 0.4f};
                            Batch(nullptr, {sx, ly, ex - sx, lineH}, {0,0,1,1}, selColor);
                        }
                    }
                } else {
                    float sy = cr.y + (cr.w - lineH) * 0.5f;
                    float sx = textX0 + input->GetCursorXAt(selB);
                    float ex = textX0 + input->GetCursorXAt(selE);
                    Batch(nullptr, {sx, sy, ex - sx, lineH}, {0,0,1,1}, {0.3f, 0.5f, 1.0f, 0.4f});
                }
            }

            if (textArea && textArea->IsWordWrapEnabled()) {
                const float scrollY = textArea->GetScrollOffset().y;
                const std::string& full = input->GetText();
                if (!full.empty()) {
                    for (int line = 0; line < textArea->GetLineCount(); ++line) {
                        int ls = textArea->GetLineStart(line);
                        int le = textArea->GetLineEnd(line);
                        baselineY = cr.y + 4.0f + ascender - scrollY + line * lineH;
                        if (le <= ls) continue;
                        auto rowQuads = input->GetFont()->LayoutText(full.substr(ls, le - ls), 0.0f);
                        for (size_t i = 0; i < rowQuads.size(); i += 2) {
                            const auto& r = rowQuads[i];
                            const auto& uv = rowQuads[i + 1];
                            Vector4 textRect = {textX0 + r.x, baselineY + r.y, r.z, r.w};
                            Batch(input->GetFont()->GetAtlasTexture(), textRect, uv, textColor);
                        }
                    }
                } else if (!input->GetPlaceholder().empty()) {
                    baselineY = cr.y + 4.0f + ascender - scrollY;
                    auto rowQuads = input->GetFont()->LayoutText(input->GetPlaceholder(), 0.0f);
                    for (size_t i = 0; i < rowQuads.size(); i += 2) {
                        const auto& r = rowQuads[i];
                        const auto& uv = rowQuads[i + 1];
                        Vector4 textRect = {textX0 + r.x, baselineY + r.y, r.z, r.w};
                        Batch(input->GetFont()->GetAtlasTexture(), textRect, uv, textColor);
                    }
                }
            } else {
                if (textArea) {
                    baselineY = cr.y + 4.0f + ascender - textArea->GetScrollOffset().y;
                } else {
                    baselineY = cr.y + (cr.w - lineH) * 0.5f + ascender;
                }
                float layoutWidth = textArea ? 0.0f : (cr.z - 8.0f);
                auto rawQuads = input->GetFont()->LayoutText(displayText, layoutWidth);
                for (size_t i = 0; i < rawQuads.size(); i += 2) {
                    const auto& r = rawQuads[i];
                    const auto& uv = rawQuads[i + 1];
                    Vector4 textRect = {textX0 + r.x, baselineY + r.y, r.z, r.w};
                    if (showQuadDebug)
                        Batch(nullptr, textRect, {0, 0, 1, 1}, quadColors[(i / 2) % 4]);
                    Batch(input->GetFont()->GetAtlasTexture(), textRect, uv, textColor);
                }
            }

            if (input->IsCaretVisible()) {
                float cursorX = input->GetCursorX();
                float caretX = textX0 + cursorX;
                if (textArea) {
                    caretY = cr.y + 4.0f + textArea->GetCursorLine() * lineH - textArea->GetScrollOffset().y;
                } else {
                    caretY = cr.y + (cr.w - lineH) * 0.5f;
                }
                Batch(nullptr, {caretX, caretY, 1.0f, lineH}, {0,0,1,1}, {1.0f, 1.0f, 1.0f, 1.0f});
            }
        }
    }

    if (LeirSettings::Get().debug.ui_outlines) {
        static const Vector4 debugOutlineColor = {0.0f, 1.0f, 0.0f, 1.0f};
        // Hairlines in PHYSICAL pixels: 1 logical px at 125% is 1.25 physical px,
        // which floor/ceil snapping inflates to 2 px whenever the position is
        // fractional. Divide by the scale so the line is exactly 1 physical px,
        // and derive it from the ELEMENT'S ALREADY-SNAPPED rect (round-snapped,
        // same as BuildBatch applies to the fill) — the line then lands on
        // integer physical edges and BuildBatch's snap is a no-op on it. Result:
        // a stable 1-px outline at any DPI, aligned with the element's own edge.
        //
        // Layer: controlled by LEIR_UI_OUTLINES_ON_TOP (see the define at the top
        // of this file). Default = on top of everything (BuildBatchDebug) so no
        // content — including the viewport's RenderTexture quad, which draws in
        // the middle layer and would otherwise cover the viewport's own outline —
        // can hide an outline. Set to 0 to use Batch (regular layer, the old
        // behavior) when the on-top lines overlap too much to read.
        //
        // Snap: controlled by LEIR_UI_OUTLINE_SNAP (0 = round, 1 = enclose,
        // 2 = hybrid, 3 = original). Enclose (default) guarantees the content
        // never sticks out of the outline; round keeps adjacent double-lines
        // separate but can leave a 1-px sliver of text outside; original is the
        // pre-snapping behavior (exact logical rect, 1 logical px). See the
        // define for the trilemma.
        Vector4 sr;
        float t = 1.0f / m_ContentScale;
#if LEIR_UI_OUTLINE_SNAP == 0
        sr = SnapToPhysicalPixels(cr, m_ContentScale);
#elif LEIR_UI_OUTLINE_SNAP == 1
        sr = SnapToPhysicalPixelsEnclose(cr, m_ContentScale);
#elif LEIR_UI_OUTLINE_SNAP == 2
        // Hybrid: enclose for text-bearing elements (content sits near the left
        // edge and is drawn sub-pixel), round for structural elements (dock,
        // tabs, buttons) whose adjacency must stay visible as separate lines.
        sr = (dynamic_cast<UILabel*>(elem) || dynamic_cast<UITextInput*>(elem) ||
              dynamic_cast<UITextArea*>(elem))
                 ? SnapToPhysicalPixelsEnclose(cr, m_ContentScale)
                 : SnapToPhysicalPixels(cr, m_ContentScale);
#elif LEIR_UI_OUTLINE_SNAP == 3
        // Original (last committed/pushed behavior): NO snapping — the exact
        // logical rect and a 1 LOGICAL px hairline. At fractional DPI this is
        // 1.25 physical px, so lines render 1 or 2 px depending on alignment.
        sr = cr;
        t = 1.0f;
#else
        #error "LEIR_UI_OUTLINE_SNAP: valor inválido (0=round 1=enclose 2=hybrid 3=original)"
#endif
#if LEIR_UI_OUTLINES_ON_TOP
        BuildBatchDebug(nullptr, {sr.x, sr.y, sr.z, t}, {0,0,1,1}, debugOutlineColor);
        BuildBatchDebug(nullptr, {sr.x, sr.y + sr.w - t, sr.z, t}, {0,0,1,1}, debugOutlineColor);
        BuildBatchDebug(nullptr, {sr.x, sr.y, t, sr.w}, {0,0,1,1}, debugOutlineColor);
        BuildBatchDebug(nullptr, {sr.x + sr.z - t, sr.y, t, sr.w}, {0,0,1,1}, debugOutlineColor);
#else
        Batch(nullptr, {sr.x, sr.y, sr.z, t}, {0,0,1,1}, debugOutlineColor);
        Batch(nullptr, {sr.x, sr.y + sr.w - t, sr.z, t}, {0,0,1,1}, debugOutlineColor);
        Batch(nullptr, {sr.x, sr.y, t, sr.w}, {0,0,1,1}, debugOutlineColor);
        Batch(nullptr, {sr.x + sr.z - t, sr.y, t, sr.w}, {0,0,1,1}, debugOutlineColor);
#endif
    }

    m_CurrentClip = clip;
    for (auto* child : elem->GetChildren())
        RenderElement(child, effClip, isDebug);

    // Window visible border: 4 thin quads drawn on top of the window's content
    // (after the children loop). m_VisualBorderSize = 0 disables it. The border
    // is purely decorative — the resize/cursor events come from the expanded
    // hit ring (UIWindow::GetHitRect), not from these quads.
    if (auto* window = dynamic_cast<UIWindow*>(elem)) {
        const float bs = window->GetVisualBorderSize();
        if (bs > 0.0f) {
            const Vector4& bc = window->GetBorderColor();
            const auto& cr2 = elem->GetComputedRect();
            const float x0 = cr2.x, y0 = cr2.y, x1 = cr2.x + cr2.z, y1 = cr2.y + cr2.w;
            Batch(nullptr, {x0, y0, x1 - x0, bs}, {0, 0, 1, 1}, bc);       // top
            Batch(nullptr, {x0, y1 - bs, x1 - x0, bs}, {0, 0, 1, 1}, bc);  // bottom
            Batch(nullptr, {x0, y0, bs, y1 - y0}, {0, 0, 1, 1}, bc);       // left
            Batch(nullptr, {x1 - bs, y0, bs, y1 - y0}, {0, 0, 1, 1}, bc);  // right
        }
    }
}

void UIRenderer::ApplyScissor(RHI::GCommandGraph& graph, const Vector4& logicalClip, RHI::RHIRect2D& last, bool& valid)
{
    RHI::RHIRect2D s{};
    const float pw = m_ScreenSize.x * m_ContentScale;
    const float ph = m_ScreenSize.y * m_ContentScale;
    ScissorFromLogicalClip(logicalClip, m_ContentScale, pw, ph, s);

    if (!valid || s.x != last.x || s.y != last.y ||
        s.width != last.width || s.height != last.height) {
        graph.SetScissor(s);
        last = s;
        valid = true;
    }
}

} // namespace Leir
