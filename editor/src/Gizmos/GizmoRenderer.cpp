#include "GizmoRenderer.h"

#include "LeirEngine/Core/Log.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"

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
    for (int f = 0; f < kFrames; ++f) {
        m_VertexBuffers[f] = m_Device->CreateBuffer(vbSize,
            Leir::RHI::BufferUsage::Vertex,
            Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
            m_VertexMemories[f]);
    }
    m_Quads.reserve(4096);

    CreatePipeline(viewportRenderPass);
}

GizmoRenderer::~GizmoRenderer()
{
    DestroyResources();
}

void GizmoRenderer::BeginFrame()
{
    m_Lines.clear();
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
        std::fabsf(n.y) < 0.9f ? Leir::Vector3::Up() : Leir::Vector3::Right());
    b0.Normalize();
    const Leir::Vector3 b1 = Leir::Vector3::Cross(n, b0);

    Leir::Vector3 prev = center + (b0 * radius);
    for (int i = 1; i <= segments; ++i) {
        const float a = (float)i / (float)segments * 2.0f * kPi;
        const Leir::Vector3 cur = center + (b0 * (std::cosf(a) * radius)) +
                                  (b1 * (std::sinf(a) * radius));
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

void GizmoRenderer::Render(Leir::RHI::GCommandGraph& graph,
                           const Leir::Matrix4x4& viewProjection,
                           float viewportWidthPx, float viewportHeightPx)
{
    if (!m_Device || !m_Pipeline.IsValid() || m_Lines.empty())
        return;

    const uint32_t frame = m_Device->GetCurrentFrameIndex();

    // UBO (viewProjection only).
    GizmoUBO ubo;
    ubo.viewProjection = viewProjection;
    void* data = nullptr;
    m_Device->MapMemory(m_UBOMemories[frame], 0, (uint32_t)sizeof(GizmoUBO), &data);
    std::memcpy(data, &ubo, sizeof(GizmoUBO));
    m_Device->UnmapMemory(m_UBOMemories[frame]);

    // Expand lines into quads, skipping segments behind the near plane (a
    // negative clip w would flip the quad).
    const glm::mat4 vp = ubo.viewProjection;
    m_Quads.clear();
    for (const auto& line : m_Lines) {
        const glm::vec3 s(line.start);
        const glm::vec3 e(line.end);
        const glm::vec4 clipS = vp * glm::vec4(s, 1.0f);
        const glm::vec4 clipE = vp * glm::vec4(e, 1.0f);
        if (clipS.w <= 0.0f || clipE.w <= 0.0f)
            continue;
        for (int c = 0; c < 4; ++c) {
            GizmoVertex v;
            v.start = line.start;
            v.end = line.end;
            v.color = line.color;
            v.cornerX = kCornerX[c];
            v.cornerY = kCornerY[c];
            v.width = line.width;
            m_Quads.push_back(v);
        }
    }
    if (m_Quads.empty())
        return;

    const uint32_t vbBytes = (uint32_t)(m_Quads.size() * sizeof(GizmoVertex));
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
    desc.blend.enable = true;
    m_Pipeline = m_Device->CreateGraphicsPipeline(desc);

    m_Device->DestroyShaderModule(vertMod);
    m_Device->DestroyShaderModule(fragMod);

    Leir::XConsole::Println("Gizmo renderer pipeline created ({})", ext);
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
