#pragma once

/**
 * @file UIRenderer.h
 * @brief UI renderer: builds batches and renders the canvas with GCommandGraph.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include "LeirEngine/Rendering/ShaderLayout.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/RHI/RHI.h"
#include <unordered_map>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }
class UICanvas;
class UIElement;
class Texture2D;
class RenderTexture;

/**
 * @brief UI vertex: position, UV, color and bindless texture index.
 * @ingroup UI
 */
struct LEIR_API UIVertex {
    Vector2 position;   ///< Position (logical pixels, NDC via push constants).
    Vector2 texCoord;   ///< UV coordinates.
    Vector4 color;      ///< Vertex color (tint).
    float textureIndex; ///< Bindless texture index (constant per quad).
};

/**
 * @brief Viewport draw: quad sampling a RenderTexture.
 * @ingroup UI
 */
struct LEIR_API ViewportDraw {
    UIVertex verts[4];      ///< Quad vertices.
    RenderTexture* texture; ///< RenderTexture sampled.
    Vector4 clip;           ///< Logical clip rect.
};

/**
 * @brief Per-frame render statistics.
 * @ingroup UI
 */
struct LEIR_API UIRenderStats {
    uint32_t quads = 0;     ///< Quads submitted last frame.
    uint32_t vertices = 0;  ///< Vertices submitted last frame.
    uint32_t drawCalls = 0; ///< Actual draw calls (after batching).
    uint32_t batches = 0;   ///< Number of merged batches.
};

/**
 * @brief UI renderer: traverses the canvas, builds batches and issues draws.
 * @ingroup UI
 * @details Handles three draw layers (regular UI, viewports, debug overlay) with
 *  scissor clipping and bindless textures.
 */
class LEIR_API UIRenderer {
public:
    /**
     * @brief Constructs a renderer for a backend.
     * @param[in] device Render backend.
     */
    UIRenderer(RHI::RenderBackend* device);

    /**
     * @brief Destroys the renderer and its pipeline.
     */
    ~UIRenderer();

    /**
     * @brief Renders the canvas into the command graph.
     * @param[in] graph Command graph (draws appended).
     * @param[in] canvas Canvas to render.
     */
    void Render(RHI::GCommandGraph& graph, UICanvas* canvas);

    /**
     * @brief Hot-reloads UI shaders and recreates the pipeline.
     */
    void ReloadShaders();

    /**
     * @brief Sets content scale (physical/logical) for scissor conversion.
     * @param[in] scale Scale factor.
     */
    void SetContentScale(float scale) { m_ContentScale = scale; }

    /**
     * @brief Returns content scale.
     * @return Scale factor.
     */
    float GetContentScale() const { return m_ContentScale; }

    /**
     * @brief Returns stats from the last Flush.
     * @return Last stats (0 if never rendered).
     */
    const UIRenderStats& GetLastStats() const { return m_LastStats; }

private:
    void CreatePipeline();
    void RenderElement(UIElement* elem, const Vector4* clip, bool isDebug);
    void BuildBatch(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    void BuildBatchDebug(Texture2D* texture, const Vector4& rect, const Vector4& uv, const Vector4& color);
    void Flush(RHI::GCommandGraph& graph);
    void ApplyScissor(RHI::GCommandGraph& graph, const Vector4& logicalClip, RHI::RHIRect2D& last, bool& valid);

    RHI::RenderBackend* m_Device;                          ///< Backend.

    RHI::RHIPipelineLayout m_PipelineLayout;                ///< Pipeline layout.
    RHI::RHIPipeline m_Pipeline;                            ///< Graphics pipeline.
    RHI::RHIDescriptorSetLayout m_DescSetLayout;            ///< Descriptor set layout.
    std::vector<RHISetLayoutEntry> m_SetLayouts;            ///< Reflection-derived layouts.
    RHI::RHIBuffer m_VertexBuffers[3];                      ///< Vertex buffers (triple buffered).
    RHI::RHIDeviceMemory m_VertexMemories[3];               ///< Vertex memories.
    int m_MaxVertices = 0;                                  ///< Max vertices.

    std::vector<UIVertex> m_Vertices;                       ///< Regular batch vertices.
    std::vector<Texture2D*> m_QuadTextures;                 ///< Regular quad textures.
    std::vector<Vector4> m_QuadClips;                       ///< Regular quad clips.

    Texture2D* m_FallbackTex = nullptr;                     ///< Fallback white texture.

    std::vector<ViewportDraw> m_ViewportDraws;              ///< Viewport draws.

    std::vector<UIVertex> m_DebugVertices;                  ///< Debug overlay vertices.
    std::vector<Texture2D*> m_DebugQuadTextures;            ///< Debug quad textures.
    std::vector<Vector4> m_DebugQuadClips;                  ///< Debug quad clips.

    const Vector4* m_CurrentClip = nullptr;                 ///< Active clip rect.
    Vector2 m_ScreenSize = {1280.0f, 720.0f};                ///< Logical canvas size.
    float m_ContentScale = 1.0f;                            ///< Content scale.
    UIRenderStats m_LastStats;                              ///< Last frame stats.
};

} // namespace Leir
