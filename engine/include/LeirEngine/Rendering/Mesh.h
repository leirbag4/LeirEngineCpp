#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/RHI/RHI.h"

#include <vector>
#include <cstdint>
#include <utility>

namespace Leir {

namespace RHI { class RenderBackend; }

struct LEIR_API Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;

    static RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<RHI::RHIVertexAttribute> GetAttributeDescriptions();
};

class LEIR_API Mesh {
public:
    Mesh(RHI::RenderBackend* device,
         const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    void Bind(RHI::RHICommandBuffer cmd) const;
    void Draw(RHI::RHICommandBuffer cmd) const;

    size_t GetVertexCount() const { return m_Vertices.size(); }
    size_t GetIndexCount() const { return m_Indices.size(); }

private:
    RHI::RenderBackend* m_Device;

    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;

    RHI::RHIBuffer m_VertexBuffer;
    RHI::RHIDeviceMemory m_VertexMemory;
    RHI::RHIBuffer m_IndexBuffer;
    RHI::RHIDeviceMemory m_IndexMemory;
};

// Built-in primitives
namespace Primitives {
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateCube();
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreateSphere(int sectors, int stacks);
    LEIR_API std::pair<std::vector<Vertex>, std::vector<uint32_t>> CreatePlane(float size = 1.0f);
}

} // namespace Leir
