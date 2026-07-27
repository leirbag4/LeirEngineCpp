#include "LeirEngine/Rendering/Mesh.h"
#include "LeirEngine/Rendering/VulkanDevice.h"

#include <cstring>
#include <cmath>
#include <cstddef>

#include <glm/gtc/constants.hpp>

namespace Leir {

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription desc{};
    desc.binding = 0;
    desc.stride = sizeof(Vertex);
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return desc;
}

std::vector<VkVertexInputAttributeDescription> Vertex::GetAttributeDescriptions()
{
    std::vector<VkVertexInputAttributeDescription> attrs(3);
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(Vertex, position);

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = offsetof(Vertex, normal);

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = offsetof(Vertex, texCoord);

    return attrs;
}

Mesh::Mesh(VulkanDevice* device,
           const std::vector<Vertex>& vertices,
           const std::vector<uint32_t>& indices)
    : m_Device(device)
    , m_Vertices(vertices)
    , m_Indices(indices)
{
    VkDeviceSize vertexSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize indexSize = indices.size() * sizeof(uint32_t);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    stagingBuffer = m_Device->CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingMemory);

    void* data;
    vkMapMemory(m_Device->GetDevice(), stagingMemory, 0, vertexSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)vertexSize);
    vkUnmapMemory(m_Device->GetDevice(), stagingMemory);

    m_VertexBuffer = m_Device->CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_VertexMemory);
    m_Device->CopyBuffer(stagingBuffer, m_VertexBuffer, vertexSize);

    vkDestroyBuffer(m_Device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), stagingMemory, nullptr);

    stagingBuffer = m_Device->CreateBuffer(indexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingMemory);

    vkMapMemory(m_Device->GetDevice(), stagingMemory, 0, indexSize, 0, &data);
    memcpy(data, indices.data(), (size_t)indexSize);
    vkUnmapMemory(m_Device->GetDevice(), stagingMemory);

    m_IndexBuffer = m_Device->CreateBuffer(indexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_IndexMemory);
    m_Device->CopyBuffer(stagingBuffer, m_IndexBuffer, indexSize);

    vkDestroyBuffer(m_Device->GetDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), stagingMemory, nullptr);
}

Mesh::~Mesh()
{
    vkDestroyBuffer(m_Device->GetDevice(), m_VertexBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), m_VertexMemory, nullptr);
    vkDestroyBuffer(m_Device->GetDevice(), m_IndexBuffer, nullptr);
    vkFreeMemory(m_Device->GetDevice(), m_IndexMemory, nullptr);
}

void Mesh::Bind(VkCommandBuffer cmd) const
{
    VkBuffer vertexBuffers[] = { m_VertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::Draw(VkCommandBuffer cmd) const
{
    vkCmdDrawIndexed(cmd, (uint32_t)m_Indices.size(), 1, 0, 0, 0);
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
        float theta = (float)j / (float)stacks * glm::pi<float>();
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int i = 0; i <= sectors; ++i) {
            float phi = (float)i / (float)sectors * 2.0f * glm::pi<float>();
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            glm::vec3 normal = { sinTheta * cosPhi, cosTheta, sinTheta * sinPhi };
            glm::vec3 position = normal * 0.5f;
            glm::vec2 texCoord = { (float)i / (float)sectors, (float)j / (float)stacks };

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
