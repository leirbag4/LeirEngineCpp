#include "LeirEngine/RHI/WebGPUBackend.h"

#include "LeirEngine/Core/Log.h"

#if (defined(_WIN32) && defined(_MSC_VER)) || defined(__EMSCRIPTEN__)

#if defined(__EMSCRIPTEN__)
// Emscripten: emdawnwebgpu (vendored + patched) provides the standard
// webgpu.h and links the wgpu functions STATICALLY (no DLL, no native
// extension header). GLFW comes from the contrib.glfw3 port.
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>
#else
#define WGPU_SKIP_DECLARATIONS
#include <webgpu/webgpu.h>
// wgpu-native extension header: defines WGPUNativeFeature_* (native-only device
// features such as texture binding arrays) that extend WGPUFeatureName values.
#define WGPU_SKIP_DECLARATIONS
#include <webgpu/wgpu.h>

#ifndef NOMINMAX
#define NOMINMAX // keep std::min/std::max usable (windows.h min/max macros)
#endif
#include <windows.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// WebGPU implementation of the RenderBackend interface (wgpu-native v29).
//
// Handle mapping: every RHI handle stores a pointer to a backend record
// allocated with new (cast to uint64_t). The command buffer handle stores the
// WGPUCommandEncoder pointer.
//
// wgpu_native.dll is loaded DYNAMICALLY (LoadLibrary + GetProcAddress): the
// WGPU_SKIP_DECLARATIONS guard hides the exported wgpu* declarations, keeping
// only the types + WGPUProc* typedefs, so no import lib is needed and the DLL
// is never a hard dependency (Vulkan/D3D12 users never load it).
//
// Coordinate convention (IMPORTANT, fixed 2026-08-15): WebGPU uses the
// D3D12/Metal convention — NDC is Y-up (like GLM/OpenGL), framebuffer Y-down
// (top-left origin). This is the OPPOSITE of Vulkan (NDC Y-down). So the
// engine's GLM projections map 1:1 with POSITIVE-height viewports, exactly like
// the D3D12 backend. NO negative-height flip is applied (an earlier draft baked
// in the Vulkan flip; the spec gpuweb#416 "Solution 1" confirms Y-up NDC).

namespace Leir {
namespace RHI {

namespace {

// Bindless table size. Kept at the WebGPU default limit
// (maxSampledTexturesPerShaderStage = 16) so no requiredLimits are needed and
// the device creation is portable. WGSL shaders declare the binding_array with
// this same size (see engine/shaders/*.wgsl).
constexpr uint32_t kBindlessMax = 16;
constexpr uint32_t kMaxColorAttachments = 8;
// wgpuQueueWriteBuffer chunks (kept well under any per-call size limit).
constexpr uint32_t kQueueWriteChunk = 65536;

WGPUStringView WgpuStr(const char* s) {
    WGPUStringView v{};
    v.data = s;
    v.length = s ? std::strlen(s) : 0;
    return v;
}

std::string WgpuStrView(WGPUStringView v) {
    if (!v.data || !v.length) return {};
    return std::string(v.data, v.length);
}

WGPUTextureFormat ToWgpu(Format f) {
    switch (f) {
        case Format::R32G32_SFLOAT:       return WGPUTextureFormat_RG32Float;
        case Format::R32G32B32A32_SFLOAT: return WGPUTextureFormat_RGBA32Float;
        case Format::R8G8B8A8_SRGB:       return WGPUTextureFormat_RGBA8UnormSrgb;
        case Format::B8G8R8A8_SRGB:       return WGPUTextureFormat_BGRA8UnormSrgb;
        case Format::D32_SFLOAT:          return WGPUTextureFormat_Depth32Float;
        case Format::R32_SFLOAT:          return WGPUTextureFormat_R32Float;
        default:                          return WGPUTextureFormat_Undefined; // R32G32B32_SFLOAT is vertex-only
    }
}

WGPUVertexFormat ToVertexFormat(Format f) {
    switch (f) {
        case Format::R32G32B32_SFLOAT:    return WGPUVertexFormat_Float32x3;
        case Format::R32G32_SFLOAT:       return WGPUVertexFormat_Float32x2;
        case Format::R32G32B32A32_SFLOAT: return WGPUVertexFormat_Float32x4;
        case Format::R32_SFLOAT:          return WGPUVertexFormat_Float32;
        default:                          return WGPUVertexFormat_Float32x3;
    }
}

uint64_t WgpuUsage(BufferUsage usage, bool hostVisible) {
    uint64_t u = 0;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::TransferSrc))
        u |= WGPUBufferUsage_CopySrc;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::TransferDst))
        u |= WGPUBufferUsage_CopyDst;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::Vertex))
        u |= WGPUBufferUsage_Vertex;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::Index))
        u |= WGPUBufferUsage_Index;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::Uniform))
        u |= WGPUBufferUsage_Uniform;
    // Host-visible buffers are re-uploaded from their shadow copy on
    // UnmapMemory via wgpuQueueWriteBuffer, which needs CopyDst.
    if (hostVisible) u |= WGPUBufferUsage_CopyDst;
    return u;
}

uint64_t WgpuUsage(ImageUsage usage) {
    uint64_t u = 0;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::TransferDst))
        u |= WGPUTextureUsage_CopyDst;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::TransferSrc))
        u |= WGPUTextureUsage_CopySrc;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::Sampled))
        u |= WGPUTextureUsage_TextureBinding;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::ColorAttachment))
        u |= WGPUTextureUsage_RenderAttachment;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::DepthStencilAttachment))
        u |= WGPUTextureUsage_RenderAttachment;
    return u;
}

// ---- async init callbacks (blocked on via wgpuInstanceWaitAny) ----

struct InitCtx {
    bool done = false;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    bool success = false;
    std::string message;
};

void OnAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
               WGPUStringView message, void* userdata1, void* /*userdata2*/) {
    InitCtx* ctx = static_cast<InitCtx*>(userdata1);
    ctx->done = true;
    if (status == WGPURequestAdapterStatus_Success && adapter) {
        ctx->adapter = adapter;
        ctx->success = true;
    } else {
        ctx->message = WgpuStrView(message);
    }
}

void OnDevice(WGPURequestDeviceStatus status, WGPUDevice device,
              WGPUStringView message, void* userdata1, void* /*userdata2*/) {
    InitCtx* ctx = static_cast<InitCtx*>(userdata1);
    ctx->done = true;
    if (status == WGPURequestDeviceStatus_Success && device) {
        ctx->device = device;
        ctx->success = true;
    } else {
        ctx->message = WgpuStrView(message);
    }
}

// ---- records (RHI handle -> record pointer) ----

struct BufferRec {
    WGPUBuffer buffer = nullptr;
    uint32_t size = 0;
    bool hostVisible = false;
    std::vector<uint8_t> shadow; // host copy for MapMemory/UnmapMemory
    uint32_t refs = 2;           // shared with the memory record
};

struct ImageRec {
    WGPUTexture texture = nullptr;
    uint32_t width = 0, height = 0;
    uint32_t refs = 2; // shared with the memory record
};

struct MemoryRec {
    void* resource = nullptr; // BufferRec* or ImageRec*
    bool isBuffer = false;
};

struct ImageViewRec {
    ImageRec* image = nullptr;
    WGPUTextureView view = nullptr;
    bool isDepth = false;
};

struct SamplerRec {
    WGPUSampler sampler = nullptr;
    bool needsRelease = false;
};

struct ShaderRec {
    WGPUShaderModule module = nullptr;
};

struct DescSetLayoutRec {
    DescriptorType type = DescriptorType::CombinedImageSampler;
    ShaderStage stage = ShaderStage::Fragment;
    bool bindless = false;
    // Owning layout for regular (non-bindless) sets; the shared cached
    // bindless layout otherwise. A bind group created from this layout MUST be
    // bound only against a pipeline whose layout uses this same object.
    WGPUBindGroupLayout wgpuLayout = nullptr;
    bool ownsLayout = false;
};

struct DescPoolRec {};

struct DescSetRec {
    bool isUbo = false;
    bool isBindless = false;
    WGPUBindGroupLayout layout = nullptr; // the set's layout object
    WGPUBindGroup bindGroup = nullptr;
    bool ownsBindGroup = false;
};

struct RenderPassRec {
    std::vector<WGPUTextureFormat> colorFormats;
    WGPUTextureFormat depthFormat = WGPUTextureFormat_Undefined;
    bool hasDepth = false;
    bool overlay = false;
};

struct PassTemplateRec {
    std::vector<RHIClearValue> clears;
    // Viewport/scissor as-is (D3D12 convention: positive-height viewport).
    float vpX = 0.0f, vpY = 0.0f, vpW = 0.0f, vpH = 0.0f;
    uint32_t scX = 0, scY = 0, scW = 0, scH = 0;
};

struct FramebufferRec {
    uint32_t width = 0, height = 0;
    std::vector<ImageViewRec*> colorAttachments;
    ImageViewRec* depthAttachment = nullptr;
};

struct PipelineLayoutRec {
    WGPUPipelineLayout wgpuLayout = nullptr;
    std::vector<DescSetLayoutRec*> setLayouts;
    // Push constants are emulated with a per-layout UBO at group
    // index = setLayouts.size() (the WGSL shaders declare the push uniform at
    // that group).
    bool hasPush = false;
    uint32_t pushGroup = 0;
    uint32_t pushSize = 0;
    // Push constants are emulated with one UBO per draw slot (the web executor
    // records several draws per frame; a single buffer written via
    // QueueWriteBuffer would make every draw read the LAST write, since queue
    // writes all complete before the command buffer executes). Grown lazily.
    // The slot counter is per-layout (not a graph-global): a graph may push on
    // several pipeline layouts (e.g. the editor grid + the scene meshes), and
    // each layout needs its own independent slots so a draw's push binds at its
    // own group index. Reset to 0 by CmdExecuteGraph at the start of every graph.
    uint32_t pushSlot = 0;
    std::vector<WGPUBuffer> pushBuffers;
    std::vector<WGPUBindGroup> pushBindGroups;
};

struct PipelineRec {
    WGPURenderPipeline pipeline = nullptr;
    PipelineLayoutRec* layout = nullptr;
};

} // namespace

struct WebGPUBackend::Impl {
    // ---- wgpu_native.dll (loaded dynamically on desktop) ----
#if defined(__EMSCRIPTEN__)
    GLFWwindow* window = nullptr;
#else
    HMODULE lib = nullptr;
    GLFWwindow* window = nullptr;
#endif

    WGPUProcCreateInstance CreateInstance = nullptr;
    WGPUProcInstanceRequestAdapter InstanceRequestAdapter = nullptr;
    WGPUProcInstanceWaitAny InstanceWaitAny = nullptr;
    WGPUProcInstanceProcessEvents InstanceProcessEvents = nullptr;
    WGPUProcInstanceCreateSurface InstanceCreateSurface = nullptr;
    WGPUProcInstanceRelease InstanceRelease = nullptr;
    WGPUProcAdapterRequestDevice AdapterRequestDevice = nullptr;
    WGPUProcAdapterGetInfo AdapterGetInfo = nullptr;
    WGPUProcAdapterRelease AdapterRelease = nullptr;
    WGPUProcDeviceRelease DeviceRelease = nullptr;
    // wgpuDevicePoll was removed from webgpu.h in v29 but is still exported by
    // wgpu-native (as an extension) — declared locally so it can be loaded
    // dynamically and used for a blocking WaitIdle.
    using DevicePollFn = WGPUBool (*)(WGPUDevice, bool,
        const WGPUQueueWorkDoneCallbackInfo*);
    DevicePollFn DevicePoll = nullptr;
    WGPUProcDeviceGetQueue DeviceGetQueue = nullptr;
    WGPUProcDeviceCreateCommandEncoder DeviceCreateCommandEncoder = nullptr;
    WGPUProcDeviceCreateShaderModule DeviceCreateShaderModule = nullptr;
    WGPUProcDeviceCreateRenderPipeline DeviceCreateRenderPipeline = nullptr;
    WGPUProcDeviceCreatePipelineLayout DeviceCreatePipelineLayout = nullptr;
    WGPUProcDeviceCreateBindGroupLayout DeviceCreateBindGroupLayout = nullptr;
    WGPUProcDeviceCreateBindGroup DeviceCreateBindGroup = nullptr;
    WGPUProcDeviceCreateBuffer DeviceCreateBuffer = nullptr;
    WGPUProcDeviceCreateTexture DeviceCreateTexture = nullptr;
    WGPUProcTextureCreateView TextureCreateView = nullptr;
    WGPUProcDeviceCreateSampler DeviceCreateSampler = nullptr;
    WGPUProcSurfaceConfigure SurfaceConfigure = nullptr;
    WGPUProcSurfaceGetCapabilities SurfaceGetCapabilities = nullptr;
    WGPUProcSurfaceGetCurrentTexture SurfaceGetCurrentTexture = nullptr;
    WGPUProcSurfacePresent SurfacePresent = nullptr;
    WGPUProcSurfaceRelease SurfaceRelease = nullptr;
    WGPUProcCommandEncoderFinish CommandEncoderFinish = nullptr;
    WGPUProcCommandEncoderBeginRenderPass CommandEncoderBeginRenderPass = nullptr;
    WGPUProcCommandEncoderCopyBufferToBuffer CommandEncoderCopyBufferToBuffer = nullptr;
    WGPUProcCommandEncoderCopyBufferToTexture CommandEncoderCopyBufferToTexture = nullptr;
    WGPUProcCommandEncoderRelease CommandEncoderRelease = nullptr;
    WGPUProcCommandBufferRelease CommandBufferRelease = nullptr;
    WGPUProcRenderPassEncoderEnd RenderPassEncoderEnd = nullptr;
    WGPUProcRenderPassEncoderSetPipeline RenderPassEncoderSetPipeline = nullptr;
    WGPUProcRenderPassEncoderSetBindGroup RenderPassEncoderSetBindGroup = nullptr;
    WGPUProcRenderPassEncoderSetVertexBuffer RenderPassEncoderSetVertexBuffer = nullptr;
    WGPUProcRenderPassEncoderSetIndexBuffer RenderPassEncoderSetIndexBuffer = nullptr;
    WGPUProcRenderPassEncoderDraw RenderPassEncoderDraw = nullptr;
    WGPUProcRenderPassEncoderDrawIndexed RenderPassEncoderDrawIndexed = nullptr;
    WGPUProcRenderPassEncoderSetViewport RenderPassEncoderSetViewport = nullptr;
    WGPUProcRenderPassEncoderSetScissorRect RenderPassEncoderSetScissorRect = nullptr;
    WGPUProcRenderPassEncoderRelease RenderPassEncoderRelease = nullptr;
    WGPUProcQueueSubmit QueueSubmit = nullptr;
    WGPUProcQueueWriteBuffer QueueWriteBuffer = nullptr;
    WGPUProcQueueWriteTexture QueueWriteTexture = nullptr;
    WGPUProcQueueRelease QueueRelease = nullptr;
    WGPUProcBufferRelease BufferRelease = nullptr;
    WGPUProcTextureRelease TextureRelease = nullptr;
    WGPUProcTextureViewRelease TextureViewRelease = nullptr;
    WGPUProcSamplerRelease SamplerRelease = nullptr;
    WGPUProcBindGroupLayoutRelease BindGroupLayoutRelease = nullptr;
    WGPUProcBindGroupRelease BindGroupRelease = nullptr;
    WGPUProcPipelineLayoutRelease PipelineLayoutRelease = nullptr;
    WGPUProcRenderPipelineRelease RenderPipelineRelease = nullptr;
    WGPUProcShaderModuleRelease ShaderModuleRelease = nullptr;

    #if !defined(__EMSCRIPTEN__)
    template <typename T>
    bool LoadProc(const char* name, T& out) {
        out = reinterpret_cast<T>(GetProcAddress(lib, name));
        if (!out) {
            XConsole::PrintError("WebGPU: missing export '{}' in wgpu_native.dll", name);
            return false;
        }
        return true;
    }
#endif

    // ---- objects ----
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUSurface surface = nullptr;
#if !defined(__EMSCRIPTEN__)
    HWND hwnd = nullptr;
#endif
    int width = 0, height = 0;
    bool vsync = false;
    WGPUTextureFormat swapchainFormat = WGPUTextureFormat_Undefined;

    // Per-frame state
    WGPUCommandEncoder encoder = nullptr;
    WGPURenderPassEncoder currentPass = nullptr;
    WGPUTexture swapchainTexture = nullptr; // acquired this frame
    WGPUTextureView swapchainView = nullptr;
    bool swapchainWasWritten = false; // main pass (BeginFrame(false)) ran this frame
    bool resized = false;

    // Depth attachment for the swapchain main pass (PhysicsDemo).
    WGPUTexture depthTexture = nullptr;
    WGPUTextureView depthView = nullptr;
    uint32_t depthW = 0, depthH = 0;

    // Shared bindless layout + bind group (rebuilt on register/update/unregister).
    WGPUBindGroupLayout bindlessLayout = nullptr;
    WGPUBindGroup bindlessBindGroup = nullptr;
    WGPUBindGroupLayout pushLayout = nullptr; // shared uniform layout for push UBOs
    uint32_t bindlessNext = 0;
    std::vector<uint32_t> bindlessFree;
    std::unordered_map<uint32_t, ImageViewRec*> bindlessViews;
    std::unordered_map<uint32_t, SamplerRec*> bindlessSamplers;
    DescSetRec* bindlessSetRec = nullptr; // shared singleton handed to GetBindlessDescriptorSet

#if defined(__EMSCRIPTEN__)
    // Browser builds: the shared bind group is only the default (dummy)
    // fallback — each draw's texture is bound per-draw via GetTextureBindGroup
    // (naga cannot compile binding_array, so the table degrades to a single
    // texture/sampler pair per bind group).
    std::unordered_map<uint32_t, WGPUBindGroup> textureBindGroups;
    int bindlessSetSlot = -1; // bindless set index of the current graph, -1 if none
#endif

    // Dummy white texture filling unbound bindless slots (1x1 R8G8B8A8Unorm).
    WGPUTexture dummyTexture = nullptr;
    WGPUTextureView dummyView = nullptr;
    WGPUSampler dummySampler = nullptr;

    // Render-target masking: wgpu forbids a texture from being BOTH a render
    // target and present in a bound bind group within the same pass (exclusive
    // usage). While a pass renders to a bindless-registered attachment its
    // table slot is masked to the dummy view, then restored at EndRenderPass.
    std::vector<uint32_t> maskedSlots;
    std::vector<ImageViewRec*> maskedOrig;

    // Size of the current render pass target (for clamping scissor rects,
    // which wgpu validates strictly against the attachment).
    uint32_t passW = 0, passH = 0;

    RenderPassRec* mainRenderPass = nullptr;
    RenderPassRec* overlayRenderPass = nullptr;

    GCaps caps;

    Impl(void* window, int w, int h, bool vs, const std::string& /*appName*/) {
        this->window = static_cast<GLFWwindow*>(window);
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(this->window, &fbW, &fbH);
        width = fbW > 0 ? fbW : w;
        height = fbH > 0 ? fbH : h;
        vsync = vs;

#if defined(__EMSCRIPTEN__)
        // Static linking (emdawnwebgpu): every proc pointer resolves to the
        // direct exported symbol. wgpuDevicePoll is a wgpu-native extension
        // and does NOT exist here (WaitIdle degrades to a no-op).
        CreateInstance = wgpuCreateInstance;
        InstanceRequestAdapter = wgpuInstanceRequestAdapter;
        InstanceWaitAny = wgpuInstanceWaitAny;
        InstanceProcessEvents = wgpuInstanceProcessEvents;
        InstanceCreateSurface = wgpuInstanceCreateSurface;
        InstanceRelease = wgpuInstanceRelease;
        AdapterRequestDevice = wgpuAdapterRequestDevice;
        AdapterGetInfo = wgpuAdapterGetInfo;
        AdapterRelease = wgpuAdapterRelease;
        DeviceRelease = wgpuDeviceRelease;
        DevicePoll = nullptr;
        DeviceGetQueue = wgpuDeviceGetQueue;
        DeviceCreateCommandEncoder = wgpuDeviceCreateCommandEncoder;
        DeviceCreateShaderModule = wgpuDeviceCreateShaderModule;
        DeviceCreateRenderPipeline = wgpuDeviceCreateRenderPipeline;
        DeviceCreatePipelineLayout = wgpuDeviceCreatePipelineLayout;
        DeviceCreateBindGroupLayout = wgpuDeviceCreateBindGroupLayout;
        DeviceCreateBindGroup = wgpuDeviceCreateBindGroup;
        DeviceCreateBuffer = wgpuDeviceCreateBuffer;
        DeviceCreateTexture = wgpuDeviceCreateTexture;
        TextureCreateView = wgpuTextureCreateView;
        DeviceCreateSampler = wgpuDeviceCreateSampler;
        SurfaceConfigure = wgpuSurfaceConfigure;
        SurfaceGetCapabilities = wgpuSurfaceGetCapabilities;
        SurfaceGetCurrentTexture = wgpuSurfaceGetCurrentTexture;
        SurfacePresent = wgpuSurfacePresent;
        SurfaceRelease = wgpuSurfaceRelease;
        CommandEncoderFinish = wgpuCommandEncoderFinish;
        CommandEncoderBeginRenderPass = wgpuCommandEncoderBeginRenderPass;
        CommandEncoderCopyBufferToBuffer = wgpuCommandEncoderCopyBufferToBuffer;
        CommandEncoderCopyBufferToTexture = wgpuCommandEncoderCopyBufferToTexture;
        CommandEncoderRelease = wgpuCommandEncoderRelease;
        CommandBufferRelease = wgpuCommandBufferRelease;
        RenderPassEncoderEnd = wgpuRenderPassEncoderEnd;
        RenderPassEncoderSetPipeline = wgpuRenderPassEncoderSetPipeline;
        RenderPassEncoderSetBindGroup = wgpuRenderPassEncoderSetBindGroup;
        RenderPassEncoderSetVertexBuffer = wgpuRenderPassEncoderSetVertexBuffer;
        RenderPassEncoderSetIndexBuffer = wgpuRenderPassEncoderSetIndexBuffer;
        RenderPassEncoderDraw = wgpuRenderPassEncoderDraw;
        RenderPassEncoderDrawIndexed = wgpuRenderPassEncoderDrawIndexed;
        RenderPassEncoderSetViewport = wgpuRenderPassEncoderSetViewport;
        RenderPassEncoderSetScissorRect = wgpuRenderPassEncoderSetScissorRect;
        RenderPassEncoderRelease = wgpuRenderPassEncoderRelease;
        QueueSubmit = wgpuQueueSubmit;
        QueueWriteBuffer = wgpuQueueWriteBuffer;
        QueueWriteTexture = wgpuQueueWriteTexture;
        QueueRelease = wgpuQueueRelease;
        BufferRelease = wgpuBufferRelease;
        TextureRelease = wgpuTextureRelease;
        TextureViewRelease = wgpuTextureViewRelease;
        SamplerRelease = wgpuSamplerRelease;
        BindGroupLayoutRelease = wgpuBindGroupLayoutRelease;
        BindGroupRelease = wgpuBindGroupRelease;
        PipelineLayoutRelease = wgpuPipelineLayoutRelease;
        RenderPipelineRelease = wgpuRenderPipelineRelease;
        ShaderModuleRelease = wgpuShaderModuleRelease;
#else
        hwnd = glfwGetWin32Window(this->window);
        lib = LoadLibraryW(L"wgpu_native.dll");
        if (!lib) {
            XConsole::PrintError(
                "WebGPU: wgpu_native.dll not found (needed for the 'webgpu' backend). "
                "Place it next to the executable or on PATH.");
            throw std::runtime_error("WebGPU: wgpu_native.dll not found");
        }

        if (!(LoadProc("wgpuCreateInstance", CreateInstance) &&
              LoadProc("wgpuInstanceRequestAdapter", InstanceRequestAdapter) &&
              LoadProc("wgpuInstanceWaitAny", InstanceWaitAny) &&
              LoadProc("wgpuInstanceProcessEvents", InstanceProcessEvents) &&
              LoadProc("wgpuInstanceCreateSurface", InstanceCreateSurface) &&
              LoadProc("wgpuInstanceRelease", InstanceRelease) &&
              LoadProc("wgpuAdapterRequestDevice", AdapterRequestDevice) &&
              LoadProc("wgpuAdapterGetInfo", AdapterGetInfo) &&
              LoadProc("wgpuAdapterRelease", AdapterRelease) &&
              LoadProc("wgpuDeviceRelease", DeviceRelease) &&
              LoadProc("wgpuDevicePoll", DevicePoll) &&
              LoadProc("wgpuDeviceGetQueue", DeviceGetQueue) &&
              LoadProc("wgpuDeviceCreateCommandEncoder", DeviceCreateCommandEncoder) &&
              LoadProc("wgpuDeviceCreateShaderModule", DeviceCreateShaderModule) &&
              LoadProc("wgpuDeviceCreateRenderPipeline", DeviceCreateRenderPipeline) &&
              LoadProc("wgpuDeviceCreatePipelineLayout", DeviceCreatePipelineLayout) &&
              LoadProc("wgpuDeviceCreateBindGroupLayout", DeviceCreateBindGroupLayout) &&
              LoadProc("wgpuDeviceCreateBindGroup", DeviceCreateBindGroup) &&
              LoadProc("wgpuDeviceCreateBuffer", DeviceCreateBuffer) &&
              LoadProc("wgpuDeviceCreateTexture", DeviceCreateTexture) &&
              LoadProc("wgpuTextureCreateView", TextureCreateView) &&
              LoadProc("wgpuDeviceCreateSampler", DeviceCreateSampler) &&
              LoadProc("wgpuSurfaceConfigure", SurfaceConfigure) &&
              LoadProc("wgpuSurfaceGetCapabilities", SurfaceGetCapabilities) &&
              LoadProc("wgpuSurfaceGetCurrentTexture", SurfaceGetCurrentTexture) &&
              LoadProc("wgpuSurfacePresent", SurfacePresent) &&
              LoadProc("wgpuSurfaceRelease", SurfaceRelease) &&
              LoadProc("wgpuCommandEncoderFinish", CommandEncoderFinish) &&
              LoadProc("wgpuCommandEncoderBeginRenderPass", CommandEncoderBeginRenderPass) &&
              LoadProc("wgpuCommandEncoderCopyBufferToBuffer", CommandEncoderCopyBufferToBuffer) &&
              LoadProc("wgpuCommandEncoderCopyBufferToTexture", CommandEncoderCopyBufferToTexture) &&
              LoadProc("wgpuCommandEncoderRelease", CommandEncoderRelease) &&
              LoadProc("wgpuCommandBufferRelease", CommandBufferRelease) &&
              LoadProc("wgpuRenderPassEncoderEnd", RenderPassEncoderEnd) &&
              LoadProc("wgpuRenderPassEncoderSetPipeline", RenderPassEncoderSetPipeline) &&
              LoadProc("wgpuRenderPassEncoderSetBindGroup", RenderPassEncoderSetBindGroup) &&
              LoadProc("wgpuRenderPassEncoderSetVertexBuffer", RenderPassEncoderSetVertexBuffer) &&
              LoadProc("wgpuRenderPassEncoderSetIndexBuffer", RenderPassEncoderSetIndexBuffer) &&
              LoadProc("wgpuRenderPassEncoderDraw", RenderPassEncoderDraw) &&
              LoadProc("wgpuRenderPassEncoderDrawIndexed", RenderPassEncoderDrawIndexed) &&
              LoadProc("wgpuRenderPassEncoderSetViewport", RenderPassEncoderSetViewport) &&
              LoadProc("wgpuRenderPassEncoderSetScissorRect", RenderPassEncoderSetScissorRect) &&
              LoadProc("wgpuRenderPassEncoderRelease", RenderPassEncoderRelease) &&
              LoadProc("wgpuQueueSubmit", QueueSubmit) &&
              LoadProc("wgpuQueueWriteBuffer", QueueWriteBuffer) &&
              LoadProc("wgpuQueueWriteTexture", QueueWriteTexture) &&
              LoadProc("wgpuQueueRelease", QueueRelease) &&
              LoadProc("wgpuBufferRelease", BufferRelease) &&
              LoadProc("wgpuTextureRelease", TextureRelease) &&
              LoadProc("wgpuTextureViewRelease", TextureViewRelease) &&
              LoadProc("wgpuSamplerRelease", SamplerRelease) &&
              LoadProc("wgpuBindGroupLayoutRelease", BindGroupLayoutRelease) &&
              LoadProc("wgpuBindGroupRelease", BindGroupRelease) &&
              LoadProc("wgpuPipelineLayoutRelease", PipelineLayoutRelease) &&
              LoadProc("wgpuRenderPipelineRelease", RenderPipelineRelease) &&
              LoadProc("wgpuShaderModuleRelease", ShaderModuleRelease)))
            throw std::runtime_error("WebGPU: failed to resolve wgpu_native.dll exports");
#endif

        Init();
    }

    void Init() {
#if defined(__EMSCRIPTEN__)
        // Browser instance: enable TimedWaitAny — required for the blocking
        // adapter/device WaitAny(UINT64_MAX) below (emdawn glue returns
        // WGPUWaitStatus_Error otherwise).
        WGPUInstanceFeatureName instFeatures[1] = { WGPUInstanceFeatureName_TimedWaitAny };
        WGPUInstanceLimits instLimits{};
        instLimits.timedWaitAnyMaxCount = 1;
        WGPUInstanceDescriptor instDesc{};
        instDesc.requiredFeatureCount = 1;
        instDesc.requiredFeatures = instFeatures;
        instDesc.requiredLimits = &instLimits;
        instance = CreateInstance(&instDesc);
        if (!instance)
            throw std::runtime_error("WebGPU: wgpuCreateInstance failed");

        // Canvas surface (GLFW's Emscripten port creates the "#canvas" element).
        WGPUEmscriptenSurfaceSourceCanvasHTMLSelector srcDesc{};
        srcDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
        srcDesc.selector = WgpuStr("#canvas");

        WGPUSurfaceDescriptor surfDesc{};
        surfDesc.label = WgpuStr("LeirEngine surface");
        surfDesc.nextInChain = &srcDesc.chain;
        surface = InstanceCreateSurface(instance, &surfDesc);
        if (!surface)
            throw std::runtime_error("WebGPU: failed to create surface");

        // Adapter request (async; completed synchronously via wgpuInstanceWaitAny,
        // which requires -sASYNCIFY=1 in the build).
        InitCtx actx{};
        WGPURequestAdapterOptions opts{};
        opts.compatibleSurface = surface;
        WGPURequestAdapterCallbackInfo acb{};
        acb.mode = WGPUCallbackMode_WaitAnyOnly;
        acb.callback = OnAdapter;
        acb.userdata1 = &actx;
        WGPUFuture afut = InstanceRequestAdapter(instance, &opts, acb);
        WGPUFutureWaitInfo awi{};
        awi.future = afut;
        if (InstanceWaitAny(instance, 1, &awi, UINT64_MAX) != WGPUWaitStatus_Success ||
            !actx.done || !actx.success) {
            InstanceRelease(instance);
            instance = nullptr;
            throw std::runtime_error("WebGPU: no compatible adapter");
        }
        adapter = actx.adapter;

        // Adapter diagnostics.
        {
            WGPUAdapterInfo info{};
            if (AdapterGetInfo(adapter, &info) == WGPUStatus_Success) {
                XConsole::Println("[WebGPU] adapter: browser / {} / {}", 
                    WgpuStrView(info.device), WgpuStrView(info.description));
            }
        }

        // Device request (async; completed synchronously via WaitAny).
        // No requiredFeatures/requiredLimits: browsers enable the
        // texture-binding-array capabilities by default and the default limits
        // (16 sampled textures + 16 samplers per stage) match kBindlessMax.
        InitCtx dctx{};
        WGPUDeviceDescriptor devDesc{};
        devDesc.label = WgpuStr("LeirEngine device");
        WGPURequestDeviceCallbackInfo dcb{};
        dcb.mode = WGPUCallbackMode_WaitAnyOnly;
        dcb.callback = OnDevice;
        dcb.userdata1 = &dctx;
        WGPUFuture dfut = AdapterRequestDevice(adapter, &devDesc, dcb);
        WGPUFutureWaitInfo dwi{};
        dwi.future = dfut;
        if (InstanceWaitAny(instance, 1, &dwi, UINT64_MAX) != WGPUWaitStatus_Success ||
            !dctx.done || !dctx.success) {
            AdapterRelease(adapter);
            adapter = nullptr;
            InstanceRelease(instance);
            instance = nullptr;
            throw std::runtime_error("WebGPU: failed to create device: " +
                dctx.message);
        }
        device = dctx.device;
        // NOTE: the adapter is kept alive for the backend's lifetime —
        // wgpuSurfaceGetCapabilities needs it on every (re)configure.
        queue = DeviceGetQueue(device);
#else
        // Prefer the DX12 backend on Windows: wgpu's Vulkan TEXTURE_BINDING_ARRAY
        // feature requires shaderSampledImageArrayNonUniformIndexing (Gen11+),
        // which Intel Gen9 iGPUs lack, so binding arrays can't be requested on a
        // Vulkan device at all there. DX12 (binding tier 3) supports them.
        WGPUInstanceDescriptor instDesc{};
        WGPUInstanceExtras extras{};
        extras.chain.sType = static_cast<WGPUSType>(WGPUSType_InstanceExtras);
        extras.backends = WGPUInstanceBackend_DX12;
        instDesc.nextInChain = &extras.chain;
        instance = CreateInstance(&instDesc);
        if (!instance)
            throw std::runtime_error("WebGPU: wgpuCreateInstance failed");

        // Surface (HWND) before the adapter request so the adapter is
        // compatible with the surface.
        WGPUSurfaceSourceWindowsHWND srcDesc{};
        srcDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
        srcDesc.hinstance = GetModuleHandleW(nullptr);
        srcDesc.hwnd = hwnd;

        WGPUSurfaceDescriptor surfDesc{};
        surfDesc.label = WgpuStr("LeirEngine surface");
        surfDesc.nextInChain = &srcDesc.chain;
        surface = InstanceCreateSurface(instance, &surfDesc);
        if (!surface)
            throw std::runtime_error("WebGPU: failed to create surface");

        // Adapter request (async; driven by wgpuInstanceProcessEvents).
        InitCtx actx{};
        WGPURequestAdapterOptions opts{};
        opts.compatibleSurface = surface;
        WGPURequestAdapterCallbackInfo acb{};
        acb.mode = WGPUCallbackMode_AllowProcessEvents;
        acb.callback = OnAdapter;
        acb.userdata1 = &actx;
        InstanceRequestAdapter(instance, &opts, acb);
        while (!actx.done)
            InstanceProcessEvents(instance);
        if (!actx.success) {
            InstanceRelease(instance);
            instance = nullptr;
            throw std::runtime_error("WebGPU: no compatible adapter");
        }
        adapter = actx.adapter;

        // Adapter diagnostics.
        {
            WGPUAdapterInfo info{};
            if (AdapterGetInfo(adapter, &info) == WGPUStatus_Success) {
                const char* bt = "?";
                switch (info.backendType) {
                    case WGPUBackendType_D3D12: bt = "d3d12"; break;
                    case WGPUBackendType_D3D11: bt = "d3d11"; break;
                    case WGPUBackendType_Vulkan: bt = "vulkan"; break;
                    case WGPUBackendType_Metal: bt = "metal"; break;
                    default: break;
                }
                XConsole::Println("[WebGPU] adapter: {} / {} / {}", bt,
                    WgpuStrView(info.device), WgpuStrView(info.description));
            }
        }

        // Device request (async; driven by wgpuInstanceProcessEvents).
        InitCtx dctx{};
        WGPUDeviceDescriptor devDesc{};
        devDesc.label = WgpuStr("LeirEngine device");
        // binding_array<texture_2d<N>> / <sampler<N>> require the native
        // texture-binding-array feature on the device (naga capability
        // TEXTURE_AND_SAMPLER_BINDING_ARRAY). UI.frag indexes the arrays with a
        // per-vertex varying, so naga also needs
        // TEXTURE_AND_SAMPLER_BINDING_ARRAY_NON_UNIFORM_INDEXING — that maps to
        // the native non-uniform-indexing feature (supported on DX12 tier 3;
        // on Vulkan it would require all six descriptor-indexing flags, which
        // Intel Gen9 lacks — hence the DX12 backend preference above).
        WGPUFeatureName bindlessFeatures[2] = {
            static_cast<WGPUFeatureName>(WGPUNativeFeature_TextureBindingArray),
            static_cast<WGPUFeatureName>(
                WGPUNativeFeature_SampledTextureAndStorageBufferArrayNonUniformIndexing),
        };
        devDesc.requiredFeatureCount = 2;
        devDesc.requiredFeatures = bindlessFeatures;
        // Binding arrays count against max_binding_array_elements_per_shader_stage
        // (native-only limit, WebGPU default is 0). 2 arrays of 16 are visible
        // to both VERTEX|FRAGMENT, so the non-sampler budget is 32 and the
        // sampler budget is 16.
        WGPUNativeLimits nativeLimits{};
        nativeLimits.chain.sType = static_cast<WGPUSType>(WGPUSType_NativeLimits);
        // maxNonSamplerBindings sizes the D3D12 shader-visible CBV/SRV/UAV heap
        // 1:1 — 0 (zero-init) makes CreateDescriptorHeap fail. Keep the default
        // 1M (matches the no-limits path).
        nativeLimits.maxNonSamplerBindings = WGPU_LIMIT_U32_UNDEFINED;
        nativeLimits.maxBindingArrayElementsPerShaderStage = kBindlessMax * 2;
        nativeLimits.maxBindingArraySamplerElementsPerShaderStage = kBindlessMax;
        WGPULimits limits = WGPU_LIMITS_INIT;
        limits.nextInChain = &nativeLimits.chain;
        devDesc.requiredLimits = &limits;
        WGPURequestDeviceCallbackInfo dcb{};
        dcb.mode = WGPUCallbackMode_AllowProcessEvents;
        dcb.callback = OnDevice;
        dcb.userdata1 = &dctx;
        AdapterRequestDevice(adapter, &devDesc, dcb);
        while (!dctx.done)
            InstanceProcessEvents(instance);
        if (!dctx.success) {
            AdapterRelease(adapter);
            adapter = nullptr;
            InstanceRelease(instance);
            instance = nullptr;
            throw std::runtime_error("WebGPU: failed to create device: " +
                dctx.message);
        }
        device = dctx.device;
        // NOTE: the adapter is kept alive for the backend's lifetime —
        // wgpuSurfaceGetCapabilities needs it on every (re)configure.
        queue = DeviceGetQueue(device);
#endif

        // Swapchain config.
        ConfigureSwapchain(width, height);

        // Shared bindless layout: binding 0 = texture, binding 1 = sampler. All
        // pipelines and the bindless bind group share this exact object
        // (WebGPU requires identity). Native builds make them binding_array<N>
        // (array sizes go in the extras chain — plain bindingArraySize is
        // ignored by wgpu-native); browser builds use single resources because
        // naga (Firefox) cannot compile binding_array at all.
        WGPUBindGroupLayoutEntry entries[2]{};
        entries[0].binding = 0;
        entries[0].visibility = static_cast<WGPUShaderStage>(
            WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
        entries[0].texture.sampleType = WGPUTextureSampleType_Float;
        entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
        entries[1].binding = 1;
        entries[1].visibility = static_cast<WGPUShaderStage>(
            WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
        entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
#if !defined(__EMSCRIPTEN__)
        WGPUBindGroupLayoutEntryExtras texExtras{};
        texExtras.chain.sType = static_cast<WGPUSType>(WGPUSType_BindGroupLayoutEntryExtras);
        texExtras.count = kBindlessMax;
        WGPUBindGroupLayoutEntryExtras sampExtras{};
        sampExtras.chain.sType = static_cast<WGPUSType>(WGPUSType_BindGroupLayoutEntryExtras);
        sampExtras.count = kBindlessMax;
        entries[0].bindingArraySize = kBindlessMax;
        entries[0].nextInChain = &texExtras.chain;
        entries[1].bindingArraySize = kBindlessMax;
        entries[1].nextInChain = &sampExtras.chain;
#endif

        WGPUBindGroupLayoutDescriptor bglDesc{};
        bglDesc.label = WgpuStr("bindless");
        bglDesc.entryCount = 2;
        bglDesc.entries = entries;
        bindlessLayout = DeviceCreateBindGroupLayout(device, &bglDesc);

        // Shared push-constants UBO layout (group index = setLayouts.size()).
        WGPUBindGroupLayoutEntry pushEntry{};
        pushEntry.binding = 0;
        pushEntry.visibility = static_cast<WGPUShaderStage>(
            WGPUShaderStage_Vertex | WGPUShaderStage_Fragment);
        pushEntry.buffer.type = WGPUBufferBindingType_Uniform;
        WGPUBindGroupLayoutDescriptor pushBglDesc{};
        pushBglDesc.label = WgpuStr("push");
        pushBglDesc.entryCount = 1;
        pushBglDesc.entries = &pushEntry;
        pushLayout = DeviceCreateBindGroupLayout(device, &pushBglDesc);

        // Dummy white texture for unbound bindless slots.
        CreateDummyTexture();

        // Built-in render passes (formats only; no native object).
        mainRenderPass = new RenderPassRec();
        mainRenderPass->colorFormats = { swapchainFormat };
        mainRenderPass->depthFormat = WGPUTextureFormat_Depth32Float;
        mainRenderPass->hasDepth = true;

        overlayRenderPass = new RenderPassRec();
        overlayRenderPass->colorFormats = { swapchainFormat };
        overlayRenderPass->hasDepth = false;
        overlayRenderPass->overlay = true;

        // ---- Capabilities ----
        caps.bindless = true;
        caps.maxTexturesPerTable = kBindlessMax;
        caps.maxUniformBuffersPerTable = kBindlessMax;
        caps.maxSamplersPerTable = kBindlessMax;
        caps.maxStorageBuffersPerTable = 0;
        caps.maxPushConstantsSize = 0; // emulated via UBO
        caps.maxColorAttachments = kMaxColorAttachments;
        caps.maxTextureSize = 8192;
        caps.minUniformBufferOffsetAlignment = 256;
        caps.multiRenderTarget = false; // MRT possible but unneeded now
        caps.instancing = true;
        caps.compute = true;
        caps.storageBuffers = false;
        caps.sRGB = true;
        caps.wireframe = false;
        caps.anisotropicFiltering = false;

        XConsole::Println("WebGPU backend created ({}x{})", width, height);
    }

    ~Impl() {
        if (device)
            WaitIdle();

        delete bindlessSetRec;
#if defined(__EMSCRIPTEN__)
        for (auto& kv : textureBindGroups) BindGroupRelease(kv.second);
        textureBindGroups.clear();
#endif
        if (bindlessBindGroup) BindGroupRelease(bindlessBindGroup);
        if (bindlessLayout) BindGroupLayoutRelease(bindlessLayout);
        if (pushLayout) BindGroupLayoutRelease(pushLayout);
        if (dummyView) TextureViewRelease(dummyView);
        if (dummyTexture) TextureRelease(dummyTexture);
        if (dummySampler) SamplerRelease(dummySampler);
        if (depthView) TextureViewRelease(depthView);
        if (depthTexture) TextureRelease(depthTexture);
        if (swapchainView) TextureViewRelease(swapchainView);
        if (swapchainTexture) TextureRelease(swapchainTexture);
        if (surface) SurfaceRelease(surface);
        if (adapter) AdapterRelease(adapter);
        if (queue) QueueRelease(queue);
        if (device) DeviceRelease(device);
        if (instance) InstanceRelease(instance);
        delete mainRenderPass;
        delete overlayRenderPass;
#if !defined(__EMSCRIPTEN__)
        if (lib) FreeLibrary(lib);
#endif
    }

    void CreateDummyTexture() {
        WGPUTextureDescriptor td{};
        td.label = WgpuStr("dummy");
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        td.dimension = WGPUTextureDimension_2D;
        td.size = { 1, 1, 1 };
        td.format = WGPUTextureFormat_RGBA8Unorm;
        td.mipLevelCount = 1;
        td.sampleCount = 1;
        dummyTexture = DeviceCreateTexture(device, &td);

        WGPUTextureViewDescriptor vd{};
        vd.label = WgpuStr("dummy view");
        vd.format = WGPUTextureFormat_RGBA8Unorm;
        vd.dimension = WGPUTextureViewDimension_2D;
        vd.baseMipLevel = 0;
        vd.mipLevelCount = 1;
        vd.baseArrayLayer = 0;
        vd.arrayLayerCount = 1;
        vd.aspect = WGPUTextureAspect_All;
        dummyView = TextureCreateView(dummyTexture, &vd);

        WGPUSamplerDescriptor sd{};
        sd.label = WgpuStr("dummy sampler");
        sd.addressModeU = WGPUAddressMode_ClampToEdge;
        sd.addressModeV = WGPUAddressMode_ClampToEdge;
        sd.addressModeW = WGPUAddressMode_ClampToEdge;
        sd.magFilter = WGPUFilterMode_Linear;
        sd.minFilter = WGPUFilterMode_Linear;
        sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sd.lodMinClamp = 0.0f;
        sd.lodMaxClamp = 32.0f;
        sd.maxAnisotropy = 1;  // 0 is invalid in wgpu v29 validation
        dummySampler = DeviceCreateSampler(device, &sd);

        // Fill it with white (RGBA8 4 bytes) via a buffer->texture copy write.
        const uint32_t white = 0xFFFFFFFFu;
        WGPUTexelCopyTextureInfo dstInfo{};
        dstInfo.texture = dummyTexture;
        dstInfo.mipLevel = 0;
        dstInfo.origin = { 0, 0, 0 };
        dstInfo.aspect = WGPUTextureAspect_All;
        WGPUTexelCopyBufferLayout srcLayout{};
        srcLayout.offset = 0;
        srcLayout.bytesPerRow = 256; // WebGPU buffer-texture pitch alignment
        srcLayout.rowsPerImage = 1;
        WGPUExtent3D extent{ 1, 1, 1 };
        QueueWriteTexture(queue, &dstInfo, &white, sizeof(white), &srcLayout, &extent);
    }

    void ConfigureSwapchain(int w, int h) {
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        // Pick the surface format: prefer BGRA8 sRGB, else the first supported.
        WGPUSurfaceCapabilities capsSc{};
        if (SurfaceGetCapabilities(surface, adapter, &capsSc) == WGPUStatus_Success &&
            capsSc.formatCount > 0) {
            swapchainFormat = capsSc.formats[0];
            for (size_t i = 0; i < capsSc.formatCount; ++i) {
                if (capsSc.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb) {
                    swapchainFormat = capsSc.formats[i];
                    break;
                }
            }
        } else {
            swapchainFormat = WGPUTextureFormat_BGRA8UnormSrgb;
        }

        WGPUSurfaceConfiguration sc{};
        sc.device = device;
        sc.format = swapchainFormat;
        sc.usage = WGPUTextureUsage_RenderAttachment;
        sc.width = (uint32_t)w;
        sc.height = (uint32_t)h;
        sc.alphaMode = WGPUCompositeAlphaMode_Auto;
#if defined(__EMSCRIPTEN__)
        sc.presentMode = WGPUPresentMode_Fifo; // emdawn glue only accepts Fifo/Undefined
#else
        sc.presentMode = vsync ? WGPUPresentMode_Fifo : WGPUPresentMode_Immediate;
#endif
        SurfaceConfigure(surface, &sc);

        width = w;
        height = h;
    }

    void RecreateSwapchainInternal() {
        WaitIdle();
        if (swapchainView) { TextureViewRelease(swapchainView); swapchainView = nullptr; }
        if (swapchainTexture) { TextureRelease(swapchainTexture); swapchainTexture = nullptr; }

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW < 1) fbW = 1;
        if (fbH < 1) fbH = 1;
        ConfigureSwapchain(fbW, fbH);
    }

    void EnsureSwapchainDepth(uint32_t w, uint32_t h) {
        if (depthView && depthW == w && depthH == h) return;
        if (depthView) { TextureViewRelease(depthView); depthView = nullptr; }
        if (depthTexture) { TextureRelease(depthTexture); depthTexture = nullptr; }
        depthW = w;
        depthH = h;

        WGPUTextureDescriptor td{};
        td.label = WgpuStr("swapchain depth");
        td.usage = WGPUTextureUsage_RenderAttachment;
        td.dimension = WGPUTextureDimension_2D;
        td.size = { w, h, 1 };
        td.format = WGPUTextureFormat_Depth32Float;
        td.mipLevelCount = 1;
        td.sampleCount = 1;
        depthTexture = DeviceCreateTexture(device, &td);

        WGPUTextureViewDescriptor vd{};
        vd.label = WgpuStr("swapchain depth view");
        vd.format = WGPUTextureFormat_Depth32Float;
        vd.dimension = WGPUTextureViewDimension_2D;
        vd.baseMipLevel = 0;
        vd.mipLevelCount = 1;
        vd.baseArrayLayer = 0;
        vd.arrayLayerCount = 1;
        vd.aspect = WGPUTextureAspect_DepthOnly;
        depthView = TextureCreateView(depthTexture, &vd);
    }

    void RebuildBindlessBindGroup() {
        if (bindlessBindGroup) { BindGroupRelease(bindlessBindGroup); bindlessBindGroup = nullptr; }

        WGPUBindGroupEntry entries[2]{};
        entries[0].binding = 0;
        entries[1].binding = 1;

#if defined(__EMSCRIPTEN__)
        // Browser builds cannot use binding_array (naga's wgpu_binding_array
        // enable is native-only), so the shared group is only the default
        // (dummy) fallback. Real textures are bound per-draw: the executor
        // overrides the bindless slot with GetTextureBindGroup(index).
        entries[0].textureView = dummyView;
        entries[1].sampler = dummySampler;
#else
        // One entry per layout binding; the entry extras carry the whole
        // binding_array (N texture views / N samplers).
        std::vector<WGPUTextureView> views(kBindlessMax);
        std::vector<WGPUSampler> samplers(kBindlessMax);
        for (uint32_t i = 0; i < kBindlessMax; ++i) {
            auto vit = bindlessViews.find(i);
            auto sit = bindlessSamplers.find(i);
            views[i] = (vit != bindlessViews.end() && vit->second)
                ? vit->second->view : dummyView;
            samplers[i] = (sit != bindlessSamplers.end() && sit->second)
                ? sit->second->sampler : dummySampler;
        }

        WGPUBindGroupEntryExtras texExtras{};
        texExtras.chain.sType = static_cast<WGPUSType>(WGPUSType_BindGroupEntryExtras);
        texExtras.textureViews = views.data();
        texExtras.textureViewCount = views.size();
        WGPUBindGroupEntryExtras sampExtras{};
        sampExtras.chain.sType = static_cast<WGPUSType>(WGPUSType_BindGroupEntryExtras);
        sampExtras.samplers = samplers.data();
        sampExtras.samplerCount = samplers.size();

        entries[0].nextInChain = &texExtras.chain;
        entries[1].nextInChain = &sampExtras.chain;
#endif

        WGPUBindGroupDescriptor bg{};
        bg.label = WgpuStr("bindless");
        bg.layout = bindlessLayout;
        bg.entryCount = 2;
        bg.entries = entries;
        bindlessBindGroup = DeviceCreateBindGroup(device, &bg);
        if (bindlessSetRec) bindlessSetRec->bindGroup = bindlessBindGroup;
    }

#if defined(__EMSCRIPTEN__)
    // Drop a cached per-texture bind group so the next draw recreates it from
    // the current bindlessViews/bindlessSamplers (used on register/update/
    // unregister and during render-target masking).
    void InvalidateTextureBindGroup(uint32_t index) {
        auto it = textureBindGroups.find(index);
        if (it != textureBindGroups.end()) {
            BindGroupRelease(it->second);
            textureBindGroups.erase(it);
        }
    }

    // Lazily-built bind group (same bindlessLayout: binding 0 = texture,
    // binding 1 = sampler) for one registered texture. Falls back to the dummy
    // when the index is unregistered or currently masked as an attachment.
    WGPUBindGroup GetTextureBindGroup(uint32_t index) {
        auto it = textureBindGroups.find(index);
        if (it != textureBindGroups.end()) return it->second;

        WGPUTextureView view = dummyView;
        WGPUSampler sampler = dummySampler;
        auto vit = bindlessViews.find(index);
        auto sit = bindlessSamplers.find(index);
        if (vit != bindlessViews.end() && vit->second && vit->second->view)
            view = vit->second->view;
        if (sit != bindlessSamplers.end() && sit->second && sit->second->sampler)
            sampler = sit->second->sampler;

        WGPUBindGroupEntry entries[2]{};
        entries[0].binding = 0;
        entries[0].textureView = view;
        entries[1].binding = 1;
        entries[1].sampler = sampler;

        WGPUBindGroupDescriptor bg{};
        bg.label = WgpuStr("tex");
        bg.layout = bindlessLayout;
        bg.entryCount = 2;
        bg.entries = entries;
        WGPUBindGroup group = DeviceCreateBindGroup(device, &bg);
        if (group) textureBindGroups[index] = group;
        return group;
    }
#endif

    void EndCurrentPass() {
        if (!currentPass) return;
        RenderPassEncoderEnd(currentPass);
        RenderPassEncoderRelease(currentPass);
        currentPass = nullptr;
    }

    void WaitIdle() {
        if (!device) return;
#if defined(__EMSCRIPTEN__)
        // wgpuDevicePoll is a wgpu-native extension unavailable in the browser;
        // the browser's command queue is drained internally (best-effort no-op).
        (void)0;
#else
        // wgpuDevicePoll is still exported by wgpu-native v29 (extension);
        // wait=true blocks until all queued GPU work completes.
        DevicePoll(device, true, nullptr);
#endif
    }

    // wgpuQueueWriteBuffer in safe chunks (queue order guarantees the writes
    // land before the current frame's submit).
    void QueueWriteChunked(WGPUBuffer buffer, uint32_t offset, const void* data, uint32_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        uint32_t done = 0;
        while (done < size) {
            uint32_t chunk = std::min(kQueueWriteChunk, size - done);
            QueueWriteBuffer(queue, buffer, (uint64_t)offset + done, bytes + done, chunk);
            done += chunk;
        }
    }
};

WebGPUBackend::WebGPUBackend(void* window, int width, int height, bool vsync,
                             const std::string& appName)
    : m_Impl(std::make_unique<Impl>(window, width, height, vsync, appName))
{
}

WebGPUBackend::~WebGPUBackend() = default;

// ---- Frame lifecycle ----

bool WebGPUBackend::BeginFrame(bool skipRenderPass) {
    Impl& im = *m_Impl;

    if (im.resized) {
        im.resized = false;
        try { im.RecreateSwapchainInternal(); }
        catch (const std::exception&) { im.width = 1; im.height = 1; }
    }

    // Acquire the next surface texture.
    WGPUSurfaceTexture st{};
    im.SurfaceGetCurrentTexture(im.surface, &st);
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return false;
    }
    im.swapchainTexture = st.texture;

    WGPUTextureViewDescriptor vd{};
    vd.label = WgpuStr("swapchain view");
    vd.format = im.swapchainFormat;
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = 1;
    vd.baseArrayLayer = 0;
    vd.arrayLayerCount = 1;
    vd.aspect = WGPUTextureAspect_All;
    im.swapchainView = im.TextureCreateView(im.swapchainTexture, &vd);

    WGPUCommandEncoderDescriptor ed{};
    ed.label = WgpuStr("frame");
    im.encoder = im.DeviceCreateCommandEncoder(im.device, &ed);

    im.swapchainWasWritten = false;
    im.passW = (uint32_t)im.width;
    im.passH = (uint32_t)im.height;

    if (!skipRenderPass) {
        // Main 3D render pass on the swapchain (PhysicsDemo). Clear matches the
        // Vulkan main pass {0.15, 0.15, 0.2, 1.0} + depth 1.0. Positive-height
        // viewport (D3D12/WebGPU convention — NO negative flip).
        im.EnsureSwapchainDepth((uint32_t)im.width, (uint32_t)im.height);

        WGPURenderPassColorAttachment color{};
        color.view = im.swapchainView;
        color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color.loadOp = WGPULoadOp_Clear;
        color.storeOp = WGPUStoreOp_Store;
        color.clearValue = { 0.15, 0.15, 0.2, 1.0 };

        WGPURenderPassDepthStencilAttachment depth{};
        depth.view = im.depthView;
        depth.depthLoadOp = WGPULoadOp_Clear;
        depth.depthStoreOp = WGPUStoreOp_Store;
        depth.depthClearValue = 1.0f;

        WGPURenderPassDescriptor rpd{};
        rpd.label = WgpuStr("main");
        rpd.colorAttachmentCount = 1;
        rpd.colorAttachments = &color;
        rpd.depthStencilAttachment = &depth;
        im.currentPass = im.CommandEncoderBeginRenderPass(im.encoder, &rpd);

        im.RenderPassEncoderSetViewport(im.currentPass, 0.0f, 0.0f,
            (float)im.width, (float)im.height, 0.0f, 1.0f);
        im.RenderPassEncoderSetScissorRect(im.currentPass, 0, 0,
            (uint32_t)im.width, (uint32_t)im.height);
        im.swapchainWasWritten = true;
    }

    return true;
}

void WebGPUBackend::BeginSwapchainOverlay() {
    Impl& im = *m_Impl;
    // End the main pass if BeginFrame(false) started one.
    im.EndCurrentPass();
    im.passW = (uint32_t)im.width;
    im.passH = (uint32_t)im.height;

    WGPURenderPassColorAttachment color{};
    color.view = im.swapchainView;
    color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color.loadOp = im.swapchainWasWritten ? WGPULoadOp_Load : WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = { 0.12, 0.12, 0.15, 1.0 };

    WGPURenderPassDescriptor rpd{};
    rpd.label = WgpuStr("overlay");
    rpd.colorAttachmentCount = 1;
    rpd.colorAttachments = &color;
    im.currentPass = im.CommandEncoderBeginRenderPass(im.encoder, &rpd);

    im.RenderPassEncoderSetViewport(im.currentPass, 0.0f, 0.0f,
        (float)im.width, (float)im.height, 0.0f, 1.0f);
    im.RenderPassEncoderSetScissorRect(im.currentPass, 0, 0,
        (uint32_t)im.width, (uint32_t)im.height);
}

void WebGPUBackend::EndFrame() {
    Impl& im = *m_Impl;
    if (!im.encoder) return;

    im.EndCurrentPass();

    WGPUCommandBufferDescriptor cbd{};
    cbd.label = WgpuStr("frame");
    WGPUCommandBuffer cb = im.CommandEncoderFinish(im.encoder, &cbd);
    im.CommandEncoderRelease(im.encoder);
    im.encoder = nullptr;

    if (cb) {
        im.QueueSubmit(im.queue, 1, &cb);
        im.CommandBufferRelease(cb);
#if !defined(__EMSCRIPTEN__)
        im.SurfacePresent(im.surface);
#endif
    }

    if (im.swapchainView) { im.TextureViewRelease(im.swapchainView); im.swapchainView = nullptr; }
    if (im.swapchainTexture) { im.TextureRelease(im.swapchainTexture); im.swapchainTexture = nullptr; }
}

void WebGPUBackend::WaitIdle() { m_Impl->WaitIdle(); }

const GCaps& WebGPUBackend::GetCaps() const { return m_Impl->caps; }

RHICommandBuffer WebGPUBackend::GetCurrentCommandBuffer() const {
    RHICommandBuffer cb;
    cb.handle = reinterpret_cast<uint64_t>(m_Impl->encoder);
    return cb;
}
uint32_t WebGPUBackend::GetCurrentFrameIndex() const { return 0; }
uint32_t WebGPUBackend::GetSwapchainWidth() const { return (uint32_t)m_Impl->width; }
uint32_t WebGPUBackend::GetSwapchainHeight() const { return (uint32_t)m_Impl->height; }

RHIRenderPass WebGPUBackend::GetRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->mainRenderPass);
    return rp;
}
RHIRenderPass WebGPUBackend::GetOverlayRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->overlayRenderPass);
    return rp;
}

bool WebGPUBackend::WasResized() const { return m_Impl->resized; }
void WebGPUBackend::ResetResized() { m_Impl->resized = false; }
void WebGPUBackend::NotifyResized() { m_Impl->resized = true; }
void WebGPUBackend::RecreateSwapchain() { m_Impl->RecreateSwapchainInternal(); }

// ---- Resource creation ----

RHIShaderModule WebGPUBackend::CreateShaderModule(const std::vector<char>& code) {
    Impl& im = *m_Impl;
    ShaderRec* rec = new ShaderRec();

#if defined(__EMSCRIPTEN__)
    // Browser WGSL (Firefox naga) cannot compile binding_array at all — the
    // wgpu_binding_array enable it requires is native-only. Web builds must use
    // the *.web.wgsl variants (single texture/sampler), so no enable is
    // prepended here.
    WGPUShaderSourceWGSL wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code.data = code.data();
    wgsl.code.length = code.size();
#else
    WGPUShaderSourceWGSL wgsl{};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code.data = code.data();
    wgsl.code.length = code.size();
#endif

    WGPUShaderModuleDescriptor smd{};
    smd.label = WgpuStr("shader");
    smd.nextInChain = &wgsl.chain;
    rec->module = im.DeviceCreateShaderModule(im.device, &smd);
    if (!rec->module) {
        XConsole::PrintError("WebGPU: shader module compilation failed");
        delete rec;
        return RHIShaderModule{};
    }

    RHIShaderModule out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyShaderModule(RHIShaderModule module) {
    Impl& im = *m_Impl;
    ShaderRec* rec = reinterpret_cast<ShaderRec*>(module.handle);
    if (rec->module) im.ShaderModuleRelease(rec->module);
    delete rec;
}

RHIPipeline WebGPUBackend::CreateGraphicsPipeline(const RHIPipelineDesc& desc) {
    Impl& im = *m_Impl;
    PipelineLayoutRec* layout = reinterpret_cast<PipelineLayoutRec*>(desc.layout.handle);
    RenderPassRec* rp = reinterpret_cast<RenderPassRec*>(desc.renderPass.handle);
    if (!layout || !rp) return RHIPipeline{};

    PipelineRec* rec = new PipelineRec();
    rec->layout = layout;

    WGPURenderPipelineDescriptor pdesc{};
    pdesc.label = WgpuStr("pipeline");
    pdesc.layout = layout->wgpuLayout;

    // v29 declares `fragment` as `WGPUFragmentState const*` — keep a mutable
    // pointer and mutate through that, then hand the const pointer to wgpu.
    WGPUFragmentState* fs = nullptr;
    for (const auto& s : desc.stages) {
        ShaderRec* sr = reinterpret_cast<ShaderRec*>(s.module.handle);
        if (!sr || !sr->module) continue;
        if (s.stage == ShaderStage::Vertex) {
            pdesc.vertex.module = sr->module;
            pdesc.vertex.entryPoint = WgpuStr("vs_main");
        } else if (s.stage == ShaderStage::Fragment) {
            fs = new WGPUFragmentState{};
            fs->module = sr->module;
            fs->entryPoint = WgpuStr("ps_main");
            pdesc.fragment = fs;
        }
    }

    // Vertex input
    WGPUVertexAttribute attrs[8]{};
    WGPUVertexBufferLayout vbLayout{};
    if (desc.vertexBinding.stride > 0 && !desc.vertexAttributes.empty()) {
        uint32_t n = (uint32_t)std::min<size_t>(desc.vertexAttributes.size(), 8);
        for (uint32_t i = 0; i < n; ++i) {
            attrs[i].format = ToVertexFormat(desc.vertexAttributes[i].format);
            attrs[i].offset = desc.vertexAttributes[i].offset;
            attrs[i].shaderLocation = desc.vertexAttributes[i].location;
        }
        vbLayout.arrayStride = desc.vertexBinding.stride;
        vbLayout.stepMode = desc.vertexBinding.inputRate == VertexInputRate::Instance
            ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
        vbLayout.attributeCount = n;
        vbLayout.attributes = attrs;
        pdesc.vertex.bufferCount = 1;
        pdesc.vertex.buffers = &vbLayout;
    }

    // Primitive state
    pdesc.primitive.topology = desc.topology == Topology::TriangleStrip
        ? WGPUPrimitiveTopology_TriangleStrip : WGPUPrimitiveTopology_TriangleList;
    if (desc.topology == Topology::TriangleStrip)
        pdesc.primitive.stripIndexFormat = WGPUIndexFormat_Uint32;
    pdesc.primitive.frontFace = WGPUFrontFace_CCW;
    pdesc.primitive.cullMode = desc.cullMode == CullMode::None ? WGPUCullMode_None
        : WGPUCullMode_Back;
    pdesc.primitive.unclippedDepth = 0;

    // Depth-stencil (only when the render pass has a depth attachment)
    WGPUDepthStencilState ds{};
    if (rp->hasDepth && desc.depthTestEnable) {
        ds.format = rp->depthFormat;
        ds.depthWriteEnabled = desc.depthWriteEnable ? WGPUOptionalBool_True : WGPUOptionalBool_False;
        ds.depthCompare = WGPUCompareFunction_Less;
        ds.stencilReadMask = 0xFFFFFFFFu;
        ds.stencilWriteMask = 0xFFFFFFFFu;
        pdesc.depthStencil = &ds;
    }

    // Multisample
    pdesc.multisample.count = 1;
    pdesc.multisample.mask = 0xFFFFFFFFu;
    pdesc.multisample.alphaToCoverageEnabled = 0;

    // Color targets (from the render pass formats)
    WGPUColorTargetState targets[kMaxColorAttachments]{};
    WGPUBlendState blend{};
    size_t targetCount = std::min<size_t>(rp->colorFormats.size(), kMaxColorAttachments);
    for (size_t i = 0; i < targetCount; ++i) {
        targets[i].format = rp->colorFormats[i];
        targets[i].writeMask = WGPUColorWriteMask_All;
        if (desc.blend.enable && i == 0) {
            blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
            blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            blend.color.operation = WGPUBlendOperation_Add;
            blend.alpha.srcFactor = WGPUBlendFactor_One;
            blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            blend.alpha.operation = WGPUBlendOperation_Add;
            targets[i].blend = &blend;
        }
    }
    if (pdesc.fragment) {
        fs->targetCount = targetCount;
        fs->targets = targets;
    }

    rec->pipeline = im.DeviceCreateRenderPipeline(im.device, &pdesc);
    delete pdesc.fragment;
    if (!rec->pipeline) {
        XConsole::PrintError("WebGPU: failed to create graphics pipeline");
        delete rec;
        return RHIPipeline{};
    }

    RHIPipeline out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyPipeline(RHIPipeline pipeline) {
    Impl& im = *m_Impl;
    PipelineRec* rec = reinterpret_cast<PipelineRec*>(pipeline.handle);
    if (rec->pipeline) im.RenderPipelineRelease(rec->pipeline);
    delete rec;
}

RHIPipelineLayout WebGPUBackend::CreatePipelineLayout(
    const std::vector<RHIDescriptorSetLayout>& layouts,
    const std::vector<RHIPushConstantRange>& pushConstants) {
    Impl& im = *m_Impl;
    PipelineLayoutRec* rec = new PipelineLayoutRec();

    std::vector<WGPUBindGroupLayout> bgls;
    bgls.reserve(layouts.size());
    for (const auto& l : layouts) {
        DescSetLayoutRec* lr = reinterpret_cast<DescSetLayoutRec*>(l.handle);
        if (!lr) continue;
        rec->setLayouts.push_back(lr);
        bgls.push_back(lr->wgpuLayout);
    }

    // Push constants -> a per-layout UBO at group index = setLayouts.size().
    for (const auto& pc : pushConstants) {
        if (pc.size > 0) {
            rec->hasPush = true;
            rec->pushGroup = (uint32_t)bgls.size();
            rec->pushSize = pc.offset + pc.size;
            bgls.push_back(im.pushLayout);
            break;
        }
    }

    WGPUPipelineLayoutDescriptor pld{};
    pld.label = WgpuStr("pipeline layout");
    pld.bindGroupLayoutCount = bgls.size();
    pld.bindGroupLayouts = bgls.data();
    rec->wgpuLayout = im.DeviceCreatePipelineLayout(im.device, &pld);
    if (!rec->wgpuLayout) {
        XConsole::PrintError("WebGPU: failed to create pipeline layout");
        delete rec;
        return RHIPipelineLayout{};
    }

    RHIPipelineLayout out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyPipelineLayout(RHIPipelineLayout layout) {
    Impl& im = *m_Impl;
    PipelineLayoutRec* rec = reinterpret_cast<PipelineLayoutRec*>(layout.handle);
    if (rec->wgpuLayout) im.PipelineLayoutRelease(rec->wgpuLayout);
    for (WGPUBuffer b : rec->pushBuffers) im.BufferRelease(b);
    for (WGPUBindGroup g : rec->pushBindGroups) im.BindGroupRelease(g);
    delete rec;
}

RHIDescriptorSetLayout WebGPUBackend::CreateDescriptorSetLayout(
    const std::vector<RHIDescriptorBinding>& bindings) {
    Impl& im = *m_Impl;
    DescSetLayoutRec* rec = new DescSetLayoutRec();

    bool bindless = false;
    if (!bindings.empty()) {
        rec->type = bindings[0].type;
        rec->stage = bindings[0].stage;
        for (const auto& b : bindings)
            if (b.bindless) bindless = true;
    }

    if (bindless) {
        // All bindless sets share the one cached WGPUBindGroupLayout object.
        rec->bindless = true;
        rec->wgpuLayout = im.bindlessLayout;
        rec->ownsLayout = false;
    } else {
        // CombinedImageSampler -> two layout entries (texture binding 0 +
        // sampler binding 1); UniformBuffer -> one buffer entry.
        std::vector<WGPUBindGroupLayoutEntry> entries;
        if (rec->type == DescriptorType::CombinedImageSampler) {
            WGPUBindGroupLayoutEntry e0{};
            e0.binding = 0;
            e0.visibility = rec->stage == ShaderStage::Vertex
                ? WGPUShaderStage_Vertex : WGPUShaderStage_Fragment;
            e0.texture.sampleType = WGPUTextureSampleType_Float;
            e0.texture.viewDimension = WGPUTextureViewDimension_2D;
            entries.push_back(e0);

            WGPUBindGroupLayoutEntry e1{};
            e1.binding = 1;
            e1.visibility = e0.visibility;
            e1.sampler.type = WGPUSamplerBindingType_Filtering;
            entries.push_back(e1);
        } else {
            WGPUBindGroupLayoutEntry e0{};
            e0.binding = 0;
            e0.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
            e0.buffer.type = WGPUBufferBindingType_Uniform;
            entries.push_back(e0);
        }

        WGPUBindGroupLayoutDescriptor bgl{};
        bgl.label = WgpuStr("set");
        bgl.entryCount = entries.size();
        bgl.entries = entries.data();
        rec->wgpuLayout = im.DeviceCreateBindGroupLayout(im.device, &bgl);
        rec->ownsLayout = rec->wgpuLayout != nullptr;
    }

    RHIDescriptorSetLayout out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyDescriptorSetLayout(RHIDescriptorSetLayout layout) {
    Impl& im = *m_Impl;
    DescSetLayoutRec* rec = reinterpret_cast<DescSetLayoutRec*>(layout.handle);
    if (rec->ownsLayout && rec->wgpuLayout) im.BindGroupLayoutRelease(rec->wgpuLayout);
    delete rec;
}

RHIDescriptorPool WebGPUBackend::CreateDescriptorPool(
    const std::vector<RHIDescriptorBinding>& poolBindings, uint32_t maxSets) {
    (void)poolBindings; (void)maxSets;
    DescPoolRec* rec = new DescPoolRec();
    RHIDescriptorPool out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyDescriptorPool(RHIDescriptorPool pool) {
    delete reinterpret_cast<DescPoolRec*>(pool.handle);
}

RHIDescriptorSet WebGPUBackend::AllocateDescriptorSet(
    RHIDescriptorPool pool, RHIDescriptorSetLayout layout) {
    (void)pool;
    Impl& im = *m_Impl;
    DescSetLayoutRec* lr = reinterpret_cast<DescSetLayoutRec*>(layout.handle);
    DescSetRec* rec = new DescSetRec();
    rec->layout = lr ? lr->wgpuLayout : nullptr;

    if (lr && lr->bindless) {
        rec->isBindless = true;
        if (!im.bindlessSetRec) {
            im.bindlessSetRec = new DescSetRec();
            im.bindlessSetRec->isBindless = true;
            im.bindlessSetRec->layout = im.bindlessLayout;
            im.bindlessSetRec->bindGroup = im.bindlessBindGroup;
        }
        // Lightweight wrapper referencing the shared table.
        rec->bindGroup = im.bindlessSetRec->bindGroup;
        rec->ownsBindGroup = false;
    } else if (lr && lr->type == DescriptorType::UniformBuffer) {
        rec->isUbo = true;
        rec->ownsBindGroup = false;
    } else {
        rec->ownsBindGroup = false;
    }

    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}

void WebGPUBackend::WriteDescriptorSets(const std::vector<RHIDescriptorWrite>& writes) {
    Impl& im = *m_Impl;
    for (const auto& w : writes) {
        DescSetRec* set = reinterpret_cast<DescSetRec*>(w.dstSet.handle);
        if (!set || set->isBindless) continue;

        WGPUBindGroupDescriptor bg{};
        bg.label = WgpuStr("set");
        bg.layout = set->layout;
        WGPUBindGroupEntry entries[2]{};

        if (w.type == DescriptorType::CombinedImageSampler && w.imageInfo.valid) {
            ImageViewRec* view = reinterpret_cast<ImageViewRec*>(w.imageInfo.imageView.handle);
            SamplerRec* samp = reinterpret_cast<SamplerRec*>(w.imageInfo.sampler.handle);
            if (!view || !view->view || !samp || !samp->sampler) continue;

            entries[0].binding = 0;
            entries[0].textureView = view->view;
            entries[1].binding = 1;
            entries[1].sampler = samp->sampler;
            bg.entryCount = 2;
            bg.entries = entries;
        } else if (w.type == DescriptorType::UniformBuffer && w.bufferInfo.valid) {
            BufferRec* buf = reinterpret_cast<BufferRec*>(w.bufferInfo.buffer.handle);
            if (!buf || !buf->buffer) continue;
            set->isUbo = true;

            entries[0].binding = 0;
            entries[0].buffer = buf->buffer;
            entries[0].offset = w.bufferInfo.offset;
            entries[0].size = WGPU_WHOLE_SIZE;
            bg.entryCount = 1;
            bg.entries = entries;
        } else {
            continue;
        }

        if (set->ownsBindGroup && set->bindGroup)
            im.BindGroupRelease(set->bindGroup);
        set->bindGroup = im.DeviceCreateBindGroup(im.device, &bg);
        set->ownsBindGroup = set->bindGroup != nullptr;
    }
}

RHIBuffer WebGPUBackend::CreateBuffer(uint32_t size, BufferUsage usage,
    MemoryProperty properties, RHIDeviceMemory& memory) {
    Impl& im = *m_Impl;
    bool hostVisible = (static_cast<uint8_t>(properties) &
        static_cast<uint8_t>(MemoryProperty::HostVisible)) != 0;

    // WebGPU uniform buffers need a size multiple of 16 (for the bind group
    // binding size). Align the allocation; the engine writes <= size bytes.
    uint32_t realSize = size;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(BufferUsage::Uniform))
        realSize = (realSize + 15u) & ~15u;

    BufferRec* rec = new BufferRec();
    rec->size = realSize;
    rec->hostVisible = hostVisible;
    if (hostVisible)
        rec->shadow.assign(realSize, 0);

    WGPUBufferDescriptor bd{};
    bd.label = WgpuStr("buffer");
    bd.usage = WgpuUsage(usage, hostVisible);
    bd.size = realSize;
    bd.mappedAtCreation = 0;
    rec->buffer = im.DeviceCreateBuffer(im.device, &bd);
    if (!rec->buffer) {
        XConsole::PrintError("WebGPU: failed to create buffer ({} bytes)", realSize);
        delete rec;
        return RHIBuffer{};
    }

    MemoryRec* mem = new MemoryRec();
    mem->resource = rec;
    mem->isBuffer = true;

    RHIBuffer out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    RHIDeviceMemory outMem;
    outMem.handle = reinterpret_cast<uint64_t>(mem);
    memory = outMem;
    return out;
}

void WebGPUBackend::DestroyBuffer(RHIBuffer buffer) {
    Impl& im = *m_Impl;
    BufferRec* rec = reinterpret_cast<BufferRec*>(buffer.handle);
    if (!rec) return;
    if (--rec->refs == 0) {
        im.BufferRelease(rec->buffer);
        delete rec;
    }
}

void WebGPUBackend::DestroyMemory(RHIDeviceMemory memory) {
    Impl& im = *m_Impl;
    MemoryRec* mem = reinterpret_cast<MemoryRec*>(memory.handle);
    if (!mem) return;
    if (mem->isBuffer) {
        BufferRec* rec = static_cast<BufferRec*>(mem->resource);
        if (rec && --rec->refs == 0) {
            im.BufferRelease(rec->buffer);
            delete rec;
        }
    } else {
        ImageRec* rec = static_cast<ImageRec*>(mem->resource);
        if (rec && --rec->refs == 0) {
            im.TextureRelease(rec->texture);
            delete rec;
        }
    }
    delete mem;
}

void WebGPUBackend::CopyBuffer(RHIBuffer src, RHIBuffer dst, uint32_t size) {
    Impl& im = *m_Impl;
    BufferRec* srcRec = reinterpret_cast<BufferRec*>(src.handle);
    BufferRec* dstRec = reinterpret_cast<BufferRec*>(dst.handle);
    if (!srcRec || !dstRec) return;

    // Host-visible staging buffers carry their data in the shadow copy: write
    // straight into the destination (no intermediate GPU copy).
    if (srcRec->hostVisible && !srcRec->shadow.empty()) {
        im.QueueWriteChunked(dstRec->buffer, 0,
            srcRec->shadow.data(), std::min(size, srcRec->size));
        return;
    }

    WGPUCommandEncoder enc = im.encoder;
    bool oneShot = enc == nullptr;
    if (oneShot) {
        WGPUCommandEncoderDescriptor ed{};
        ed.label = WgpuStr("copy");
        enc = im.DeviceCreateCommandEncoder(im.device, &ed);
    }
    im.CommandEncoderCopyBufferToBuffer(enc, srcRec->buffer, 0, dstRec->buffer, 0, size);
    if (oneShot) {
        WGPUCommandBufferDescriptor cbd{};
        cbd.label = WgpuStr("copy");
        WGPUCommandBuffer cb = im.CommandEncoderFinish(enc, &cbd);
        im.CommandEncoderRelease(enc);
        im.QueueSubmit(im.queue, 1, &cb);
        im.CommandBufferRelease(cb);
    }
}

bool WebGPUBackend::MapMemory(RHIDeviceMemory memory, uint32_t offset,
    uint32_t size, void** data) {
    MemoryRec* mem = reinterpret_cast<MemoryRec*>(memory.handle);
    if (!mem || !mem->isBuffer) return false;
    BufferRec* rec = static_cast<BufferRec*>(mem->resource);
    if (!rec || !rec->hostVisible || !data) return false;
    if (offset + size > rec->size) return false;
    *data = rec->shadow.data() + offset;
    return true;
}

void WebGPUBackend::UnmapMemory(RHIDeviceMemory memory) {
    MemoryRec* mem = reinterpret_cast<MemoryRec*>(memory.handle);
    if (!mem || !mem->isBuffer) return;
    BufferRec* rec = static_cast<BufferRec*>(mem->resource);
    if (!rec || !rec->hostVisible || rec->shadow.empty()) return;
    Impl& im = *m_Impl;
    im.QueueWriteChunked(rec->buffer, 0,
        rec->shadow.data(), rec->size);
}

RHIImage WebGPUBackend::CreateImage(uint32_t width, uint32_t height, Format format,
    ImageUsage usage, MemoryProperty properties, RHIDeviceMemory& memory) {
    Impl& im = *m_Impl;
    (void)properties; // WebGPU images are device-local; uploads go via copy.

    ImageRec* rec = new ImageRec();
    rec->width = width;
    rec->height = height;

    WGPUTextureDescriptor td{};
    td.label = WgpuStr("image");
    td.usage = WgpuUsage(usage);
    td.dimension = WGPUTextureDimension_2D;
    td.size = { width, height, 1 };
    td.format = ToWgpu(format);
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    rec->texture = im.DeviceCreateTexture(im.device, &td);
    if (!rec->texture) {
        XConsole::PrintError("WebGPU: failed to create texture ({}x{} format {})",
            width, height, (int)format);
        delete rec;
        return RHIImage{};
    }

    MemoryRec* mem = new MemoryRec();
    mem->resource = rec;
    mem->isBuffer = false;

    RHIImage out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    RHIDeviceMemory outMem;
    outMem.handle = reinterpret_cast<uint64_t>(mem);
    memory = outMem;
    return out;
}

void WebGPUBackend::DestroyImage(RHIImage image) {
    Impl& im = *m_Impl;
    ImageRec* rec = reinterpret_cast<ImageRec*>(image.handle);
    if (!rec) return;
    if (--rec->refs == 0) {
        im.TextureRelease(rec->texture);
        delete rec;
    }
}

RHIImageView WebGPUBackend::CreateImageView(RHIImage image, Format format, Aspect aspect) {
    Impl& im = *m_Impl;
    ImageRec* img = reinterpret_cast<ImageRec*>(image.handle);
    if (!img || !img->texture) return RHIImageView{};

    ImageViewRec* rec = new ImageViewRec();
    rec->image = img;
    rec->isDepth = (static_cast<uint8_t>(aspect) & static_cast<uint8_t>(Aspect::Depth)) != 0;

    WGPUTextureViewDescriptor vd{};
    vd.label = WgpuStr("image view");
    vd.format = ToWgpu(format);
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.baseMipLevel = 0;
    vd.mipLevelCount = 1;
    vd.baseArrayLayer = 0;
    vd.arrayLayerCount = 1;
    vd.aspect = rec->isDepth ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All;
    rec->view = im.TextureCreateView(img->texture, &vd);
    if (!rec->view) {
        XConsole::PrintError("WebGPU: failed to create image view");
        delete rec;
        return RHIImageView{};
    }

    RHIImageView out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyImageView(RHIImageView imageView) {
    Impl& im = *m_Impl;
    ImageViewRec* rec = reinterpret_cast<ImageViewRec*>(imageView.handle);
    if (!rec) return;
    if (rec->view) im.TextureViewRelease(rec->view);
    delete rec;
}

RHISampler WebGPUBackend::CreateSampler(Filter filter, SamplerAddressMode addressMode) {
    Impl& im = *m_Impl;
    SamplerRec* rec = new SamplerRec();

    WGPUSamplerDescriptor sd{};
    sd.label = WgpuStr("sampler");
    WGPUAddressMode mode = addressMode == SamplerAddressMode::Repeat
        ? WGPUAddressMode_Repeat : WGPUAddressMode_ClampToEdge;
    sd.addressModeU = mode;
    sd.addressModeV = mode;
    sd.addressModeW = mode;
    WGPUFilterMode f = filter == Filter::Nearest ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
    sd.magFilter = f;
    sd.minFilter = f;
    sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = 32.0f;
    sd.maxAnisotropy = 1;  // 0 is invalid in wgpu v29 validation
    rec->sampler = im.DeviceCreateSampler(im.device, &sd);
    rec->needsRelease = rec->sampler != nullptr;
    if (!rec->sampler) {
        XConsole::PrintError("WebGPU: failed to create sampler");
        delete rec;
        return RHISampler{};
    }

    RHISampler out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroySampler(RHISampler sampler) {
    Impl& im = *m_Impl;
    SamplerRec* rec = reinterpret_cast<SamplerRec*>(sampler.handle);
    if (!rec) return;
    if (rec->needsRelease && rec->sampler) im.SamplerRelease(rec->sampler);
    delete rec;
}

void WebGPUBackend::TransitionImageLayout(RHIImage image, Format format,
    ImageLayout oldLayout, ImageLayout newLayout) {
    // No-op: WebGPU tracks image layouts automatically and synchronizes
    // render-pass attachment usage. Kept for interface compatibility.
    (void)image; (void)format; (void)oldLayout; (void)newLayout;
}

void WebGPUBackend::CopyBufferToImage(RHIBuffer buffer, RHIImage image,
    uint32_t width, uint32_t height) {
    Impl& im = *m_Impl;
    BufferRec* src = reinterpret_cast<BufferRec*>(buffer.handle);
    ImageRec* dst = reinterpret_cast<ImageRec*>(image.handle);
    if (!src || !dst || !src->buffer || !dst->texture) return;

    WGPUTexelCopyBufferLayout lay{};
    lay.offset = 0;
    lay.bytesPerRow = (width * 4 + 255u) & ~255u; // WebGPU buffer-texture pitch
    lay.rowsPerImage = height;

    WGPUTexelCopyBufferInfo srcInfo{};
    srcInfo.layout = lay;
    srcInfo.buffer = src->buffer;

    WGPUTexelCopyTextureInfo dstInfo{};
    dstInfo.texture = dst->texture;
    dstInfo.mipLevel = 0;
    dstInfo.origin = { 0, 0, 0 };
    dstInfo.aspect = WGPUTextureAspect_All;

    WGPUExtent3D extent{ width, height, 1 };

    // Texture uploads happen at load time, outside the frame lifecycle; use a
    // one-shot encoder+submit (queue ordering keeps it before the frame's own
    // submit). Inside a frame the frame's encoder is reused.
    WGPUCommandEncoder enc = im.encoder;
    bool oneShot = enc == nullptr;
    if (oneShot) {
        WGPUCommandEncoderDescriptor ed{};
        ed.label = WgpuStr("copy");
        enc = im.DeviceCreateCommandEncoder(im.device, &ed);
    }
    im.CommandEncoderCopyBufferToTexture(enc, &srcInfo, &dstInfo, &extent);
    if (oneShot) {
        WGPUCommandBufferDescriptor cbd{};
        cbd.label = WgpuStr("copy");
        WGPUCommandBuffer cb = im.CommandEncoderFinish(enc, &cbd);
        im.CommandEncoderRelease(enc);
        im.QueueSubmit(im.queue, 1, &cb);
        im.CommandBufferRelease(cb);
    }
}

RHIRenderPass WebGPUBackend::CreateRenderPass(const std::vector<Format>& colorFormats,
    Format depthFormat, bool overlay) {
    RenderPassRec* rec = new RenderPassRec();
    rec->overlay = overlay;
    for (Format f : colorFormats) rec->colorFormats.push_back(ToWgpu(f));
    if (depthFormat == Format::D32_SFLOAT) {
        rec->hasDepth = true;
        rec->depthFormat = WGPUTextureFormat_Depth32Float;
    }

    RHIRenderPass out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyRenderPass(RHIRenderPass renderPass) {
    delete reinterpret_cast<RenderPassRec*>(renderPass.handle);
}

RHIPassTemplate WebGPUBackend::CreatePassTemplate(const RHIPassTemplateDesc& desc) {
    PassTemplateRec* rec = new PassTemplateRec();
    rec->clears = desc.clearValues;

    // D3D12/WebGPU convention: NDC is Y-up (like GLM) and the framebuffer is
    // Y-down, so the positive-height viewport maps GLM output 1:1. NO negative
    // flip (unlike Vulkan, whose NDC is Y-down).
    rec->vpX = desc.viewport.x;
    rec->vpY = desc.viewport.y;
    rec->vpW = desc.viewport.width;
    rec->vpH = desc.viewport.height;

    rec->scX = (uint32_t)desc.scissor.x;
    rec->scY = (uint32_t)desc.scissor.y;
    rec->scW = desc.scissor.width;
    rec->scH = desc.scissor.height;

    RHIPassTemplate out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyPassTemplate(RHIPassTemplate passTemplate) {
    delete reinterpret_cast<PassTemplateRec*>(passTemplate.handle);
}

// ---- Bindless ----

uint32_t WebGPUBackend::RegisterBindlessTexture(const RHIDescriptorImageInfo& info) {
    Impl& im = *m_Impl;
    if (!info.valid || !info.imageView.IsValid() || !info.sampler.IsValid())
        return 0;

    uint32_t index;
    if (!im.bindlessFree.empty()) {
        index = im.bindlessFree.back();
        im.bindlessFree.pop_back();
    } else {
        index = im.bindlessNext++;
    }
    if (index >= kBindlessMax) {
        XConsole::PrintError("WebGPU: bindless table full ({})", kBindlessMax);
        return 0;
    }

    im.bindlessViews[index] = reinterpret_cast<ImageViewRec*>(info.imageView.handle);
    im.bindlessSamplers[index] = reinterpret_cast<SamplerRec*>(info.sampler.handle);
#if defined(__EMSCRIPTEN__)
    im.InvalidateTextureBindGroup(index);
#else
    im.RebuildBindlessBindGroup();
#endif
    return index;
}

void WebGPUBackend::UpdateBindlessTexture(uint32_t index, const RHIDescriptorImageInfo& info) {
    Impl& im = *m_Impl;
    if (index >= kBindlessMax || !info.valid) return;
    im.bindlessViews[index] = reinterpret_cast<ImageViewRec*>(info.imageView.handle);
    im.bindlessSamplers[index] = reinterpret_cast<SamplerRec*>(info.sampler.handle);
#if defined(__EMSCRIPTEN__)
    im.InvalidateTextureBindGroup(index);
#else
    im.RebuildBindlessBindGroup();
#endif
}

void WebGPUBackend::UnregisterBindlessTexture(uint32_t index) {
    Impl& im = *m_Impl;
    if (index >= kBindlessMax) return;
    im.bindlessViews.erase(index);
    im.bindlessSamplers.erase(index);
    im.bindlessFree.push_back(index);
#if defined(__EMSCRIPTEN__)
    im.InvalidateTextureBindGroup(index);
#else
    im.RebuildBindlessBindGroup();
#endif
}

RHIDescriptorSet WebGPUBackend::GetBindlessDescriptorSet() const {
    Impl& im = *m_Impl;
    // Browser builds: the shared group is only the dummy fallback; on desktop
    // it holds the whole binding_array. RebuildBindlessBindGroup is otherwise
    // driven by Register/Update/Unregister, so seed it lazily here in case
    // none of those ran yet (all platforms) and refresh the singleton's group.
    if (!im.bindlessBindGroup)
        im.RebuildBindlessBindGroup();
    if (!im.bindlessSetRec) {
        im.bindlessSetRec = new DescSetRec();
        im.bindlessSetRec->isBindless = true;
        im.bindlessSetRec->layout = im.bindlessLayout;
        im.bindlessSetRec->bindGroup = im.bindlessBindGroup;
        im.bindlessSetRec->ownsBindGroup = false;
    }
    im.bindlessSetRec->bindGroup = im.bindlessBindGroup;
    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(im.bindlessSetRec);
    return out;
}

uint32_t WebGPUBackend::GetBindlessMaxTextures() const { return kBindlessMax; }

// ---- Framebuffer ----

RHIFramebuffer WebGPUBackend::CreateFramebuffer(RHIRenderPass renderPass,
    uint32_t width, uint32_t height, const std::vector<RHIImageView>& attachments) {
    (void)renderPass;
    FramebufferRec* rec = new FramebufferRec();
    rec->width = width;
    rec->height = height;
    for (const auto& a : attachments) {
        ImageViewRec* view = reinterpret_cast<ImageViewRec*>(a.handle);
        if (!view || !view->view) continue;
        if (view->isDepth)
            rec->depthAttachment = view;
        else
            rec->colorAttachments.push_back(view);
    }

    RHIFramebuffer out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void WebGPUBackend::DestroyFramebuffer(RHIFramebuffer framebuffer) {
    delete reinterpret_cast<FramebufferRec*>(framebuffer.handle);
}

// ---- Command recording ----

void WebGPUBackend::CmdBeginRenderPass(RHICommandBuffer cmd, RHIPassTemplate passTemplate,
    RHIFramebuffer framebuffer) {
    Impl& im = *m_Impl;
    WGPUCommandEncoder enc = reinterpret_cast<WGPUCommandEncoder>(cmd.handle);
    PassTemplateRec* trec = reinterpret_cast<PassTemplateRec*>(passTemplate.handle);
    FramebufferRec* frec = reinterpret_cast<FramebufferRec*>(framebuffer.handle);
    if (!enc || !trec || !frec) return;

    // Mask any bindless-registered color attachment to the dummy view for the
    // duration of this pass (wgpu exclusive-usage rule). Restored in
    // CmdEndRenderPass.
    im.maskedSlots.clear();
    im.maskedOrig.clear();
    for (ImageViewRec* view : frec->colorAttachments) {
        for (auto& kv : im.bindlessViews) {
            if (kv.second == view) {
                im.maskedSlots.push_back(kv.first);
                im.maskedOrig.push_back(view);
                break;
            }
        }
    }
    if (!im.maskedSlots.empty()) {
        for (uint32_t idx : im.maskedSlots) {
            im.bindlessViews.erase(idx);
#if defined(__EMSCRIPTEN__)
            im.InvalidateTextureBindGroup(idx);
#endif
        }
#if !defined(__EMSCRIPTEN__)
        im.RebuildBindlessBindGroup();
#endif
    }

    im.passW = frec->width;
    im.passH = frec->height;

    im.EndCurrentPass();

    // Clear values: color attachments use their (non-depth) clear; depth uses
    // the depth clear. Missing clears default to Clear (RenderTexture always
    // provides them).
    bool hasDepthClear = false;
    float depthClearValue = 1.0f;
    for (const auto& c : trec->clears)
        if (c.isDepth) { hasDepthClear = true; depthClearValue = c.depth; }

    std::vector<WGPURenderPassColorAttachment> colors;
    colors.reserve(frec->colorAttachments.size());
    size_t ci = 0;
    for (ImageViewRec* view : frec->colorAttachments) {
        WGPURenderPassColorAttachment ca{};
        ca.view = view->view;
        ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ca.loadOp = WGPULoadOp_Clear;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (ci < trec->clears.size() && !trec->clears[ci].isDepth) {
            ca.clearValue.r = trec->clears[ci].color.x;
            ca.clearValue.g = trec->clears[ci].color.y;
            ca.clearValue.b = trec->clears[ci].color.z;
            ca.clearValue.a = trec->clears[ci].color.w;
        }
        colors.push_back(ca);
        ++ci;
    }

    WGPURenderPassDepthStencilAttachment depth{};
    if (frec->depthAttachment && frec->depthAttachment->view) {
        depth.view = frec->depthAttachment->view;
        depth.depthLoadOp = hasDepthClear ? WGPULoadOp_Clear : WGPULoadOp_Load;
        depth.depthStoreOp = WGPUStoreOp_Store;
        depth.depthClearValue = depthClearValue;
    }

    WGPURenderPassDescriptor rpd{};
    rpd.label = WgpuStr("pass");
    rpd.colorAttachmentCount = colors.size();
    rpd.colorAttachments = colors.data();
    rpd.depthStencilAttachment = depth.view ? &depth : nullptr;
    im.currentPass = im.CommandEncoderBeginRenderPass(enc, &rpd);

    im.RenderPassEncoderSetViewport(im.currentPass, trec->vpX, trec->vpY,
        trec->vpW, trec->vpH, 0.0f, 1.0f);
    im.RenderPassEncoderSetScissorRect(im.currentPass, trec->scX, trec->scY,
        trec->scW, trec->scH);
}
void WebGPUBackend::CmdEndRenderPass(RHICommandBuffer cmd) {
    (void)cmd;
    Impl& im = *m_Impl;
    im.EndCurrentPass();
    // Restore bindless slots masked while this pass rendered to them.
    if (!im.maskedSlots.empty()) {
        for (size_t i = 0; i < im.maskedSlots.size(); ++i) {
            im.bindlessViews[im.maskedSlots[i]] = im.maskedOrig[i];
#if defined(__EMSCRIPTEN__)
            im.InvalidateTextureBindGroup(im.maskedSlots[i]);
#endif
        }
        im.maskedSlots.clear();
        im.maskedOrig.clear();
#if !defined(__EMSCRIPTEN__)
        im.RebuildBindlessBindGroup();
#endif
    }
}

void WebGPUBackend::CmdBindPipeline(RHICommandBuffer cmd, RHIPipeline pipeline) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    PipelineRec* rec = reinterpret_cast<PipelineRec*>(pipeline.handle);
    if (!rec || !rec->pipeline) return;
    im.RenderPassEncoderSetPipeline(im.currentPass, rec->pipeline);
}

void WebGPUBackend::CmdBindDescriptorSets(RHICommandBuffer cmd, RHIPipelineLayout layout,
    uint32_t firstSet, const std::vector<RHIDescriptorSet>& sets) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd; (void)layout;
    for (size_t i = 0; i < sets.size(); ++i) {
        DescSetRec* s = reinterpret_cast<DescSetRec*>(sets[i].handle);
        if (!s) continue;
        WGPUBindGroup bg = s->isBindless ? im.bindlessBindGroup : s->bindGroup;
        if (!bg) continue;
        uint32_t slot = firstSet + (uint32_t)i;
#if defined(__EMSCRIPTEN__)
        if (s->isBindless) im.bindlessSetSlot = (int)slot;
#endif
        im.RenderPassEncoderSetBindGroup(im.currentPass, slot, bg, 0, nullptr);
    }
}

void WebGPUBackend::CmdBindVertexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    BufferRec* rec = reinterpret_cast<BufferRec*>(buffer.handle);
    if (!rec || !rec->buffer) return;
    im.RenderPassEncoderSetVertexBuffer(im.currentPass, 0, rec->buffer, 0, WGPU_WHOLE_SIZE);
}

void WebGPUBackend::CmdBindIndexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    BufferRec* rec = reinterpret_cast<BufferRec*>(buffer.handle);
    if (!rec || !rec->buffer) return;
    im.RenderPassEncoderSetIndexBuffer(im.currentPass, rec->buffer, WGPUIndexFormat_Uint32,
        0, WGPU_WHOLE_SIZE);
}

void WebGPUBackend::CmdDraw(RHICommandBuffer cmd, uint32_t vertexCount, uint32_t firstVertex) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    im.RenderPassEncoderDraw(im.currentPass, vertexCount, 1, firstVertex, 0);
}

void WebGPUBackend::CmdDrawIndexed(RHICommandBuffer cmd, uint32_t indexCount,
    uint32_t instanceCount, uint32_t firstIndex) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    im.RenderPassEncoderDrawIndexed(im.currentPass, indexCount, instanceCount,
        firstIndex, 0, 0);
}

void WebGPUBackend::CmdPushConstants(RHICommandBuffer cmd, RHIPipelineLayout layout,
    ShaderStageMask stage, uint32_t offset, uint32_t size, const void* data) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd; (void)stage;

    PipelineLayoutRec* lr = reinterpret_cast<PipelineLayoutRec*>(layout.handle);
    if (!lr || !lr->hasPush) return;

    // One UBO per draw slot (see PipelineLayoutRec) so concurrent draws each
    // read their own push data. The slot counter is per-layout: a graph may
    // push on several layouts and each must index its own buffer list. The
    // buffer size is the max push range aligned to 16 (WebGPU uniform buffer
    // size rule).
    uint32_t slot = lr->pushSlot;
    if (lr->pushBuffers.size() <= slot) {
        uint32_t bufSize = std::max<uint32_t>((lr->pushSize + 15u) & ~15u, 16u);
        WGPUBufferDescriptor bd{};
        bd.label = WgpuStr("push");
        bd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        bd.size = bufSize;
        WGPUBuffer buf = im.DeviceCreateBuffer(im.device, &bd);

        WGPUBindGroupEntry entry{};
        entry.binding = 0;
        entry.buffer = buf;
        entry.size = WGPU_WHOLE_SIZE;
        WGPUBindGroupDescriptor bg{};
        bg.label = WgpuStr("push");
        bg.layout = im.pushLayout;
        bg.entryCount = 1;
        bg.entries = &entry;
        WGPUBindGroup group = im.DeviceCreateBindGroup(im.device, &bg);

        lr->pushBuffers.push_back(buf);
        lr->pushBindGroups.push_back(group);
    }
    if (slot >= lr->pushBuffers.size()) return;
    WGPUBuffer buf = lr->pushBuffers[slot];
    WGPUBindGroup group = lr->pushBindGroups[slot];
    if (!buf || !group) return;

    im.QueueWriteBuffer(im.queue, buf, offset, data, size);
    im.RenderPassEncoderSetBindGroup(im.currentPass, lr->pushGroup, group, 0, nullptr);
    lr->pushSlot = slot + 1;
}

void WebGPUBackend::CmdSetViewport(RHICommandBuffer cmd, const RHIViewport& viewport) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    // Positive viewport, no flip (D3D12/WebGPU convention).
    im.RenderPassEncoderSetViewport(im.currentPass, viewport.x, viewport.y,
        viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
}

void WebGPUBackend::CmdSetScissor(RHICommandBuffer cmd, const RHIRect2D& scissor) {
    Impl& im = *m_Impl;
    if (!im.currentPass) return;
    (void)cmd;
    uint32_t x = (uint32_t)scissor.x;
    uint32_t y = (uint32_t)scissor.y;
    uint32_t w = std::min(scissor.width, im.passW - std::min(x, im.passW));
    uint32_t h = std::min(scissor.height, im.passH - std::min(y, im.passH));
    im.RenderPassEncoderSetScissorRect(im.currentPass, x, y, w, h);
}

void WebGPUBackend::CmdBarrier(RHICommandBuffer cmd) {
    // No-op: WebGPU synchronizes automatically.
    (void)cmd;
}

void WebGPUBackend::CmdTransitionImageLayout(RHICommandBuffer cmd, RHIImage image,
    Format format, ImageLayout oldLayout, ImageLayout newLayout, Aspect aspect) {
    // No-op: WebGPU tracks layouts automatically.
    (void)cmd; (void)image; (void)format; (void)oldLayout; (void)newLayout; (void)aspect;
}

void WebGPUBackend::CmdExecuteGraph(RHICommandBuffer cmd, const GCommandGraph& graph) {
    Impl& im = *m_Impl;
    // Reset the per-layout push-slot counters used by CmdPushConstants: each
    // layout that pushes during this graph reuses its own buffers from slot 0.
    for (const auto& rec : graph.GetRecords()) {
        if (rec.type == GRecordType::Draw && rec.draw.layout.handle) {
            auto* lr = reinterpret_cast<PipelineLayoutRec*>(rec.draw.layout.handle);
            if (lr)
                lr->pushSlot = 0;
        }
    }
#if defined(__EMSCRIPTEN__)
    im.bindlessSetSlot = -1;
#endif
    for (const auto& rec : graph.GetRecords()) {
        switch (rec.type) {
        case GRecordType::BeginRenderPass: {
            // WebGPU synchronizes attachment usage automatically (no layout
            // tracking needed). Just begin the pass from the template.
            CmdBeginRenderPass(cmd, rec.pass.passTemplate, rec.pass.framebuffer);
            break;
        }
        case GRecordType::EndRenderPass:
            CmdEndRenderPass(cmd);
            break;
        case GRecordType::Draw: {
            if (rec.draw.pipeline.IsValid())
                CmdBindPipeline(cmd, rec.draw.pipeline);
            for (const auto& sb : rec.draw.setBindings)
                CmdBindDescriptorSets(cmd, sb.layout, sb.firstSet, sb.sets);
#if defined(__EMSCRIPTEN__)
            // Per-texture bind groups: override the bindless slot with the
            // group for this draw's sampled texture (single-texture layout —
            // naga cannot compile binding_array). Falls back to the shared
            // dummy group when the draw samples nothing.
            if (im.bindlessSetSlot >= 0) {
                WGPUBindGroup bg = rec.draw.sampledTextures.empty()
                    ? im.bindlessBindGroup
                    : im.GetTextureBindGroup(rec.draw.sampledTextures[0]);
                if (bg)
                    im.RenderPassEncoderSetBindGroup(im.currentPass,
                        (uint32_t)im.bindlessSetSlot, bg, 0, nullptr);
            }
#endif
            if (rec.draw.vertexBuffer.IsValid())
                CmdBindVertexBuffer(cmd, rec.draw.vertexBuffer);
            if (rec.draw.indexBuffer.IsValid())
                CmdBindIndexBuffer(cmd, rec.draw.indexBuffer);
            if (!rec.draw.pushData.empty()) {
                CmdPushConstants(cmd, rec.draw.layout, rec.draw.pushStage,
                    rec.draw.pushOffset, (uint32_t)rec.draw.pushData.size(),
                    rec.draw.pushData.data());
            }
            if (rec.draw.hasViewport)
                CmdSetViewport(cmd, rec.draw.viewport);
            if (rec.draw.hasScissor)
                CmdSetScissor(cmd, rec.draw.scissor);
            if (rec.draw.indexed)
                CmdDrawIndexed(cmd, rec.draw.indexCount,
                    rec.draw.instanceCount, rec.draw.firstIndex);
            else
                CmdDraw(cmd, rec.draw.vertexCount, rec.draw.firstVertex);
            break;
        }
        }
    }
}

} // namespace RHI
} // namespace Leir

#else // !(WIN32 && MSVC)

namespace Leir {
namespace RHI {

// WebGPU full impl is built on Windows/MSVC (wgpu-native zip) and Emscripten
// (emdawnwebgpu); other platforms get a stub that reports the backend as
// unavailable.

WebGPUBackend::WebGPUBackend(void*, int, int, bool, const std::string&)
    : m_Impl(nullptr)
{
}

WebGPUBackend::~WebGPUBackend() = default;

bool WebGPUBackend::BeginFrame(bool) { return false; }
void WebGPUBackend::BeginSwapchainOverlay() {}
void WebGPUBackend::EndFrame() {}
void WebGPUBackend::WaitIdle() {}
const GCaps& WebGPUBackend::GetCaps() const { static GCaps g; return g; }
RHICommandBuffer WebGPUBackend::GetCurrentCommandBuffer() const { return RHICommandBuffer{}; }
uint32_t WebGPUBackend::GetCurrentFrameIndex() const { return 0; }
uint32_t WebGPUBackend::GetSwapchainWidth() const { return 0; }
uint32_t WebGPUBackend::GetSwapchainHeight() const { return 0; }
RHIRenderPass WebGPUBackend::GetRenderPass() const { return RHIRenderPass{}; }
RHIRenderPass WebGPUBackend::GetOverlayRenderPass() const { return RHIRenderPass{}; }
bool WebGPUBackend::WasResized() const { return false; }
void WebGPUBackend::ResetResized() {}
void WebGPUBackend::NotifyResized() {}
void WebGPUBackend::RecreateSwapchain() {}
RHIShaderModule WebGPUBackend::CreateShaderModule(const std::vector<char>&) { return RHIShaderModule{}; }
void WebGPUBackend::DestroyShaderModule(RHIShaderModule) {}
RHIPipeline WebGPUBackend::CreateGraphicsPipeline(const RHIPipelineDesc&) { return RHIPipeline{}; }
void WebGPUBackend::DestroyPipeline(RHIPipeline) {}
RHIPipelineLayout WebGPUBackend::CreatePipelineLayout(
    const std::vector<RHIDescriptorSetLayout>&, const std::vector<RHIPushConstantRange>&) {
    return RHIPipelineLayout{};
}
void WebGPUBackend::DestroyPipelineLayout(RHIPipelineLayout) {}
RHIDescriptorSetLayout WebGPUBackend::CreateDescriptorSetLayout(
    const std::vector<RHIDescriptorBinding>&) { return RHIDescriptorSetLayout{}; }
void WebGPUBackend::DestroyDescriptorSetLayout(RHIDescriptorSetLayout) {}
RHIDescriptorPool WebGPUBackend::CreateDescriptorPool(
    const std::vector<RHIDescriptorBinding>&, uint32_t) { return RHIDescriptorPool{}; }
void WebGPUBackend::DestroyDescriptorPool(RHIDescriptorPool) {}
RHIDescriptorSet WebGPUBackend::AllocateDescriptorSet(RHIDescriptorPool, RHIDescriptorSetLayout) {
    return RHIDescriptorSet{};
}
void WebGPUBackend::WriteDescriptorSets(const std::vector<RHIDescriptorWrite>&) {}
RHIBuffer WebGPUBackend::CreateBuffer(uint32_t, BufferUsage, MemoryProperty, RHIDeviceMemory&) {
    return RHIBuffer{};
}
void WebGPUBackend::DestroyBuffer(RHIBuffer) {}
void WebGPUBackend::DestroyMemory(RHIDeviceMemory) {}
void WebGPUBackend::CopyBuffer(RHIBuffer, RHIBuffer, uint32_t) {}
bool WebGPUBackend::MapMemory(RHIDeviceMemory, uint32_t, uint32_t, void**) { return false; }
void WebGPUBackend::UnmapMemory(RHIDeviceMemory) {}
RHIImage WebGPUBackend::CreateImage(uint32_t, uint32_t, Format, ImageUsage, MemoryProperty,
    RHIDeviceMemory&) { return RHIImage{}; }
void WebGPUBackend::DestroyImage(RHIImage) {}
RHIImageView WebGPUBackend::CreateImageView(RHIImage, Format, Aspect) { return RHIImageView{}; }
void WebGPUBackend::DestroyImageView(RHIImageView) {}
RHISampler WebGPUBackend::CreateSampler(Filter, SamplerAddressMode) { return RHISampler{}; }
void WebGPUBackend::DestroySampler(RHISampler) {}
void WebGPUBackend::TransitionImageLayout(RHIImage, Format, ImageLayout, ImageLayout) {}
void WebGPUBackend::CopyBufferToImage(RHIBuffer, RHIImage, uint32_t, uint32_t) {}
RHIRenderPass WebGPUBackend::CreateRenderPass(const std::vector<Format>&, Format, bool) {
    return RHIRenderPass{};
}
void WebGPUBackend::DestroyRenderPass(RHIRenderPass) {}
RHIPassTemplate WebGPUBackend::CreatePassTemplate(const RHIPassTemplateDesc&) {
    return RHIPassTemplate{};
}
void WebGPUBackend::DestroyPassTemplate(RHIPassTemplate) {}
uint32_t WebGPUBackend::RegisterBindlessTexture(const RHIDescriptorImageInfo&) { return 0; }
void WebGPUBackend::UpdateBindlessTexture(uint32_t, const RHIDescriptorImageInfo&) {}
void WebGPUBackend::UnregisterBindlessTexture(uint32_t) {}
RHIDescriptorSet WebGPUBackend::GetBindlessDescriptorSet() const { return RHIDescriptorSet{}; }
uint32_t WebGPUBackend::GetBindlessMaxTextures() const { return 0; }
RHIFramebuffer WebGPUBackend::CreateFramebuffer(RHIRenderPass, uint32_t, uint32_t,
    const std::vector<RHIImageView>&) { return RHIFramebuffer{}; }
void WebGPUBackend::DestroyFramebuffer(RHIFramebuffer) {}
void WebGPUBackend::CmdBeginRenderPass(RHICommandBuffer, RHIPassTemplate, RHIFramebuffer) {}
void WebGPUBackend::CmdEndRenderPass(RHICommandBuffer) {}
void WebGPUBackend::CmdExecuteGraph(RHICommandBuffer, const GCommandGraph&) {}
void WebGPUBackend::CmdBindPipeline(RHICommandBuffer, RHIPipeline) {}
void WebGPUBackend::CmdBindDescriptorSets(RHICommandBuffer, RHIPipelineLayout, uint32_t,
    const std::vector<RHIDescriptorSet>&) {}
void WebGPUBackend::CmdBindVertexBuffer(RHICommandBuffer, RHIBuffer) {}
void WebGPUBackend::CmdBindIndexBuffer(RHICommandBuffer, RHIBuffer) {}
void WebGPUBackend::CmdDraw(RHICommandBuffer, uint32_t, uint32_t) {}
void WebGPUBackend::CmdDrawIndexed(RHICommandBuffer, uint32_t, uint32_t, uint32_t) {}
void WebGPUBackend::CmdPushConstants(RHICommandBuffer, RHIPipelineLayout, ShaderStageMask,
    uint32_t, uint32_t, const void*) {}
void WebGPUBackend::CmdSetViewport(RHICommandBuffer, const RHIViewport&) {}
void WebGPUBackend::CmdSetScissor(RHICommandBuffer, const RHIRect2D&) {}
void WebGPUBackend::CmdBarrier(RHICommandBuffer) {}
void WebGPUBackend::CmdTransitionImageLayout(RHICommandBuffer, RHIImage, Format, ImageLayout,
    ImageLayout, Aspect) {}

} // namespace RHI
} // namespace Leir

#endif // !(WIN32 && MSVC) && !__EMSCRIPTEN__