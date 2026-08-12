#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Vector4.h"

#include <cstdint>
#include <cstddef>
#include <vector>

// LeirEngine RHI — minimal render abstraction layer.
//
// This is the *evolutive minimum* RHI (see TODO_RHI_SLANG.md Fase 2a): a thin,
// stable front-end over interchangeable backends (Vulkan today, D3D12 next,
// Metal/WebGPU later). The full RHI (GCommandGraph, bindless, reflection,
// GCaps) described in TODO §3 is a later migration.
//
// Public headers must never expose backend types (Vk*, ID3D12*, ...). This
// header defines only backend-neutral types: enums, opaque handles and
// description structs.

namespace Leir {
namespace RHI {

// Compile-time backend selector (TODO_RHI_SLANG.md §5). Defaults to Vulkan.
// Define LEIR_BACKEND before including to pick another backend once available.
#define LEIR_BACKEND_VULKAN 1
#ifndef LEIR_BACKEND
#define LEIR_BACKEND LEIR_BACKEND_VULKAN
#endif

// ---- Enum values (backend-neutral) ----

enum class ShaderStage : uint8_t {
    Vertex = 0,
    Fragment = 1,
};

enum class ShaderStageMask : uint8_t {
    None = 0,
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    VertexFragment = Vertex | Fragment,
};

inline ShaderStageMask operator|(ShaderStageMask a, ShaderStageMask b) {
    return static_cast<ShaderStageMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

enum class Format : uint8_t {
    R32G32B32_SFLOAT = 0,
    R32G32_SFLOAT = 1,
    R32G32B32A32_SFLOAT = 2,
    R8G8B8A8_SRGB = 3,
    B8G8R8A8_SRGB = 4,
    D32_SFLOAT = 5,
};

enum class Topology : uint8_t {
    TriangleList = 0,
    TriangleStrip = 1,
};

enum class PolygonMode : uint8_t {
    Fill = 0,
};

enum class CullMode : uint8_t {
    None = 0,
    Back = 1,
    Front = 2,
};

enum class VertexInputRate : uint8_t {
    Vertex = 0,
    Instance = 1,
};

enum class DescriptorType : uint8_t {
    CombinedImageSampler = 0,
    UniformBuffer = 1,
};

enum class Filter : uint8_t {
    Nearest = 0,
    Linear = 1,
};

enum class SamplerAddressMode : uint8_t {
    Repeat = 0,
    ClampToEdge = 1,
};

enum class BufferUsage : uint8_t {
    None = 0,
    TransferSrc = 1 << 0,
    TransferDst = 1 << 1,
    Vertex = 1 << 2,
    Index = 1 << 3,
    Uniform = 1 << 4,
};

enum class MemoryProperty : uint8_t {
    None = 0,
    DeviceLocal = 1 << 0,
    HostVisible = 1 << 1,
    HostCoherent = 1 << 2,
};

enum class ImageUsage : uint8_t {
    None = 0,
    TransferDst = 1 << 0,
    TransferSrc = 1 << 1,
    Sampled = 1 << 2,
    ColorAttachment = 1 << 3,
    DepthStencilAttachment = 1 << 4,
};

enum class Aspect : uint8_t {
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
};

enum class ImageLayout : uint8_t {
    Undefined = 0,
    TransferDst = 1,
    ShaderReadOnly = 2,
    ColorAttachment = 3,
    DepthStencilAttachment = 4,
};

enum class LoadOp : uint8_t {
    Load = 0,
    Clear = 1,
    DontCare = 2,
};

enum class StoreOp : uint8_t {
    Store = 0,
    DontCare = 1,
};

// Bitwise OR helpers for the flag enums.
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline MemoryProperty operator|(MemoryProperty a, MemoryProperty b) {
    return static_cast<MemoryProperty>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline ImageUsage operator|(ImageUsage a, ImageUsage b) {
    return static_cast<ImageUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline Aspect operator|(Aspect a, Aspect b) {
    return static_cast<Aspect>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

// ---- Opaque handles ----
// A handle is a 64-bit value the backend fills in. Values <= 0 mean invalid.

using Handle = uint64_t;

struct RHIBuffer          { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIDeviceMemory    { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIImage           { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIImageView       { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHISampler         { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIShaderModule    { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIPipeline        { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIPipelineLayout  { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIDescriptorSetLayout { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIDescriptorPool  { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIDescriptorSet   { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIRenderPass      { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHIFramebuffer     { Handle handle = 0; bool IsValid() const { return handle != 0; } };
struct RHICommandBuffer   { Handle handle = 0; bool IsValid() const { return handle != 0; } };

// ---- Description structs (backend-neutral) ----

struct RHIViewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct RHIRect2D {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct RHIVertexInputBinding {
    uint32_t binding = 0;
    uint32_t stride = 0;
    VertexInputRate inputRate = VertexInputRate::Vertex;
};

struct RHIVertexAttribute {
    uint32_t location = 0;
    uint32_t binding = 0;
    Format format = Format::R32G32B32_SFLOAT;
    uint32_t offset = 0;
};

struct RHIDescriptorBinding {
    uint32_t binding = 0;
    DescriptorType type = DescriptorType::CombinedImageSampler;
    uint32_t count = 1;
    ShaderStage stage = ShaderStage::Fragment;
};

struct RHIPushConstantRange {
    ShaderStageMask stage = ShaderStageMask::Vertex;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct RHIShaderStageInfo {
    ShaderStage stage = ShaderStage::Vertex;
    RHIShaderModule module;
    const char* entryPoint = "main";
};

struct RHIBlendState {
    bool enable = false;
};

struct RHIPipelineDesc {
    RHIPipelineLayout layout;
    RHIRenderPass renderPass;
    std::vector<RHIShaderStageInfo> stages;
    RHIVertexInputBinding vertexBinding;
    std::vector<RHIVertexAttribute> vertexAttributes;
    Topology topology = Topology::TriangleList;
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::Back;
    bool depthTestEnable = true;
    RHIBlendState blend;
};

struct RHIViewportState {
    RHIViewport viewport;
    RHIRect2D scissor;
};

struct RHIClearValue {
    // color (rgba) or depthStencil — only one is meaningful per attachment.
    Vector4 color = {0.0f, 0.0f, 0.0f, 1.0f};
    float depth = 1.0f;
    uint32_t stencil = 0;
};

struct RHIDescriptorImageInfo {
    RHIImageView imageView;
    RHISampler sampler;
    bool valid = false;
};

struct RHIDescriptorBufferInfo {
    RHIBuffer buffer;
    uint32_t offset = 0;
    uint32_t range = 0;
    bool valid = false;
};

struct RHIDescriptorWrite {
    RHIDescriptorSet dstSet;
    uint32_t dstBinding = 0;
    uint32_t count = 1;
    DescriptorType type = DescriptorType::CombinedImageSampler;
    RHIDescriptorImageInfo imageInfo;
    RHIDescriptorBufferInfo bufferInfo;
};

} // namespace RHI
} // namespace Leir
