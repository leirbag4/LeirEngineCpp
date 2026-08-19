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

// Grid geometry (procedural, Unity-style LOD by distance from the camera).
// Minor lines every 1u are dim and semi-transparent; chunk lines every 10u are
// brighter and reach the horizon. Both fade smoothly OUTWARD from the camera
// (near the camera the fine 1x1 squares are visible inside each 10x10 chunk,
// and they grow more transparent as they recede / as the camera zooms out).
constexpr float kMinorSpacing = 1.0f;
constexpr float kMinorExtent = 60.0f;   // half-window (world units)
constexpr float kMinorWidth = 1.5f;     // px
constexpr float kMinorMaxAlpha = 0.40f; // never fully opaque (Unity-like)
constexpr float kMinorFadeStart = 8.0f;
constexpr float kMinorFadeEnd = 32.0f;

constexpr float kMajorSpacing = 10.0f;
constexpr float kMajorExtent = 160.0f;  // half-window (world units)
constexpr float kMajorWidth = 3.0f;     // px
constexpr float kMajorMaxAlpha = 0.90f;
constexpr float kMajorFadeStart = 40.0f;
constexpr float kMajorFadeEnd = 100.0f; // matches the camera far plane

constexpr float kAxisExtent = 500.0f;   // the axes reach the horizon at any zoom
constexpr float kAxisWidth = 2.0f;      // px

const Leir::Vector4 kMinorColor(0.30f, 0.32f, 0.36f, 1.0f);   // base grid lines
const Leir::Vector4 kMajorColor(0.85f, 0.88f, 0.96f, 1.0f);  // chunk lines, near-white
const Leir::Vector4 kAxisXColor(0.95f, 0.25f, 0.25f, 1.0f);  // red: X axis
const Leir::Vector4 kAxisZColor(0.30f, 0.55f, 1.0f, 1.0f);   // blue: Z axis

// Smooth distance fade: maxAlpha inside fadeStart, 0 at fadeEnd (smoothstep).
float FadeAlpha(float dist, float fadeStart, float fadeEnd, float maxAlpha)
{
    if (dist <= fadeStart)
        return maxAlpha;
    if (dist >= fadeEnd)
        return 0.0f;
    const float t = (dist - fadeStart) / (fadeEnd - fadeStart);
    const float s = t * t * (3.0f - 2.0f * t);
    return maxAlpha * (1.0f - s);
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

void EditorGrid::GenerateLines(const Leir::Vector3& cameraPos)
{
    const float camX = cameraPos.x;
    const float camZ = cameraPos.z;

    // Minor lines every 1u, parallel to Z at x=k (skipping chunk positions and
    // the axes so nothing is drawn twice on top of itself).
    const int minorX0 = (int)std::floor(camX - kMinorExtent);
    const int minorX1 = (int)std::ceil(camX + kMinorExtent);
    for (int k = minorX0; k <= minorX1; ++k) {
        if (k == 0)
            continue;        // the blue Z axis covers x==0
        if (k % 10 == 0)
            continue;        // the chunk line covers x==k
        const float alpha = FadeAlpha(std::fabs((float)k - camX),
            kMinorFadeStart, kMinorFadeEnd, kMinorMaxAlpha);
        if (alpha < 0.02f)
            continue;
        Leir::Vector4 c = kMinorColor;
        c.w = alpha;
        DrawLine({ (float)k, kGridY, camZ - kMinorExtent },
                 { (float)k, kGridY, camZ + kMinorExtent }, c, kMinorWidth);
    }
    // Minor lines parallel to X at z=k.
    const int minorZ0 = (int)std::floor(camZ - kMinorExtent);
    const int minorZ1 = (int)std::ceil(camZ + kMinorExtent);
    for (int k = minorZ0; k <= minorZ1; ++k) {
        if (k == 0)
            continue;        // the red X axis covers z==0
        if (k % 10 == 0)
            continue;
        const float alpha = FadeAlpha(std::fabs((float)k - camZ),
            kMinorFadeStart, kMinorFadeEnd, kMinorMaxAlpha);
        if (alpha < 0.02f)
            continue;
        Leir::Vector4 c = kMinorColor;
        c.w = alpha;
        DrawLine({ camX - kMinorExtent, kGridY, (float)k },
                 { camX + kMinorExtent, kGridY, (float)k }, c, kMinorWidth);
    }

    // Chunk lines every 10u (the 10x10 grouping that remains when you zoom out),
    // reaching the horizon (fade ends at the camera far plane).
    const int majorX0 = (int)std::floor((camX - kMajorExtent) / kMajorSpacing);
    const int majorX1 = (int)std::ceil((camX + kMajorExtent) / kMajorSpacing);
    for (int k = majorX0; k <= majorX1; ++k) {
        const float coord = (float)k * kMajorSpacing;
        if (coord == 0.0f)
            continue;        // the blue Z axis covers x==0
        const float alpha = FadeAlpha(std::fabs(coord - camX),
            kMajorFadeStart, kMajorFadeEnd, kMajorMaxAlpha);
        if (alpha < 0.02f)
            continue;
        Leir::Vector4 c = kMajorColor;
        c.w = alpha;
        DrawLine({ coord, kGridY, camZ - kMajorExtent },
                 { coord, kGridY, camZ + kMajorExtent }, c, kMajorWidth);
    }
    const int majorZ0 = (int)std::floor((camZ - kMajorExtent) / kMajorSpacing);
    const int majorZ1 = (int)std::ceil((camZ + kMajorExtent) / kMajorSpacing);
    for (int k = majorZ0; k <= majorZ1; ++k) {
        const float coord = (float)k * kMajorSpacing;
        if (coord == 0.0f)
            continue;        // the red X axis covers z==0
        const float alpha = FadeAlpha(std::fabs(coord - camZ),
            kMajorFadeStart, kMajorFadeEnd, kMajorMaxAlpha);
        if (alpha < 0.02f)
            continue;
        Leir::Vector4 c = kMajorColor;
        c.w = alpha;
        DrawLine({ camX - kMajorExtent, kGridY, coord },
                 { camX + kMajorExtent, kGridY, coord }, c, kMajorWidth);
    }

    // Origin axes (opaque, never fade).
    DrawLine({ -kAxisExtent, kGridY, 0.0f }, { kAxisExtent, kGridY, 0.0f }, kAxisXColor, kAxisWidth);
    DrawLine({ 0.0f, kGridY, -kAxisExtent }, { 0.0f, kGridY, kAxisExtent }, kAxisZColor, kAxisWidth);
}

void EditorGrid::Render(Leir::RHI::GCommandGraph& graph,
                        const Leir::Matrix4x4& viewProjection,
                        const Leir::Vector3& cameraPos,
                        float viewportWidthPx, float viewportHeightPx)
{
    if (!m_Device || !m_Pipeline.IsValid())
        return;

    const uint32_t frame = m_Device->GetCurrentFrameIndex();

    // Procedural grid, regenerated every frame and recentered on the camera.
    BeginFrame();
    GenerateLines(cameraPos);
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