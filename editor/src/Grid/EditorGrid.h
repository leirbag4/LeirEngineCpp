#pragma once

#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"

#include <cstdint>
#include <vector>

namespace Leir {
struct RHISetLayoutEntry;
namespace RHI {
class RenderBackend;
}
} // namespace Leir

// Unity-style ground grid rendered in the editor viewport (Y=0).
//
// The grid is drawn as ONE flat quad (a single mesh covers every LOD level) and
// the LINES are generated procedurally in the fragment shader (Grid.frag.slang):
// distance to the nearest grid line, converted to screen pixels with fwidth(),
// so every line has a constant 1px anti-aliased width at any distance/angle.
// L1/L10/L100 are evaluated and blended in the same fragment pass, so shared
// chunk-line positions never z-fight (a single quad has no coplanar overlaps).
// Chunk lines (every 10th of a level) are brighter; the origin axes (red X /
// blue Z, 2px, never fading) are also computed here. See Grid.frag.slang.
class EditorGrid {
public:
    EditorGrid(Leir::RHI::RenderBackend* device,
               Leir::RHI::RHIRenderPass viewportRenderPass);
    ~EditorGrid();

    EditorGrid(const EditorGrid&) = delete;
    EditorGrid& operator=(const EditorGrid&) = delete;

    void Render(Leir::RHI::GCommandGraph& graph,
                const Leir::Matrix4x4& viewProjection,
                const Leir::Vector3& cameraPos);

private:
    struct GridVertex {
        Leir::Vector3 pos;
    };

    // Layout must match Grid.vert.slang's cbuffer UniformBufferObject (the
    // fragment stage never reads it — D3D12 binds it VERTEX-only).
    struct GridUBO {
        Leir::Matrix4x4 viewProjection;
    };

    // Layout must match Grid.vert.slang's GridPushConstants (64 bytes).
    struct GridPushConstants {
        float lineWidth = 1.5f;      // 0
        float chunkWidth = 2.0f;     // 4
        float pad0 = 0.0f;           // 8
        float pad1 = 0.0f;           // 12
        Leir::Vector3 cameraPos;     // 16
        float pad2 = 0.0f;           // 28
        Leir::Vector4 baseColor{1.0f, 1.0f, 1.0f, 1.0f}; // 32
        Leir::Vector4 chunkColor{1.0f, 1.0f, 1.0f, 1.0f}; // 48
    };

    struct LevelMesh {
        Leir::RHI::RHIBuffer vertexBuffer;
        Leir::RHI::RHIDeviceMemory vertexMemory;
        Leir::RHI::RHIBuffer indexBuffer;
        Leir::RHI::RHIDeviceMemory indexMemory;
        int indexCount = 0;
    };

    static const int kFrames = 2;

    void CreatePipeline(Leir::RHI::RHIRenderPass viewportRenderPass);
    void BuildLevelMeshes();
    void BuildLevel(float halfExtent, LevelMesh& out);
    void UploadBuffers(const std::vector<GridVertex>& verts,
                       const std::vector<uint32_t>& idxs, LevelMesh& out);
    void DestroyResources();

    static Leir::RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<Leir::RHI::RHIVertexAttribute> GetAttributeDescriptions();

    Leir::RHI::RenderBackend* m_Device = nullptr;

    LevelMesh m_Grid;

    Leir::RHI::RHIPipeline m_Pipeline;
    Leir::RHI::RHIPipelineLayout m_PipelineLayout;
    std::vector<Leir::RHISetLayoutEntry> m_SetLayouts;
    Leir::RHI::RHIDescriptorSetLayout m_UBOLayout;
    Leir::RHI::RHIDescriptorPool m_UBOPool;
    Leir::RHI::RHIDescriptorSet m_UBOSets[kFrames];
    Leir::RHI::RHIBuffer m_UBOBuffers[kFrames];
    Leir::RHI::RHIDeviceMemory m_UBOMemories[kFrames];
};
