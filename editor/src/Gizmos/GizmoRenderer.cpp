#include "GizmoRenderer.h"

#include "LeirEngine/Core/Log.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"

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

constexpr float kPi = 3.14159265358979323846f;

} // namespace

namespace {

using namespace Leir;

} // namespace

GizmoRenderer::GizmoRenderer(Leir::RHI::RenderBackend* device,
                             Leir::RHI::RHIRenderPass viewportRenderPass)
    : m_Device(device)
{
    // Double-buffered dynamic vertex buffer (same pattern as UIRenderer): the
    // CPU writes frame N's buffer while the GPU reads frame N-1's, so there is
    // no sync stall.
    const uint32_t vbSize = kMaxVertices * (uint32_t)sizeof(GizmoVertex);
    const uint32_t solidVbSize = kMaxSolidVertices * (uint32_t)sizeof(SolidVertex);
    for (int f = 0; f < kFrames; ++f) {
        m_VertexBuffers[f] = m_Device->CreateBuffer(vbSize,
            Leir::RHI::BufferUsage::Vertex,
            Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
            m_VertexMemories[f]);
        m_SolidVertexBuffers[f] = m_Device->CreateBuffer(solidVbSize,
            Leir::RHI::BufferUsage::Vertex,
            Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
            m_SolidVertexMemories[f]);
    }
    m_Quads.reserve(4096);
    m_SolidVerts.reserve(4096);

    CreatePipeline(viewportRenderPass);
    CreateSolidPipeline(viewportRenderPass);
}

GizmoRenderer::~GizmoRenderer()
{
    DestroyResources();
}

void GizmoRenderer::BeginFrame()
{
    m_Lines.clear();
    m_SolidVerts.clear();
    m_OverflowLogged = false;
}

void GizmoRenderer::DrawLine(const Leir::Vector3& a, const Leir::Vector3& b,
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

void GizmoRenderer::DrawBox(const Leir::Vector3& center, const Leir::Vector3& size,
                            const Leir::Vector4& color, float widthPx)
{
    const Leir::Vector3 h = size * 0.5f;
    const Leir::Vector3 c[8] = {
        center + Leir::Vector3(-h.x, -h.y, -h.z),
        center + Leir::Vector3(h.x, -h.y, -h.z),
        center + Leir::Vector3(h.x, -h.y, h.z),
        center + Leir::Vector3(-h.x, -h.y, h.z),
        center + Leir::Vector3(-h.x, h.y, -h.z),
        center + Leir::Vector3(h.x, h.y, -h.z),
        center + Leir::Vector3(h.x, h.y, h.z),
        center + Leir::Vector3(-h.x, h.y, h.z),
    };
    // 12 edges of the wireframe box.
    const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, // bottom
        {4,5}, {5,6}, {6,7}, {7,4}, // top
        {0,4}, {1,5}, {2,6}, {3,7}, // verticals
    };
    for (int i = 0; i < 12; ++i)
        DrawLine(c[edges[i][0]], c[edges[i][1]], color, widthPx);
}

void GizmoRenderer::DrawCircle(const Leir::Vector3& center, float radius,
                               const Leir::Vector3& normal, const Leir::Vector4& color,
                               int segments, float widthPx)
{
    if (segments < 3)
        segments = 3;
    const Leir::Vector3 n = normal.Normalized();

    // Orthonormal basis around the normal.
    Leir::Vector3 b0 = Leir::Vector3::Cross(n,
        std::fabs(n.y) < 0.9f ? Leir::Vector3::Up() : Leir::Vector3::Right());
    b0.Normalize();
    const Leir::Vector3 b1 = Leir::Vector3::Cross(n, b0);

    Leir::Vector3 prev = center + (b0 * radius);
    for (int i = 1; i <= segments; ++i) {
        const float a = (float)i / (float)segments * 2.0f * kPi;
        const Leir::Vector3 cur = center + (b0 * (std::cos(a) * radius)) +
                                  (b1 * (std::sin(a) * radius));
        DrawLine(prev, cur, color, widthPx);
        prev = cur;
    }
}

void GizmoRenderer::DrawSphere(const Leir::Vector3& center, float radius,
                               const Leir::Vector4& color, int segments, float widthPx)
{
    // Wireframe sphere = 3 orthogonal rings (XY, XZ, YZ planes).
    DrawCircle(center, radius, Leir::Vector3::Up(), color, segments, widthPx);
    DrawCircle(center, radius, Leir::Vector3::Right(), color, segments, widthPx);
    DrawCircle(center, radius, Leir::Vector3::Forward(), color, segments, widthPx);
}

void GizmoRenderer::DrawTriangle(const Leir::Vector3& a, const Leir::Vector3& b,
                                 const Leir::Vector3& c, const Leir::Vector4& color)
{
    if (m_SolidVerts.size() + 3 > kMaxSolidVertices) {
        if (!m_OverflowLogged) {
            Leir::XConsole::Debug("Gizmo solid vertex overflow (dropping)");
            m_OverflowLogged = true;
        }
        return;
    }
    SolidVertex va; va.position = a; va.color = color; m_SolidVerts.push_back(va);
    SolidVertex vb; vb.position = b; vb.color = color; m_SolidVerts.push_back(vb);
    SolidVertex vc; vc.position = c; vc.color = color; m_SolidVerts.push_back(vc);
}

void GizmoRenderer::DrawQuadFilled(const Leir::Vector3& a, const Leir::Vector3& b,
                                   const Leir::Vector3& c, const Leir::Vector3& d,
                                   const Leir::Vector4& color)
{
    DrawTriangle(a, b, c, color);
    DrawTriangle(a, c, d, color);
}

void GizmoRenderer::DrawCubeFilled(const Leir::Vector3& center, const Leir::Vector3& size,
                                   const Leir::Vector4& color)
{
    DrawCubeFilledOriented(center, size, Leir::Quaternion::Identity(), color);
}

void GizmoRenderer::DrawCubeFilledOriented(const Leir::Vector3& center, const Leir::Vector3& size,
                                           const Leir::Quaternion& rotation,
                                           const Leir::Vector4& color)
{
    const Leir::Vector3 h = size * 0.5f;
    const Leir::Vector3 c0(center.x - h.x, center.y - h.y, center.z - h.z);
    const Leir::Vector3 c1(center.x + h.x, center.y - h.y, center.z - h.z);
    const Leir::Vector3 c2(center.x + h.x, center.y - h.y, center.z + h.z);
    const Leir::Vector3 c3(center.x - h.x, center.y - h.y, center.z + h.z);
    const Leir::Vector3 c4(center.x - h.x, center.y + h.y, center.z - h.z);
    const Leir::Vector3 c5(center.x + h.x, center.y + h.y, center.z - h.z);
    const Leir::Vector3 c6(center.x + h.x, center.y + h.y, center.z + h.z);
    const Leir::Vector3 c7(center.x - h.x, center.y + h.y, center.z + h.z);

    // Rotate the 8 corners around `center` so the cube follows the gizmo's
    // orientation (the scale handle cubes behave like the translate cones).
    const Leir::Vector3 corners[8] = {
        center + rotation * (c0 - center), center + rotation * (c1 - center),
        center + rotation * (c2 - center), center + rotation * (c3 - center),
        center + rotation * (c4 - center), center + rotation * (c5 - center),
        center + rotation * (c6 - center), center + rotation * (c7 - center),
    };

    // Per-face shading so the 3D cube reads correctly even at small sizes.
    const Leir::Vector4 shadeUp = color;
    const Leir::Vector4 shadeDown = color * 0.6f;
    const Leir::Vector4 shadeFront = color * 0.8f;
    const Leir::Vector4 shadeBack = color * 0.5f;
    const Leir::Vector4 shadeRight = color * 0.9f;
    const Leir::Vector4 shadeLeft = color * 0.55f;

    DrawQuadFilled(corners[4], corners[5], corners[6], corners[7], shadeUp);    // +Y
    DrawQuadFilled(corners[0], corners[3], corners[2], corners[1], shadeDown);  // -Y
    DrawQuadFilled(corners[3], corners[7], corners[6], corners[2], shadeFront); // +Z
    DrawQuadFilled(corners[0], corners[1], corners[5], corners[4], shadeBack);  // -Z
    DrawQuadFilled(corners[1], corners[2], corners[6], corners[5], shadeRight); // +X
    DrawQuadFilled(corners[0], corners[4], corners[7], corners[3], shadeLeft);  // -X
}

void GizmoRenderer::DrawCone(const Leir::Vector3& baseCenter, float baseRadius,
                             const Leir::Vector3& tip, const Leir::Vector4& color,
                             int segments)
{
    if (segments < 3)
        segments = 3;
    if (baseRadius <= 0.0f || (tip - baseCenter).SqrLength() < 1e-12f)
        return;

    const Leir::Vector3 axis = (tip - baseCenter).Normalized();
    Leir::Vector3 b0 = Leir::Vector3::Cross(axis,
        std::fabs(axis.y) < 0.9f ? Leir::Vector3::Up() : Leir::Vector3::Right());
    b0.Normalize();
    const Leir::Vector3 b1 = Leir::Vector3::Cross(axis, b0);

    // Side (triangle fan base circle -> tip). Winding picked so the front face
    // is visible from outside (cull is disabled, but keep it correct anyway).
    for (int i = 0; i < segments; ++i) {
        const float a0 = (float)i / (float)segments * 2.0f * kPi;
        const float a1 = (float)(i + 1) / (float)segments * 2.0f * kPi;
        const Leir::Vector3 p0 = baseCenter + (b0 * (std::cos(a0) * baseRadius)) +
                                 (b1 * (std::sin(a0) * baseRadius));
        const Leir::Vector3 p1 = baseCenter + (b0 * (std::cos(a1) * baseRadius)) +
                                 (b1 * (std::sin(a1) * baseRadius));
        DrawTriangle(p0, tip, p1, color);
    }
    // Base disc (fan from center).
    for (int i = 0; i < segments; ++i) {
        const float a0 = (float)i / (float)segments * 2.0f * kPi;
        const float a1 = (float)(i + 1) / (float)segments * 2.0f * kPi;
        const Leir::Vector3 p0 = baseCenter + (b0 * (std::cos(a0) * baseRadius)) +
                                 (b1 * (std::sin(a0) * baseRadius));
        const Leir::Vector3 p1 = baseCenter + (b0 * (std::cos(a1) * baseRadius)) +
                                 (b1 * (std::sin(a1) * baseRadius));
        DrawTriangle(p0, p1, baseCenter, color);
    }
}

void GizmoRenderer::Render(Leir::RHI::GCommandGraph& graph,
                           const Leir::Matrix4x4& viewProjection,
                           float viewportWidthPx, float viewportHeightPx)
{
    if (!m_Device || !m_Pipeline.IsValid() ||
        (m_Lines.empty() && m_SolidVerts.empty()))
        return;

    const uint32_t frame = m_Device->GetCurrentFrameIndex();

    // UBO (viewProjection only).
    GizmoUBO ubo;
    ubo.viewProjection = viewProjection;
    void* data = nullptr;
    m_Device->MapMemory(m_UBOMemories[frame], 0, (uint32_t)sizeof(GizmoUBO), &data);
    std::memcpy(data, &ubo, sizeof(GizmoUBO));
    m_Device->UnmapMemory(m_UBOMemories[frame]);

    // Expand lines into quads. Each quad is 4 strip corners; a single
    // TriangleStrip over all quads would stitch a spurious triangle between
    // consecutive quads, so each quad (except the last) is followed by TWO
    // degenerate vertices that close the strip: a repeat of this quad's LAST
    // corner, then a repeat of the NEXT quad's FIRST corner (the standard
    // strip-restart pair). Emitting two copies of the last corner instead
    // leaves a spurious (A3, B0, B1) triangle between the quads.
    //
    // Segments that cross the near plane are CLIPPED to it (a negative clip w
    // would flip the quad), so a line stays visible until the camera actually
    // crosses its plane instead of vanishing as an endpoint goes behind the
    // near plane. Fully-behind segments are dropped. The clip intersection
    // parameter t (clip.w is linear along the world segment) is reused to
    // interpolate the WORLD endpoints, keeping the VS math unchanged.
    //
    // Lines are drawn FAR-TO-NEAR (the pipeline has depth WRITE disabled, so
    // draw order decides overlaps): at a grazing camera the projections of
    // coplanar lines overlap, and the NEAREST line must win the shared pixels
    // — sorting by the segment's closest view-space distance makes the nearer
    // line draw last and cleanly cover the farther one (no red/blue "mixing").
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
            GizmoVertex v;
            v.start = seg.s;
            v.end = seg.e;
            v.color = seg.color;
            v.cornerX = kCornerX[c];
            v.cornerY = kCornerY[c];
            v.width = seg.width;
            m_Quads.push_back(v);
        }
        if (li + 1 < segs.size()) {
            // Strip restart: repeat this quad's last corner (A3), then the
            // next quad's first corner (B0). Both are zero-area connectors.
            const auto& next = segs[li + 1];
            GizmoVertex a3;
            a3.start = seg.s;
            a3.end = seg.e;
            a3.color = seg.color;
            a3.cornerX = kCornerX[3];
            a3.cornerY = kCornerY[3];
            a3.width = seg.width;
            m_Quads.push_back(a3);

            GizmoVertex b0;
            b0.start = next.s;
            b0.end = next.e;
            b0.color = next.color;
            b0.cornerX = kCornerX[0];
            b0.cornerY = kCornerY[0];
            b0.width = next.width;
            m_Quads.push_back(b0);
        }
    }

    const uint32_t vbBytes = (uint32_t)(m_Quads.size() * sizeof(GizmoVertex));
    if (!m_Quads.empty()) {
        m_Device->MapMemory(m_VertexMemories[frame], 0, vbBytes, &data);
        std::memcpy(data, m_Quads.data(), vbBytes);
        m_Device->UnmapMemory(m_VertexMemories[frame]);

        graph.BindPipeline(m_Pipeline);
        graph.BindDescriptorSets(m_PipelineLayout, 0, { m_UBOSets[frame] });
        graph.BindVertexBuffer(m_VertexBuffers[frame]);

        GizmoPushConstants push;
        push.viewportWidth = viewportWidthPx;
        push.viewportHeight = viewportHeightPx;
        graph.PushConstants(m_PipelineLayout, Leir::RHI::ShaderStageMask::Vertex,
            0, (uint32_t)sizeof(GizmoPushConstants), &push);

        graph.Draw((uint32_t)m_Quads.size(), 0);
    }

    // ---- Solid (filled) geometry: plain triangles, same UBO set, its own
    // pipeline (GizmoSolid). Depth-tested against the scene, depth write off.
    if (!m_SolidVerts.empty() && m_SolidPipeline.IsValid()) {
        const uint32_t solidBytes = (uint32_t)(m_SolidVerts.size() * sizeof(SolidVertex));
        m_Device->MapMemory(m_SolidVertexMemories[frame], 0, solidBytes, &data);
        std::memcpy(data, m_SolidVerts.data(), solidBytes);
        m_Device->UnmapMemory(m_SolidVertexMemories[frame]);

        graph.BindPipeline(m_SolidPipeline);
        graph.BindDescriptorSets(m_SolidPipelineLayout, 0, { m_UBOSets[frame] });
        graph.BindVertexBuffer(m_SolidVertexBuffers[frame]);
        graph.Draw((uint32_t)m_SolidVerts.size(), 0);
    }
}

void GizmoRenderer::CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass)
{
    const std::string shaderDir = LEIR_SHADER_DIR;
    const std::string ext = m_Device->GetShaderFileExtension();
    const std::string vertPath = shaderDir + "/Gizmo.vert" + ext;
    const std::string fragPath = shaderDir + "/Gizmo.frag" + ext;

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
        pushRange.size = (uint32_t)sizeof(GizmoPushConstants);
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
        m_UBOBuffers[i] = m_Device->CreateBuffer((uint32_t)sizeof(GizmoUBO),
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
        write.bufferInfo.range = (uint32_t)sizeof(GizmoUBO);
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
    desc.depthTestEnable = true;
    desc.depthWriteEnable = false; // gizmo lines only test against the scene,
                                   // not each other (later-drawn wins overlaps
                                   // -> no red/white zippering at grazing views)
    desc.blend.enable = true;
    m_Pipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);

    Leir::XConsole::Println("Gizmo renderer pipeline created ({})", ext);
}

void GizmoRenderer::CreateSolidPipeline(Leir::RHI::RHIRenderPass viewportRenderPass)
{
    const std::string shaderDir = LEIR_SHADER_DIR;
    const std::string ext = m_Device->GetShaderFileExtension();
    const std::string vertPath = shaderDir + "/GizmoSolid.vert" + ext;
    const std::string fragPath = shaderDir + "/GizmoSolid.frag" + ext;

    // Reuse the same UBO (set 0, binding 0 = viewProjection) as the line
    // pipeline: GizmoSolid declares the identical cbuffer, so the descriptor
    // set layout/pool/sets are shared (no extra descriptors needed).
    m_SolidPipelineLayout = m_Device->CreatePipelineLayout({ m_UBOLayout }, {});

    auto vertCode = Leir::Shader::ReadFile(vertPath);
    auto fragCode = Leir::Shader::ReadFile(fragPath);
    if (vertCode.empty() || fragCode.empty()) {
        Leir::XConsole::PrintWarning("GizmoSolid shaders not found ({} / {})", vertPath, fragPath);
        return;
    }
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
    desc.layout = m_SolidPipelineLayout;
    desc.renderPass = viewportRenderPass;
    desc.stages = { stages[0], stages[1] };
    desc.vertexBinding = GetSolidBindingDescription();
    desc.vertexAttributes = GetSolidAttributeDescriptions();
    desc.topology = Leir::RHI::Topology::TriangleList;
    desc.polygonMode = Leir::RHI::PolygonMode::Fill;
    desc.cullMode = Leir::RHI::CullMode::None;
    desc.depthTestEnable = true;
    desc.depthWriteEnable = false; // gizmos never write depth
    desc.blend.enable = true;
    m_SolidPipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);

    Leir::XConsole::Println("Gizmo solid pipeline created ({})", ext);
}

Leir::RHI::RHIVertexInputBinding GizmoRenderer::GetSolidBindingDescription()
{
    Leir::RHI::RHIVertexInputBinding binding;
    binding.binding = 0;
    binding.stride = sizeof(SolidVertex); // 28
    binding.inputRate = Leir::RHI::VertexInputRate::Vertex;
    return binding;
}

std::vector<Leir::RHI::RHIVertexAttribute> GizmoRenderer::GetSolidAttributeDescriptions()
{
    std::vector<Leir::RHI::RHIVertexAttribute> attrs(2);

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(SolidVertex, position);
    attrs[0].semantic = "POSITION";
    attrs[0].semanticIndex = 0;

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = Leir::RHI::Format::R32G32B32A32_SFLOAT;
    attrs[1].offset = offsetof(SolidVertex, color);
    attrs[1].semantic = "COLOR";
    attrs[1].semanticIndex = 0;

    return attrs;
}

Leir::RHI::RHIVertexInputBinding GizmoRenderer::GetBindingDescription()
{
    Leir::RHI::RHIVertexInputBinding binding;
    binding.binding = 0;
    binding.stride = sizeof(GizmoVertex); // 56
    binding.inputRate = Leir::RHI::VertexInputRate::Vertex;
    return binding;
}

std::vector<Leir::RHI::RHIVertexAttribute> GizmoRenderer::GetAttributeDescriptions()
{
    std::vector<Leir::RHI::RHIVertexAttribute> attrs(6);

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(GizmoVertex, start);
    attrs[0].semantic = "POSITION";
    attrs[0].semanticIndex = 0;

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(GizmoVertex, end);
    attrs[1].semantic = "TEXCOORD";
    attrs[1].semanticIndex = 0;

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = Leir::RHI::Format::R32G32B32A32_SFLOAT;
    attrs[2].offset = offsetof(GizmoVertex, color);
    attrs[2].semantic = "COLOR";
    attrs[2].semanticIndex = 0;

    attrs[3].binding = 0;
    attrs[3].location = 3;
    attrs[3].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[3].offset = offsetof(GizmoVertex, cornerX);
    attrs[3].semantic = "TEXCOORD";
    attrs[3].semanticIndex = 1;

    attrs[4].binding = 0;
    attrs[4].location = 4;
    attrs[4].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[4].offset = offsetof(GizmoVertex, cornerY);
    attrs[4].semantic = "TEXCOORD";
    attrs[4].semanticIndex = 2;

    attrs[5].binding = 0;
    attrs[5].location = 5;
    attrs[5].format = Leir::RHI::Format::R32_SFLOAT;
    attrs[5].offset = offsetof(GizmoVertex, width);
    attrs[5].semantic = "TEXCOORD";
    attrs[5].semanticIndex = 3;

    return attrs;
}

void GizmoRenderer::DestroyResources()
{
    if (m_Pipeline.IsValid())
        m_Device->DestroyPipeline(m_Pipeline);
    if (m_PipelineLayout.IsValid())
        m_Device->DestroyPipelineLayout(m_PipelineLayout);
    if (m_SolidPipeline.IsValid())
        m_Device->DestroyPipeline(m_SolidPipeline);
    if (m_SolidPipelineLayout.IsValid())
        m_Device->DestroyPipelineLayout(m_SolidPipelineLayout);
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
        if (m_SolidVertexBuffers[i].IsValid())
            m_Device->DestroyBuffer(m_SolidVertexBuffers[i]);
        if (m_SolidVertexMemories[i].IsValid())
            m_Device->DestroyMemory(m_SolidVertexMemories[i]);
    }
}
