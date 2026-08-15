#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/RHI/RenderBackend.h"

#include <cstring>
#include <cmath>
#include <cstddef>

#include "LeirEngine/Math/Mathf.h"

namespace Leir {

RHI::RHIVertexInputBinding Vertex::GetBindingDescription()
{
    RHI::RHIVertexInputBinding desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = RHI::VertexInputRate::Vertex;
    return desc;
}

std::vector<RHI::RHIVertexAttribute> Vertex::GetAttributeDescriptions()
{
    std::vector<RHI::RHIVertexAttribute> attrs(3);
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = RHI::Format::R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, position);

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = RHI::Format::R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, normal);

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = RHI::Format::R32G32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, texCoord);

    return attrs;
}

Mesh::Mesh(RHI::RenderBackend* device,
           const std::vector<Vertex>& vertices,
           const std::vector<uint32_t>& indices)
    : m_Device(device)
    , m_Vertices(vertices)
    , m_Indices(indices)
{
    uint32_t vertexSize = (uint32_t)(vertices.size() * sizeof(Vertex));
    uint32_t indexSize = (uint32_t)(indices.size() * sizeof(uint32_t));

    RHI::RHIBuffer stagingBuffer;
    RHI::RHIDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(vertexSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        stagingMemory);

    void* data;
    m_Device->MapMemory(stagingMemory, 0, vertexSize, &data);
    memcpy(data, vertices.data(), (size_t)vertexSize);
    m_Device->UnmapMemory(stagingMemory);

    m_VertexBuffer = m_Device->CreateBuffer(vertexSize,
        RHI::BufferUsage::TransferDst | RHI::BufferUsage::Vertex,
        RHI::MemoryProperty::DeviceLocal,
        m_VertexMemory);
    m_Device->CopyBuffer(stagingBuffer, m_VertexBuffer, vertexSize);

    m_Device->DestroyBuffer(stagingBuffer);
    m_Device->DestroyMemory(stagingMemory);

    stagingBuffer = m_Device->CreateBuffer(indexSize,
        RHI::BufferUsage::TransferSrc,
        RHI::MemoryProperty::HostVisible | RHI::MemoryProperty::HostCoherent,
        stagingMemory);

    m_Device->MapMemory(stagingMemory, 0, indexSize, &data);
    memcpy(data, indices.data(), (size_t)indexSize);
    m_Device->UnmapMemory(stagingMemory);

    m_IndexBuffer = m_Device->CreateBuffer(indexSize,
        RHI::BufferUsage::TransferDst | RHI::BufferUsage::Index,
        RHI::MemoryProperty::DeviceLocal,
        m_IndexMemory);
    m_Device->CopyBuffer(stagingBuffer, m_IndexBuffer, indexSize);

    m_Device->DestroyBuffer(stagingBuffer);
    m_Device->DestroyMemory(stagingMemory);
}

Mesh::~Mesh()
{
    m_Device->DestroyBuffer(m_VertexBuffer);
    m_Device->DestroyMemory(m_VertexMemory);
    m_Device->DestroyBuffer(m_IndexBuffer);
    m_Device->DestroyMemory(m_IndexMemory);
}

void Mesh::Bind(RHI::GCommandGraph& graph) const
{
    graph.BindVertexBuffer(m_VertexBuffer);
    graph.BindIndexBuffer(m_IndexBuffer);
}

void Mesh::Draw(RHI::GCommandGraph& graph) const
{
    graph.DrawIndexed((uint32_t)m_Indices.size(), 1, 0);
}

namespace Primitives {

std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateCube()
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float s = 0.5f;

    vertices.push_back({{-s, -s,  s}, { 0, 0, 1}, {0, 1}});
    vertices.push_back({{ s, -s,  s}, { 0, 0, 1}, {1, 1}});
    vertices.push_back({{ s,  s,  s}, { 0, 0, 1}, {1, 0}});
    vertices.push_back({{-s,  s,  s}, { 0, 0, 1}, {0, 0}});

    vertices.push_back({{ s, -s, -s}, { 0, 0,-1}, {0, 1}});
    vertices.push_back({{-s, -s, -s}, { 0, 0,-1}, {1, 1}});
    vertices.push_back({{-s,  s, -s}, { 0, 0,-1}, {1, 0}});
    vertices.push_back({{ s,  s, -s}, { 0, 0,-1}, {0, 0}});

    vertices.push_back({{ s, -s,  s}, { 1, 0, 0}, {0, 1}});
    vertices.push_back({{ s, -s, -s}, { 1, 0, 0}, {1, 1}});
    vertices.push_back({{ s,  s, -s}, { 1, 0, 0}, {1, 0}});
    vertices.push_back({{ s,  s,  s}, { 1, 0, 0}, {0, 0}});

    vertices.push_back({{-s, -s, -s}, {-1, 0, 0}, {0, 1}});
    vertices.push_back({{-s, -s,  s}, {-1, 0, 0}, {1, 1}});
    vertices.push_back({{-s,  s,  s}, {-1, 0, 0}, {1, 0}});
    vertices.push_back({{-s,  s, -s}, {-1, 0, 0}, {0, 0}});

    vertices.push_back({{-s,  s,  s}, { 0, 1, 0}, {0, 1}});
    vertices.push_back({{ s,  s,  s}, { 0, 1, 0}, {1, 1}});
    vertices.push_back({{ s,  s, -s}, { 0, 1, 0}, {1, 0}});
    vertices.push_back({{-s,  s, -s}, { 0, 1, 0}, {0, 0}});

    vertices.push_back({{-s, -s, -s}, { 0,-1, 0}, {0, 1}});
    vertices.push_back({{ s, -s, -s}, { 0,-1, 0}, {1, 1}});
    vertices.push_back({{ s, -s,  s}, { 0,-1, 0}, {1, 0}});
    vertices.push_back({{-s, -s,  s}, { 0,-1, 0}, {0, 0}});

    uint32_t i = 0;
    for (int face = 0; face < 6; ++face) {
        indices.push_back(i+0); indices.push_back(i+1); indices.push_back(i+2);
        indices.push_back(i+0); indices.push_back(i+2); indices.push_back(i+3);
        i += 4;
    }

    return {vertices, indices};
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateSphere(int sectors, int stacks)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int j = 0; j <= stacks; ++j) {
        float theta = (float)j / (float)stacks * Mathf::PI;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int i = 0; i <= sectors; ++i) {
            float phi = (float)i / (float)sectors * 2.0f * Mathf::PI;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            Vector3 normal = { sinTheta * cosPhi, cosTheta, sinTheta * sinPhi };
            Vector3 position = normal * 0.5f;
            Vector2 texCoord = { (float)i / (float)sectors, (float)j / (float)stacks };

            vertices.push_back({position, normal, texCoord});
        }
    }

    for (int j = 0; j < stacks; ++j) {
        for (int i = 0; i < sectors; ++i) {
            int first = j * (sectors + 1) + i;
            int second = first + sectors + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
        }
    }

    return {vertices, indices};
}

std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreatePlane(float size)
{
    float s = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-s, 0,  s}, {0, 1, 0}, {0, 1}},
        {{ s, 0,  s}, {0, 1, 0}, {1, 1}},
        {{ s, 0, -s}, {0, 1, 0}, {1, 0}},
        {{-s, 0, -s}, {0, 1, 0}, {0, 0}},
    };
    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};
    return {vertices, indices};
}

} // namespace Primitives
} // namespace Leir
