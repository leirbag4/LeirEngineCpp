#pragma once

#include "LeirEngine/Core/Export.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Leir {

class VulkanDevice;

struct LEIR_API Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

class LEIR_API Mesh {
public:
    Mesh(VulkanDevice* device,
         const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    void Bind(VkCommandBuffer cmd) const;
    void Draw(VkCommandBuffer cmd) const;

    size_t GetVertexCount() const { return m_Vertices.size(); }
    size_t GetIndexCount() const { return m_Indices.size(); }

private:
    VulkanDevice* m_Device;

    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexMemory = VK_NULL_HANDLE;
    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexMemory = VK_NULL_HANDLE;
};

// Built-in primitives
namespace Primitives {
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateCube();
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateSphere(int sectors, int stacks);
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreatePlane(float size = 1.0f);
}

} // namespace Leir
