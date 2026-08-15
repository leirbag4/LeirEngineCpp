#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/RHI/RHI.h"

#include <cstdint>
#include <utility>
#include <vector>

// GCommandGraph (TODO_RHI_SLANG.md §3.2) — a backend-neutral, per-frame command
// graph.
//
// Recording code (RenderPipeline, Mesh, Material, UIRenderer, RenderTexture)
// appends draw/pass records to a graph instead of issuing backend Cmd* calls
// directly. The backend then translates the graph to native commands in
// RenderBackend::CmdExecuteGraph, inserting image-layout transitions
// automatically via last-use tracking (the per-frame manual
// CmdTransitionImageLayout / CmdBarrier calls are gone).
//
// Two execution modes:
//   - Graphs that begin with BeginRenderPass() own a render pass (e.g. the
//     editor's offscreen RenderTexture pass).
//   - Pass-less graphs (no pass records) record draws into whatever pass is
//     already active in the native command buffer (the swapchain 3D pass
//     started by BeginFrame(false), or the overlay pass started by
//     BeginSwapchainOverlay()).
//
// The graph is header-only: it owns no backend resources, only records. Reuse
// one graph per frame by calling Clear().

namespace Leir {
namespace RHI {

// Attachment of a render-pass record: the image to layout-track and whether it
// is the depth attachment (the rest are color attachments, in order).
struct GAttachment {
    RHIImage image;
    bool isDepth = false;
};

// A descriptor-set bind recorded for a draw (preserves the first-set index so
// the backend binds e.g. UBO at set 0 and the bindless table at set 1).
struct GSetBinding {
    RHIPipelineLayout layout;
    uint32_t firstSet = 0;
    std::vector<RHIDescriptorSet> sets;
};

// A render-pass begin record: the pass template (clears + viewport + scissor),
// the framebuffer, and the attachment images for last-use transition tracking.
struct GRenderPassRecord {
    RHIPassTemplate passTemplate;
    RHIFramebuffer framebuffer;
    std::vector<GAttachment> attachments;
};

// A draw record: the full per-draw state snapshot taken at Draw/DrawIndexed.
// Repeated state (pipeline, buffers, push constants, bindless set) that is only
// bound once and reused by several draws appears in the first record only — the
// executor replays records in order, so native state persists across records,
// mirroring immediate-mode recording.
struct GDrawRecord {
    RHIPipeline pipeline;
    RHIPipelineLayout layout;
    std::vector<GSetBinding> setBindings;
    RHIBuffer vertexBuffer;
    RHIBuffer indexBuffer;

    bool indexed = false;
    uint32_t vertexCount = 0;
    uint32_t firstVertex = 0;
    uint32_t indexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex = 0;

    ShaderStageMask pushStage = ShaderStageMask::Vertex;
    uint32_t pushOffset = 0;
    std::vector<uint8_t> pushData;

    RHIViewport viewport{};
    bool hasViewport = false;
    RHIRect2D scissor{};
    bool hasScissor = false;

    // Bindless texture indices sampled by this draw. The executor uses them to
    // ensure each referenced image is in SHADER_READ_ONLY before the draw
    // (last-use tracking: e.g. the RenderTexture color image alternates between
    // COLOR_ATTACHMENT and SHADER_READ_ONLY every frame).
    std::vector<uint32_t> sampledTextures;
};

enum class GRecordType : uint8_t {
    BeginRenderPass = 0,
    EndRenderPass = 1,
    Draw = 2,
};

struct GRecord {
    GRecordType type = GRecordType::Draw;
    GRenderPassRecord pass;
    GDrawRecord draw;
};

class GCommandGraph {
public:
    void Clear() {
        m_Records.clear();
        m_Current = {};
        m_LastPass = {};
        m_HasRenderPass = false;
    }

    bool HasRenderPass() const { return m_HasRenderPass; }
    const std::vector<GRecord>& GetRecords() const { return m_Records; }

    void BeginRenderPass(RHIPassTemplate passTemplate, RHIFramebuffer framebuffer,
                         std::vector<GAttachment> attachments) {
        GRecord rec;
        rec.type = GRecordType::BeginRenderPass;
        rec.pass.passTemplate = passTemplate;
        rec.pass.framebuffer = framebuffer;
        rec.pass.attachments = std::move(attachments);
        m_LastPass = rec.pass;
        m_Records.push_back(std::move(rec));
        m_HasRenderPass = true;
    }

    // The EndRenderPass record carries the same attachments so the executor
    // knows which images to mark ShaderReadOnly after the pass (the render pass
    // transitions them via its final layout on Vulkan; D3D12 needs an explicit
    // barrier).
    void EndRenderPass() {
        GRecord rec;
        rec.type = GRecordType::EndRenderPass;
        rec.pass = m_LastPass;
        m_Records.push_back(std::move(rec));
        m_HasRenderPass = false;
    }

    void BindPipeline(RHIPipeline pipeline) { m_Current.pipeline = pipeline; }

    void BindDescriptorSets(RHIPipelineLayout layout, uint32_t firstSet,
                            const std::vector<RHIDescriptorSet>& sets) {
        GSetBinding b;
        b.layout = layout;
        b.firstSet = firstSet;
        b.sets = sets;
        m_Current.setBindings.push_back(std::move(b));
        m_Current.layout = layout;
    }

    void BindVertexBuffer(RHIBuffer buffer) { m_Current.vertexBuffer = buffer; }
    void BindIndexBuffer(RHIBuffer buffer) { m_Current.indexBuffer = buffer; }

    void PushConstants(RHIPipelineLayout layout, ShaderStageMask stage,
                       uint32_t offset, uint32_t size, const void* data) {
        m_Current.layout = layout;
        m_Current.pushStage = stage;
        m_Current.pushOffset = offset;
        const auto* bytes = static_cast<const uint8_t*>(data);
        m_Current.pushData.assign(bytes, bytes + size);
    }

    void SetViewport(const RHIViewport& viewport) {
        m_Current.viewport = viewport;
        m_Current.hasViewport = true;
    }

    void SetScissor(const RHIRect2D& scissor) {
        m_Current.scissor = scissor;
        m_Current.hasScissor = true;
    }

    // Bindless indices sampled by the next draw record (consumed by
    // Draw/DrawIndexed).
    void SetSampledTextures(std::vector<uint32_t> indices) {
        m_Current.sampledTextures = std::move(indices);
    }

    void Draw(uint32_t vertexCount, uint32_t firstVertex) {
        GRecord rec;
        rec.type = GRecordType::Draw;
        rec.draw = m_Current;
        rec.draw.indexed = false;
        rec.draw.vertexCount = vertexCount;
        rec.draw.firstVertex = firstVertex;
        m_Records.push_back(std::move(rec));
        m_Current = {};
    }

    void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) {
        GRecord rec;
        rec.type = GRecordType::Draw;
        rec.draw = m_Current;
        rec.draw.indexed = true;
        rec.draw.indexCount = indexCount;
        rec.draw.instanceCount = instanceCount;
        rec.draw.firstIndex = firstIndex;
        m_Records.push_back(std::move(rec));
        m_Current = {};
    }

private:
    std::vector<GRecord> m_Records;
    GDrawRecord m_Current;
    GRenderPassRecord m_LastPass;
    bool m_HasRenderPass = false;
};

} // namespace RHI
} // namespace Leir