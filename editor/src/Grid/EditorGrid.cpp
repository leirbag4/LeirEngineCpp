#include "EditorGrid.h"

#include "LeirEngine/Core/Log.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Rendering/Shader.h"
#include "LeirEngine/Rendering/ShaderLayout.h"

#include <cstring>

namespace {

constexpr float kGridY = -0.01f; // slightly below 0 to avoid z-fighting with objects on the floor

// Level geometry: one flat quad spanning ±halfExtent on X and Z. The lines are
// generated in the fragment shader, so the mesh is trivial.
constexpr float kL1Extent = 100.0f;    // unit 1,   fade 30  - 80
constexpr float kL10Extent = 1000.0f;  // unit 10,  fade 60  - 600
constexpr float kL100Extent = 10000.0f;// unit 100, fade 500 - 8000
constexpr float kAxisExtent = 250.0f;  // origin axes, no fade, 2px

const Leir::Vector4 kThinColor(0.85f, 0.88f, 0.93f, 1.0f);   // base grid lines
const Leir::Vector4 kChunkColor(1.0f, 1.0f, 1.0f, 1.0f);     // every 10th line, brighter
const Leir::Vector4 kAxisXColor(0.95f, 0.25f, 0.25f, 1.0f);  // red: X axis (left-right)
const Leir::Vector4 kAxisZColor(0.30f, 0.55f, 1.0f, 1.0f);   // blue: Z axis (depth)

} // namespace

namespace {

using namespace Leir;

} // namespace

EditorGrid::EditorGrid(Leir::RHI::RenderBackend* device,
                       Leir::RHI::RHIRenderPass viewportRenderPass)
    : m_Device(device)
{
    BuildLevelMeshes();
    CreatePipeline(viewportRenderPass);
}

EditorGrid::~EditorGrid()
{
    DestroyResources();
}

void EditorGrid::Render(Leir::RHI::GCommandGraph& graph,
                        const Leir::Matrix4x4& viewProjection,
                        const Leir::Vector3& cameraPos)
{
    if (!m_Device || !m_Pipeline.IsValid())
        return;

    const uint32_t frame = m_Device->GetCurrentFrameIndex();

    GridUBO ubo;
    ubo.viewProjection = viewProjection;
    void* data = nullptr;
    m_Device->MapMemory(m_UBOMemories[frame], 0, (uint32_t)sizeof(GridUBO), &data);
    std::memcpy(data, &ubo, sizeof(GridUBO));
    m_Device->UnmapMemory(m_UBOMemories[frame]);

    graph.BindPipeline(m_Pipeline);
    graph.BindDescriptorSets(m_PipelineLayout, 0, { m_UBOSets[frame] });

    auto drawMesh = [&](const LevelMesh& mesh, const GridPushConstants& push) {
        if (mesh.indexCount <= 0)
            return;
        graph.BindVertexBuffer(mesh.vertexBuffer);
        graph.BindIndexBuffer(mesh.indexBuffer);
        graph.PushConstants(m_PipelineLayout, Leir::RHI::ShaderStageMask::VertexFragment,
            0, (uint32_t)sizeof(GridPushConstants), &push);
        graph.DrawIndexed(mesh.indexCount, 1, 0);
    };

    // Levels from small to large (near lines first, bigger quads on top where
    // their fade takes over). The axes draw last so the origin crossing reads
    // clean and never fades.
    drawMesh(m_L1, { 1.0f, 30.0f, 80.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        cameraPos, 0.0f, kThinColor, kChunkColor });
    drawMesh(m_L10, { 10.0f, 60.0f, 600.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        cameraPos, 0.0f, kThinColor, kChunkColor });
    drawMesh(m_L100, { 100.0f, 500.0f, 8000.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        cameraPos, 0.0f, kThinColor, kChunkColor });
    drawMesh(m_Axis, { 1.0f, 0.0f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        cameraPos, 0.0f, kAxisXColor, kAxisZColor });
}

void EditorGrid::BuildLevelMeshes()
{
    BuildLevel(kL1Extent, m_L1);
    BuildLevel(kL10Extent, m_L10);
    BuildLevel(kL100Extent, m_L100);
    BuildLevel(kAxisExtent, m_Axis);
}

void EditorGrid::BuildLevel(float halfExtent, LevelMesh& out)
{
    // One flat quad covering [-halfExtent, halfExtent] on X and Z. The line
    // pattern comes from the fragment shader (procedural grid).
    std::vector<GridVertex> verts;
    std::vector<uint32_t> idxs;
    verts.reserve(4);
    idxs.reserve(6);

    verts.push_back({ Leir::Vector3(-halfExtent, kGridY, -halfExtent) });
    verts.push_back({ Leir::Vector3(halfExtent, kGridY, -halfExtent) });
    verts.push_back({ Leir::Vector3(halfExtent, kGridY, halfExtent) });
    verts.push_back({ Leir::Vector3(-halfExtent, kGridY, halfExtent) });
    idxs = { 0, 1, 2, 0, 2, 3 };

    UploadBuffers(verts, idxs, out);
}

void EditorGrid::UploadBuffers(const std::vector<GridVertex>& verts,
                               const std::vector<uint32_t>& idxs, LevelMesh& out)
{
    if (verts.empty() || idxs.empty())
        return;

    const uint32_t vbSize = (uint32_t)(verts.size() * sizeof(GridVertex));
    const uint32_t ibSize = (uint32_t)(idxs.size() * sizeof(uint32_t));

    Leir::RHI::RHIDeviceMemory vertStagingMem, idxStagingMem;
    Leir::RHI::RHIBuffer vertStagingBuf = m_Device->CreateBuffer(vbSize,
        Leir::RHI::BufferUsage::TransferSrc,
        Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
        vertStagingMem);
    Leir::RHI::RHIBuffer idxStagingBuf = m_Device->CreateBuffer(ibSize,
        Leir::RHI::BufferUsage::TransferSrc,
        Leir::RHI::MemoryProperty::HostVisible | Leir::RHI::MemoryProperty::HostCoherent,
        idxStagingMem);

    void* data = nullptr;
    m_Device->MapMemory(vertStagingMem, 0, vbSize, &data);
    std::memcpy(data, verts.data(), vbSize);
    m_Device->UnmapMemory(vertStagingMem);

    m_Device->MapMemory(idxStagingMem, 0, ibSize, &data);
    std::memcpy(data, idxs.data(), ibSize);
    m_Device->UnmapMemory(idxStagingMem);

    out.vertexBuffer = m_Device->CreateBuffer(vbSize,
        Leir::RHI::BufferUsage::Vertex | Leir::RHI::BufferUsage::TransferDst,
        Leir::RHI::MemoryProperty::DeviceLocal, out.vertexMemory);
    out.indexBuffer = m_Device->CreateBuffer(ibSize,
        Leir::RHI::BufferUsage::Index | Leir::RHI::BufferUsage::TransferDst,
        Leir::RHI::MemoryProperty::DeviceLocal, out.indexMemory);
    out.indexCount = (int)idxs.size();

    m_Device->CopyBuffer(vertStagingBuf, out.vertexBuffer, vbSize);
    m_Device->CopyBuffer(idxStagingBuf, out.indexBuffer, ibSize);

    m_Device->DestroyBuffer(vertStagingBuf);
    m_Device->DestroyMemory(vertStagingMem);
    m_Device->DestroyBuffer(idxStagingBuf);
    m_Device->DestroyMemory(idxStagingMem);
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
        pushRange.stage = Leir::RHI::ShaderStageMask::VertexFragment;
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
    desc.topology = Leir::RHI::Topology::TriangleList;
    desc.polygonMode = Leir::RHI::PolygonMode::Fill;
    desc.cullMode = Leir::RHI::CullMode::None;
    desc.depthTestEnable = true;
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
    binding.stride = sizeof(GridVertex); // pos(12)
    binding.inputRate = Leir::RHI::VertexInputRate::Vertex;
    return binding;
}

std::vector<Leir::RHI::RHIVertexAttribute> EditorGrid::GetAttributeDescriptions()
{
    std::vector<Leir::RHI::RHIVertexAttribute> attrs(1);

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = Leir::RHI::Format::R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(GridVertex, pos);

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
    }

    auto destroyLevel = [&](LevelMesh& mesh) {
        if (mesh.vertexBuffer.IsValid())
            m_Device->DestroyBuffer(mesh.vertexBuffer);
        if (mesh.vertexMemory.IsValid())
            m_Device->DestroyMemory(mesh.vertexMemory);
        if (mesh.indexBuffer.IsValid())
            m_Device->DestroyBuffer(mesh.indexBuffer);
        if (mesh.indexMemory.IsValid())
            m_Device->DestroyMemory(mesh.indexMemory);
    };
    destroyLevel(m_L1);
    destroyLevel(m_L10);
    destroyLevel(m_L100);
    destroyLevel(m_Axis);
}
