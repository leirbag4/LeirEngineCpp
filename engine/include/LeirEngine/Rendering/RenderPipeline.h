#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/Rendering/ShaderLayout.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }
class Scene;
class MeshRenderer;
class Camera;
class Light;
class Material;
class SpriteRenderer;
class SpriteSheet;
class Texture2D;

struct LEIR_API PushConstants {
    Vector3 lightDir = {0.0f, -1.0f, 0.0f};
    float pad0 = 0.0f;
    Vector3 lightColor = {1.0f, 1.0f, 1.0f};
    float pad1 = 0.0f;
    Vector3 ambientColor = {0.2f, 0.2f, 0.3f};
    float pad2 = 0.0f;
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    Matrix4x4 model;
    // Bindless texture index of the material's texture. Must match the shader's
    // PushConstants layout: offset 128, after model (shader std430 size 144).
    uint32_t textureIndex = 0;
};

struct LEIR_API PerMeshUBO {
    Matrix4x4 viewProjection;
};

struct UBOBuffer {
    RHI::RHIBuffer buffer;
    RHI::RHIDeviceMemory memory;
};

struct LEIR_API SpriteVertex {
    Vector2 position;
    Vector2 texCoord;

    static RHI::RHIVertexInputBinding GetBindingDescription();
    static std::vector<RHI::RHIVertexAttribute> GetAttributeDescriptions();
};

struct LEIR_API SpritePushConstants {
    Matrix4x4 mvp;
    Vector4 color;
    Vector4 uvRect; // {u, v, w, h} in UV space
    // Bindless texture index. Matches the shader's SpritePushConstants layout:
    // offset 96, after uvRect (shader std430 size 112).
    uint32_t textureIndex = 0;
};

class LEIR_API RenderPipeline {
public:
    RenderPipeline(RHI::RenderBackend* device);
    ~RenderPipeline();

    void Render(RHI::GCommandGraph& graph, Scene* scene);
    void RenderOverlay(RHI::GCommandGraph& graph, Scene* scene);

    // Hot-reload: re-read Sprite.vert/Sprite.frag and recreate the sprite
    // pipeline (buffers, descriptor sets and layout stay valid).
    void ReloadSpritePipeline();

private:
    void RenderMeshRenderer(RHI::GCommandGraph& graph, MeshRenderer* renderer,
        const Matrix4x4& viewProj, const Matrix4x4& model,
        const PushConstants& push);

    void CreateSpriteResources();
    void CreateSpritePipeline();
    void DestroySpriteResources();
    void RenderSprite(RHI::GCommandGraph& graph, SpriteRenderer* renderer,
        const Matrix4x4& mvp);

    RHI::RenderBackend* m_Device;
    std::vector<UBOBuffer> m_UBOBuffers;
    std::vector<RHI::RHIDescriptorSet> m_UBOSets;
    RHI::RHIDescriptorPool m_UBODescriptorPool;

    // 2D sprite pipeline
    struct {
        RHI::RHIPipelineLayout pipelineLayout;
        RHI::RHIPipeline pipeline;
        RHI::RHIDescriptorSetLayout descSetLayout;
        std::vector<RHISetLayoutEntry> setLayouts; // derived from reflection (owned)
        RHI::RHIBuffer vertexBuffer;
        RHI::RHIDeviceMemory vertexMemory;
        RHI::RHIBuffer indexBuffer;
        RHI::RHIDeviceMemory indexMemory;
        int indexCount = 0;
        Texture2D* fallbackTexture = nullptr;
    } m_Sprite;
};

} // namespace Leir
