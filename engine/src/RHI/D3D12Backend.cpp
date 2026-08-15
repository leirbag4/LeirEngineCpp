#include "LeirEngine/RHI/D3D12Backend.h"

#include "LeirEngine/Core/Log.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcommon.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// D3D12 implementation of the RenderBackend interface.
//
// Handle mapping: every RHI handle stores a pointer to a backend record
// allocated with new (cast to uint64_t). Dispatchable handles (command list)
// store the native ID3D12GraphicsCommandList* directly.

namespace Leir {
namespace RHI {

namespace {

// Minimal COM smart pointer (avoid pulling <wrl/client.h> for MinGW compat).
template <class T>
struct ComPtr {
    T* p = nullptr;
    ComPtr() = default;
    ComPtr(std::nullptr_t) {}
    ~ComPtr() { Reset(); }
    ComPtr(const ComPtr& o) : p(o.p) { if (p) p->AddRef(); }
    ComPtr& operator=(const ComPtr& o) {
        if (this != &o) { if (o.p) o.p->AddRef(); Reset(); p = o.p; }
        return *this;
    }
    T* Get() const { return p; }
    T* operator->() const { return p; }
    T** operator&() { return &p; }
    void Reset() { if (p) { p->Release(); p = nullptr; } }
};

// ---- enum conversions ----

DXGI_FORMAT ToDxgi(Format f) {
    switch (f) {
        case Format::R32G32B32_SFLOAT:    return DXGI_FORMAT_R32G32B32_FLOAT;
        case Format::R32G32_SFLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
        case Format::R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::R8G8B8A8_SRGB:       return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case Format::B8G8R8A8_SRGB:       return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case Format::D32_SFLOAT:          return DXGI_FORMAT_D32_FLOAT;
        case Format::R32_SFLOAT:          return DXGI_FORMAT_R32_FLOAT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

D3D12_RESOURCE_STATES ToState(ImageLayout l) {
    switch (l) {
        case ImageLayout::Undefined:             return D3D12_RESOURCE_STATE_COMMON;
        case ImageLayout::TransferDst:           return D3D12_RESOURCE_STATE_COPY_DEST;
        case ImageLayout::ShaderReadOnly:        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        case ImageLayout::ColorAttachment:       return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case ImageLayout::DepthStencilAttachment: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    return D3D12_RESOURCE_STATE_COMMON;
}

// Stand-in for CD3DX12_HEAP_PROPERTIES (d3dx12.h is not bundled with the SDK).
D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = type;
    hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    return hp;
}

UINT AlignUp(UINT value, UINT alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ---- records ----

enum class SetKind : uint8_t { Sampler, UniformBuffer };

struct BufferRec {
    ComPtr<ID3D12Resource> res;
    UINT size = 0;
    void* mapped = nullptr;
};

struct ImageRec {
    ComPtr<ID3D12Resource> res;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0, height = 0;
    bool isRT = false;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

// Memory record. For buffers/images the memory and the resource share the same
// ID3D12Resource (double AddRef: DestroyBuffer/DestroyImage + DestroyMemory both
// need to release before the resource dies).
struct MemoryRec {
    ComPtr<ID3D12Resource> res;
    void* mapped = nullptr;
};

struct ImageViewRec {
    ImageRec* image = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool isRTV = false;
    bool isDSV = false;
    UINT rtvSlot = UINT_MAX;
    UINT dsvSlot = UINT_MAX;
};

struct SamplerRec {
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
    // Sampler characteristics. With bindless each texture's sampler is re-created
    // at its own heap slot from this desc (the cached gpu handle above is only
    // used by the legacy single-sampler WriteDescriptorSets path).
    D3D12_SAMPLER_DESC desc{};
};

struct ShaderRec {
    std::vector<uint8_t> blob;
};

struct DescSetLayoutRec {
    DescriptorType type = DescriptorType::CombinedImageSampler;
    ShaderStage stage = ShaderStage::Fragment;
    bool bindless = false;
};

struct DescPoolRec {};

struct DescSetRec {
    SetKind kind = SetKind::Sampler;
    UINT srvSlot = UINT_MAX;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = {};
    D3D12_GPU_DESCRIPTOR_HANDLE samplerGpu = {};
    BufferRec* ubo = nullptr;
};

struct RootParamInfo {
    SetKind kind = SetKind::Sampler;
    UINT srvParam = 0;
    UINT samplerParam = 0;
};

struct PipelineLayoutRec {
    ComPtr<ID3D12RootSignature> rootSig;
    std::vector<RootParamInfo> setParams; // indexed by descriptor set index
    UINT pushParam = UINT_MAX;
};

struct PipelineRec {
    ComPtr<ID3D12PipelineState> pso;
    PipelineLayoutRec* layout = nullptr;
    UINT vbStride = 0;
    D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

struct RenderPassRec {
    std::vector<DXGI_FORMAT> colorFormats;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
    bool hasDepth = false;
    bool overlay = false;
};

// Persistent render-pass state (TODO_RHI_SLANG.md §3.1 GPassTemplate): clears
// + viewport + scissor precomputed once, referenced per frame.
struct PassTemplateRec {
    std::vector<RHIClearValue> clears;
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
};

struct FramebufferRec {
    RenderPassRec* rp = nullptr;
    UINT width = 0, height = 0;
    std::vector<ImageViewRec*> colorAttachments;
    ImageViewRec* depthAttachment = nullptr;
};

} // namespace

struct D3D12Backend::Impl {
    // Debug controller must be declared first so it is destroyed LAST (after
    // the device), letting the debug layer report device destruction.
    ComPtr<ID3D12Debug> m_debugCtrl;

    GCaps caps;

    ComPtr<IDXGIFactory4> factory;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain3> swapchain;
    HWND hwnd = nullptr;
    int width = 0, height = 0;
    bool vsync = false;

    static const UINT kFrames = 2;
    ComPtr<ID3D12CommandAllocator> allocators[kFrames];
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    ComPtr<ID3D12Fence> fences[kFrames];
    HANDLE fenceEvents[kFrames] = {};
    UINT64 fenceValues[kFrames] = {};
    UINT frameIndex = 0;
    UINT backBufferIndex = 0;

    bool resized = false;

    // One-shot copy helper
    ComPtr<ID3D12CommandAllocator> copyAlloc;
    ComPtr<ID3D12GraphicsCommandList> copyList;
    ComPtr<ID3D12Fence> copyFence;
    HANDLE copyEvent = nullptr;
    UINT64 copyFenceValue = 0;
    ComPtr<ID3D12Fence> waitFence;
    HANDLE waitEvent = nullptr;
    UINT64 waitFenceValue = 0;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;      // shader-visible CBV/SRV/UAV
    ComPtr<ID3D12DescriptorHeap> samplerHeap;  // shader-visible samplers
    UINT rtvInc = 0, dsvInc = 0, srvInc = 0, samplerInc = 0;
    std::vector<bool> rtvFree;
    std::vector<bool> dsvFree;
    std::vector<bool> srvFree;
    std::vector<bool> samplerFree;

    std::map<std::pair<int, int>, UINT> samplerCache; // (filter,address) -> slot

    // Bindless texture table (descriptor indexing, TODO_RHI_SLANG.md §3.5):
    // each registered texture owns one SRV (slot i) + one sampler (slot i),
    // referenced from shaders as a stable index. Bounded by the sampler heap
    // (D3D12 caps at 2048 shader-visible samplers).
    static const UINT kBindless = 2048;
    uint32_t bindlessNext = 0;
    std::vector<uint32_t> bindlessFree;

    uint32_t NextBindlessIndex() {
        if (!bindlessFree.empty()) {
            uint32_t i = bindlessFree.back();
            bindlessFree.pop_back();
            return i;
        }
        if (bindlessNext < kBindless) return bindlessNext++;
        XConsole::PrintError("D3D12: bindless texture table full ({})", (unsigned)kBindless);
        return 0;
    }

    // Swapchain
    static const UINT kBackBuffers = 3;
    ComPtr<ID3D12Resource> backBuffers[kBackBuffers];
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTVs[kBackBuffers] = {};
    UINT backBufferRtvSlots[kBackBuffers] = { UINT_MAX, UINT_MAX, UINT_MAX };
    D3D12_RESOURCE_STATES backBufferState[kBackBuffers];

    // Built-in render passes
    RenderPassRec* mainRenderPass = nullptr;
    RenderPassRec* overlayRenderPass = nullptr;

    // Current pipeline (for vertex-buffer stride in CmdBindVertexBuffer)
    PipelineRec* currentPipeline = nullptr;
    ComPtr<ID3D12RootSignature> currentRootSig;

    // Cached bindless descriptor set (the whole heaps as tables).
    DescSetRec* bindlessSetRec = nullptr;

    Impl(void* window, int w, int h, bool vs, const std::string& appName) {
        hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(window));

        // The swapchain must match the window's client area in PHYSICAL pixels
        // (same convention RecreateSwapchainInternal uses). The editor passes
        // the logical size (GetWidth/GetHeight), which differs on HiDPI.
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &fbW, &fbH);
        width = fbW > 0 ? fbW : w;
        height = fbH > 0 ? fbH : h;
        vsync = vs;
        (void)appName;

        bool debug = false;
#ifdef _DEBUG
        debug = true;
#endif
        // Env override for diagnosing device-removed (debug layer can mask
        // driver issues).
        if (const char* dl = std::getenv("LEIR_D3D12_DEBUG"))
            debug = dl[0] != '0';

        if (debug) {
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugCtrl))))
                m_debugCtrl->EnableDebugLayer();
        }

        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
            throw std::runtime_error("D3D12: failed to create device");

        // If the debug layer is on, tell it to keep messages so we can dump them
        // later (a device-removal usually surfaces as a stored message).
        if (debug) {
            if (ComPtr<ID3D12InfoQueue> iq;
                SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&iq))))
                iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, FALSE);
        }

        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))))
            throw std::runtime_error("D3D12: failed to create command queue");

        UINT dxgiFlags = 0;
        if (debug) dxgiFlags = DXGI_CREATE_FACTORY_DEBUG;
        if (FAILED(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory))))
            throw std::runtime_error("D3D12: failed to create DXGI factory");

        // Descriptor heaps first: the swapchain's InitBackBuffers allocates RTV
        // slots, so rtvHeap/rtvFree must exist before CreateSwapchain().
        CreateDescriptorHeaps();
        CreateSwapchain();
        CreateFrameObjects();
        CreateCopyHelper();
        CreateBuiltinRenderPasses();

        // ---- Capabilities (TODO_RHI_SLANG.md §3.6) ----
        D3D12_FEATURE_DATA_D3D12_OPTIONS opts{};
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts)))) {
            memset(&opts, 0, sizeof(opts));
        }
        caps.bindless = opts.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3;
        caps.maxTexturesPerTable = 1000000;      // max descriptors in a shader-visible CBV/SRV/UAV heap
        caps.maxUniformBuffersPerTable = 1000000;
        caps.maxStorageBuffersPerTable = 1000000;
        caps.maxSamplersPerTable = 2048;         // max samplers in a shader-visible sampler heap
        caps.maxPushConstantsSize = 256;         // 64 DWORDs of root constants
        caps.maxColorAttachments = 8;            // D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT
        caps.maxTextureSize = 16384;             // D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION
        caps.minUniformBufferOffsetAlignment = 256;
        caps.multiRenderTarget = true;
        caps.instancing = true;
        caps.compute = true;
        caps.storageBuffers = true;
        caps.sRGB = true;
        caps.wireframe = true;
        caps.anisotropicFiltering = true;

        XConsole::Println("D3D12 backend created ({}x{})", width, height);
    }

    ~Impl() {
        WaitIdle();
        delete bindlessSetRec;
        for (auto& ev : fenceEvents) if (ev) CloseHandle(ev);
        if (copyEvent) CloseHandle(copyEvent);
        if (waitEvent) CloseHandle(waitEvent);
    }

    // ---- helpers ----

    D3D12_CPU_DESCRIPTOR_HANDLE RtvCpu(UINT i) {
        return { rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * rtvInc };
    }
    D3D12_CPU_DESCRIPTOR_HANDLE DsvCpu(UINT i) {
        return { dsvHeap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * dsvInc };
    }
    D3D12_CPU_DESCRIPTOR_HANDLE SrvCpu(UINT i) {
        return { srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * srvInc };
    }
    D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu(UINT i) {
        return { srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * srvInc };
    }
    D3D12_CPU_DESCRIPTOR_HANDLE SamplerCpu(UINT i) {
        return { samplerHeap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * samplerInc };
    }
    D3D12_GPU_DESCRIPTOR_HANDLE SamplerGpu(UINT i) {
        return { samplerHeap->GetGPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * samplerInc };
    }

    UINT AllocRtv() {
        for (UINT i = 0; i < (UINT)rtvFree.size(); ++i)
            if (!rtvFree[i]) { rtvFree[i] = true; return i; }
        throw std::runtime_error("D3D12: RTV heap exhausted");
    }
    UINT AllocDsv() {
        for (UINT i = 0; i < (UINT)dsvFree.size(); ++i)
            if (!dsvFree[i]) { dsvFree[i] = true; return i; }
        throw std::runtime_error("D3D12: DSV heap exhausted");
    }
    UINT AllocSrv() {
        for (UINT i = 0; i < (UINT)srvFree.size(); ++i)
            if (!srvFree[i]) { srvFree[i] = true; return i; }
        throw std::runtime_error("D3D12: SRV heap exhausted");
    }
    UINT AllocSampler() {
        for (UINT i = 0; i < (UINT)samplerFree.size(); ++i)
            if (!samplerFree[i]) { samplerFree[i] = true; return i; }
        throw std::runtime_error("D3D12: sampler heap exhausted");
    }

    void CreateSwapchain() {
        if (width < 1) width = 1;
        if (height < 1) height = 1;

        DXGI_SWAP_CHAIN_DESC1 scDesc{};
        scDesc.Width = (UINT)width;
        scDesc.Height = (UINT)height;
        scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // resource stays UNORM; sRGB applied via RTV view format
        scDesc.Stereo = FALSE;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.BufferCount = kBackBuffers;
        scDesc.Scaling = DXGI_SCALING_STRETCH;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        ComPtr<IDXGISwapChain1> sc1;
        HRESULT hres = factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &scDesc,
                nullptr, nullptr, &sc1);
        if (FAILED(hres))
            throw std::runtime_error("D3D12: failed to create swap chain (HRESULT 0x"
                + [&]{ char b[16]; sprintf(b, "%08X", (unsigned)hres); return std::string(b); }() + ")");
        if (FAILED(sc1->QueryInterface(IID_PPV_ARGS(&swapchain))))
            throw std::runtime_error("D3D12: failed to QI swap chain");
        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        InitBackBuffers();
    }

    void InitBackBuffers() {
        // Free the previously-allocated RTV slots (if any) so resize doesn't
        // leak descriptor-heap space.
        for (UINT i = 0; i < kBackBuffers; ++i) {
            if (backBufferRtvSlots[i] != UINT_MAX) {
                rtvFree[backBufferRtvSlots[i]] = false;
                backBufferRtvSlots[i] = UINT_MAX;
            }
        }
        for (UINT i = 0; i < kBackBuffers; ++i) {
            backBuffers[i].Reset();
            if (FAILED(swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]))))
                throw std::runtime_error("D3D12: failed to get back buffer");
            UINT slot = AllocRtv();
            backBufferRtvSlots[i] = slot;
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = RtvCpu(slot);
            // Backbuffer resource is UNORM (the Intel UHD driver device-removes on an
            // sRGB flip-model swapchain), but the RTV is UNORM_SRGB so the GPU encodes
            // linear→sRGB on store — identical result to an sRGB swapchain, and the PSO
            // color formats (mainRenderPass/overlayRenderPass) already declare UNORM_SRGB.
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(backBuffers[i].Get(), &rtvDesc, rtv);
            backBufferRTVs[i] = rtv;
            backBufferState[i] = D3D12_RESOURCE_STATE_PRESENT;
        }
    }

    void CreateFrameObjects() {
        for (UINT i = 0; i < kFrames; ++i) {
            if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&allocators[i]))))
                throw std::runtime_error("D3D12: failed to create command allocator");
            if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i]))))
                throw std::runtime_error("D3D12: failed to create fence");
            fenceEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        }
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                allocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdList))))
            throw std::runtime_error("D3D12: failed to create command list");
        cmdList->Close();
        cmdList->QueryInterface(IID_PPV_ARGS(&cmdList4));
    }

    void CreateDescriptorHeaps() {
        rtvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        srvInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        samplerInc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 64;
        device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvHeap));
        rtvFree.assign(64, false);

        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hd.NumDescriptors = 32;
        device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dsvHeap));
        dsvFree.assign(32, false);

        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 4096;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&srvHeap));
        srvFree.assign(4096, false);

        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        hd.NumDescriptors = kBindless; // one slot per bindless texture index
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&samplerHeap));
        samplerFree.assign(kBindless, false);
    }

    void CreateCopyHelper() {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&copyAlloc))))
            throw std::runtime_error("D3D12: failed to create copy allocator");
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                copyAlloc.Get(), nullptr, IID_PPV_ARGS(&copyList))))
            throw std::runtime_error("D3D12: failed to create copy list");
        copyList->Close();
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&copyFence))))
            throw std::runtime_error("D3D12: failed to create copy fence");
        copyEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&waitFence))))
            throw std::runtime_error("D3D12: failed to create wait fence");
        waitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    void CreateBuiltinRenderPasses() {
        mainRenderPass = new RenderPassRec();
        mainRenderPass->colorFormats = { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
        mainRenderPass->depthFormat = DXGI_FORMAT_D32_FLOAT;
        mainRenderPass->hasDepth = true;
        mainRenderPass->overlay = false;

        overlayRenderPass = new RenderPassRec();
        overlayRenderPass->colorFormats = { DXGI_FORMAT_B8G8R8A8_UNORM_SRGB };
        overlayRenderPass->hasDepth = false;
        overlayRenderPass->overlay = true;
    }

    void DumpInfoQueue() {
        ComPtr<ID3D12InfoQueue> iq;
        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&iq)))) return;
        for (UINT64 n = iq->GetNumStoredMessages(); n > 0; --n) {
            SIZE_T len = 0;
            iq->GetMessage(n - 1, nullptr, &len);
            if (len > 0) {
                std::vector<BYTE> buf(len);
                D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
                if (SUCCEEDED(iq->GetMessage(n - 1, msg, &len)) && msg->pDescription)
                    XConsole::PrintError("D3D12 message: {}", msg->pDescription);
            }
        }
    }

    // Execute a set of commands immediately and wait for the GPU (init-time ops).
    void ExecuteImmediate(std::function<void(ID3D12GraphicsCommandList*)> fn) {
        copyAlloc->Reset();
        copyList->Reset(copyAlloc.Get(), nullptr);
        fn(copyList.Get());
        copyList->Close();
        ID3D12CommandList* lists[] = { copyList.Get() };
        queue->ExecuteCommandLists(1, lists);
        ++copyFenceValue;
        queue->Signal(copyFence.Get(), copyFenceValue);
        copyFence->SetEventOnCompletion(copyFenceValue, copyEvent);
        WaitForSingleObject(copyEvent, INFINITE);
        HRESULT reason = device->GetDeviceRemovedReason();
        if (FAILED(reason)) {
            XConsole::PrintError("D3D12: device removed after ExecuteImmediate (hr=0x{:08X})",
                (unsigned)reason);
            DumpInfoQueue();
        }
    }

    void WaitIdle() {
        ++waitFenceValue;
        queue->Signal(waitFence.Get(), waitFenceValue);
        waitFence->SetEventOnCompletion(waitFenceValue, waitEvent);
        WaitForSingleObject(waitEvent, INFINITE);
    }

    void RecreateSwapchainInternal() {
        WaitIdle();
        for (UINT i = 0; i < kBackBuffers; ++i)
            backBuffers[i].Reset();

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(glfwGetCurrentContext(), &fbW, &fbH);
        if (fbW < 1) fbW = 1;
        if (fbH < 1) fbH = 1;

        if (FAILED(swapchain->ResizeBuffers(kBackBuffers, (UINT)fbW, (UINT)fbH,
                DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
            throw std::runtime_error("D3D12: failed to resize swap chain");
        width = fbW;
        height = fbH;
        InitBackBuffers();
    }
};

D3D12Backend::D3D12Backend(void* window, int width, int height, bool vsync,
                           const std::string& appName)
    : m_Impl(std::make_unique<Impl>(window, width, height, vsync, appName))
{
}

D3D12Backend::~D3D12Backend() = default;

// ---- Frame lifecycle ----

bool D3D12Backend::BeginFrame(bool skipRenderPass) {
    (void)skipRenderPass;
    Impl& im = *m_Impl;

    if (im.resized) {
        im.resized = false;
        try { im.RecreateSwapchainInternal(); }
        catch (const std::exception&) { im.width = 1; im.height = 1; }
    }

    // Wait for the previous frame using this allocator to complete.
    if (im.fences[im.frameIndex]->GetCompletedValue() < im.fenceValues[im.frameIndex]) {
        im.fences[im.frameIndex]->SetEventOnCompletion(
            im.fenceValues[im.frameIndex], im.fenceEvents[im.frameIndex]);
        WaitForSingleObject(im.fenceEvents[im.frameIndex], INFINITE);
    }

    if (FAILED(im.allocators[im.frameIndex]->Reset()))
        return false;
    if (FAILED(im.cmdList->Reset(im.allocators[im.frameIndex].Get(), nullptr)))
        return false;

    im.backBufferIndex = im.swapchain->GetCurrentBackBufferIndex();

    ID3D12DescriptorHeap* heaps[] = { im.srvHeap.Get(), im.samplerHeap.Get() };
    im.cmdList->SetDescriptorHeaps(2, heaps);
    im.currentRootSig.Reset();

    // Back buffer: PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER bbBarrier{};
    bbBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bbBarrier.Transition.pResource = im.backBuffers[im.backBufferIndex].Get();
    bbBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    bbBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bbBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    im.cmdList->ResourceBarrier(1, &bbBarrier);
    im.backBufferState[im.backBufferIndex] = D3D12_RESOURCE_STATE_RENDER_TARGET;

    return true;
}

void D3D12Backend::BeginSwapchainOverlay() {
    Impl& im = *m_Impl;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = im.backBufferRTVs[im.backBufferIndex];

    im.cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    float clear[4] = { 0.12f, 0.12f, 0.15f, 1.0f };
    im.cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);

    D3D12_VIEWPORT vp{ 0.0f, 0.0f, (float)im.width, (float)im.height, 0.0f, 1.0f };
    im.cmdList->RSSetViewports(1, &vp);
    D3D12_RECT sc{ 0, 0, (LONG)im.width, (LONG)im.height };
    im.cmdList->RSSetScissorRects(1, &sc);
}

void D3D12Backend::EndFrame() {
    Impl& im = *m_Impl;

    // Back buffer: RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER bbBarrier{};
    bbBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bbBarrier.Transition.pResource = im.backBuffers[im.backBufferIndex].Get();
    bbBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bbBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    bbBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    im.cmdList->ResourceBarrier(1, &bbBarrier);
    im.backBufferState[im.backBufferIndex] = D3D12_RESOURCE_STATE_PRESENT;

    im.cmdList->Close();

    ID3D12CommandList* lists[] = { im.cmdList.Get() };
    im.queue->ExecuteCommandLists(1, lists);
    im.swapchain->Present(im.vsync ? 1 : 0, 0);

    ++im.fenceValues[im.frameIndex];
    im.queue->Signal(im.fences[im.frameIndex].Get(), im.fenceValues[im.frameIndex]);

    im.frameIndex = (im.frameIndex + 1) % Impl::kFrames;
}

void D3D12Backend::WaitIdle() { m_Impl->WaitIdle(); }

const GCaps& D3D12Backend::GetCaps() const { return m_Impl->caps; }

RHICommandBuffer D3D12Backend::GetCurrentCommandBuffer() const {
    RHICommandBuffer cb;
    cb.handle = reinterpret_cast<uint64_t>(m_Impl->cmdList.Get());
    return cb;
}
uint32_t D3D12Backend::GetCurrentFrameIndex() const { return m_Impl->frameIndex; }
uint32_t D3D12Backend::GetSwapchainWidth() const { return (uint32_t)m_Impl->width; }
uint32_t D3D12Backend::GetSwapchainHeight() const { return (uint32_t)m_Impl->height; }

RHIRenderPass D3D12Backend::GetRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->mainRenderPass);
    return rp;
}
RHIRenderPass D3D12Backend::GetOverlayRenderPass() const {
    RHIRenderPass rp;
    rp.handle = reinterpret_cast<uint64_t>(m_Impl->overlayRenderPass);
    return rp;
}

bool D3D12Backend::WasResized() const { return m_Impl->resized; }
void D3D12Backend::ResetResized() { m_Impl->resized = false; }
void D3D12Backend::NotifyResized() { m_Impl->resized = true; }
void D3D12Backend::RecreateSwapchain() { m_Impl->RecreateSwapchainInternal(); }

// ---- Resource creation ----

RHIShaderModule D3D12Backend::CreateShaderModule(const std::vector<char>& code) {
    ShaderRec* rec = new ShaderRec();
    rec->blob.resize(code.size());
    std::memcpy(rec->blob.data(), code.data(), code.size());
    RHIShaderModule m;
    m.handle = reinterpret_cast<uint64_t>(rec);
    return m;
}
void D3D12Backend::DestroyShaderModule(RHIShaderModule module) {
    delete reinterpret_cast<ShaderRec*>(module.handle);
}

RHIPipeline D3D12Backend::CreateGraphicsPipeline(const RHIPipelineDesc& desc) {
    PipelineLayoutRec* layout = reinterpret_cast<PipelineLayoutRec*>(desc.layout.handle);
    RenderPassRec* rp = reinterpret_cast<RenderPassRec*>(desc.renderPass.handle);
    PipelineRec* rec = new PipelineRec();
    rec->layout = layout;
    rec->vbStride = desc.vertexBinding.stride;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = layout->rootSig.Get();

    for (const auto& s : desc.stages) {
        ShaderRec* sr = reinterpret_cast<ShaderRec*>(s.module.handle);
        D3D12_SHADER_BYTECODE bc{ sr->blob.data(), (SIZE_T)sr->blob.size() };
        if (s.stage == ShaderStage::Vertex) pso.VS = bc;
        else if (s.stage == ShaderStage::Fragment) pso.PS = bc;
    }

    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    elements.reserve(desc.vertexAttributes.size());
    for (const auto& a : desc.vertexAttributes) {
        D3D12_INPUT_ELEMENT_DESC el{};
        el.SemanticName = a.semantic && *a.semantic ? a.semantic
            : (a.location == 0 ? "POSITION" : (a.location == 1 ? "TEXCOORD" : "COLOR"));
        el.SemanticIndex = a.semanticIndex;
        el.Format = ToDxgi(a.format);
        el.InputSlot = a.binding;
        el.AlignedByteOffset = a.offset;
        el.InputSlotClass = desc.vertexBinding.inputRate == VertexInputRate::Instance
            ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
            : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        el.InstanceDataStepRate = el.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA ? 1 : 0;
        elements.push_back(el);
    }
    pso.InputLayout = { elements.data(), (UINT)elements.size() };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    rec->primitiveTopology = desc.topology == Topology::TriangleStrip
        ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = desc.cullMode == CullMode::None ? D3D12_CULL_MODE_NONE
        : (desc.cullMode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK);
    pso.RasterizerState.FrontCounterClockwise = TRUE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < 8; ++i)
        pso.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    if (desc.blend.enable) {
        pso.BlendState.RenderTarget[0].BlendEnable = TRUE;
        pso.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    }

    pso.DepthStencilState.DepthEnable = desc.depthTestEnable ? TRUE : FALSE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;

    pso.NumRenderTargets = (UINT)rp->colorFormats.size();
    for (UINT i = 0; i < (UINT)rp->colorFormats.size(); ++i)
        pso.RTVFormats[i] = rp->colorFormats[i];
    if (rp->hasDepth) pso.DSVFormat = rp->depthFormat;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX; // 0 (default) hides all samples -> nothing drawn

    HRESULT hr = m_Impl->device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&rec->pso));
    if (FAILED(hr)) {
        XConsole::PrintError("D3D12: failed to create graphics pipeline (0x{:08X})",
            (unsigned)hr);
        m_Impl->DumpInfoQueue();
        delete rec;
        return RHIPipeline{};
    }
    RHIPipeline p;
    p.handle = reinterpret_cast<uint64_t>(rec);
    return p;
}
void D3D12Backend::DestroyPipeline(RHIPipeline pipeline) {
    delete reinterpret_cast<PipelineRec*>(pipeline.handle);
}

RHIPipelineLayout D3D12Backend::CreatePipelineLayout(
    const std::vector<RHIDescriptorSetLayout>& layouts,
    const std::vector<RHIPushConstantRange>& pushConstants) {
    PipelineLayoutRec* rec = new PipelineLayoutRec();

    std::vector<D3D12_ROOT_PARAMETER> params;
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    ranges.reserve(layouts.size() * 2);

    for (const auto& l : layouts) {
        DescSetLayoutRec* lr = reinterpret_cast<DescSetLayoutRec*>(l.handle);
        if (lr->type == DescriptorType::UniformBuffer) {
            D3D12_ROOT_PARAMETER p{};
            p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            p.Descriptor.ShaderRegister = 0;
            p.Descriptor.RegisterSpace = 0;
            p.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            rec->setParams.push_back({ SetKind::UniformBuffer, (UINT)params.size(), 0 });
            params.push_back(p);
        } else if (lr->bindless) {
            // Bindless texture set: the shader declares unbounded arrays that
            // the backend backs with the whole descriptor heaps as tables —
            // SRV array at t0,space1, sampler array at s0,space2 (stable,
            // independent of the vk set number; see TODO_RHI_SLANG.md §4.1).
            D3D12_DESCRIPTOR_RANGE srvRange{};
            srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors = Impl::kBindless;
            srvRange.BaseShaderRegister = 0;
            srvRange.RegisterSpace = 1;
            srvRange.OffsetInDescriptorsFromTableStart = 0;
            ranges.push_back(srvRange);
            D3D12_DESCRIPTOR_RANGE* srvPtr = &ranges.back();

            D3D12_DESCRIPTOR_RANGE samRange{};
            samRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            samRange.NumDescriptors = Impl::kBindless;
            samRange.BaseShaderRegister = 0;
            samRange.RegisterSpace = 2;
            samRange.OffsetInDescriptorsFromTableStart = 0;
            ranges.push_back(samRange);
            D3D12_DESCRIPTOR_RANGE* samPtr = &ranges.back();

            D3D12_ROOT_PARAMETER psrv{};
            psrv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            psrv.DescriptorTable.NumDescriptorRanges = 1;
            psrv.DescriptorTable.pDescriptorRanges = srvPtr;
            psrv.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            UINT srvIdx = (UINT)params.size();
            params.push_back(psrv);

            D3D12_ROOT_PARAMETER psam{};
            psam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            psam.DescriptorTable.NumDescriptorRanges = 1;
            psam.DescriptorTable.pDescriptorRanges = samPtr;
            psam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            UINT samIdx = (UINT)params.size();
            params.push_back(psam);

            rec->setParams.push_back({ SetKind::Sampler, srvIdx, samIdx });
        } else {
            D3D12_DESCRIPTOR_RANGE srvRange{};
            srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors = 1;
            srvRange.BaseShaderRegister = 0;
            srvRange.RegisterSpace = 0;
            srvRange.OffsetInDescriptorsFromTableStart = 0;
            ranges.push_back(srvRange);
            D3D12_DESCRIPTOR_RANGE* srvPtr = &ranges.back();

            D3D12_DESCRIPTOR_RANGE samRange{};
            samRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            samRange.NumDescriptors = 1;
            samRange.BaseShaderRegister = 0;
            samRange.RegisterSpace = 0;
            samRange.OffsetInDescriptorsFromTableStart = 0;
            ranges.push_back(samRange);
            D3D12_DESCRIPTOR_RANGE* samPtr = &ranges.back();

            D3D12_ROOT_PARAMETER psrv{};
            psrv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            psrv.DescriptorTable.NumDescriptorRanges = 1;
            psrv.DescriptorTable.pDescriptorRanges = srvPtr;
            psrv.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            UINT srvIdx = (UINT)params.size();
            params.push_back(psrv);

            D3D12_ROOT_PARAMETER psam{};
            psam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            psam.DescriptorTable.NumDescriptorRanges = 1;
            psam.DescriptorTable.pDescriptorRanges = samPtr;
            psam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            UINT samIdx = (UINT)params.size();
            params.push_back(psam);

            rec->setParams.push_back({ SetKind::Sampler, srvIdx, samIdx });
        }
    }

    if (!pushConstants.empty() && pushConstants[0].size > 0) {
        D3D12_ROOT_PARAMETER p{};
        p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p.Constants.ShaderRegister = 1; // push constants pinned to b1 in .slang
        p.Constants.RegisterSpace = 0;
        p.Constants.Num32BitValues = pushConstants[0].size / 4;
        p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rec->pushParam = (UINT)params.size();
        params.push_back(p);
    }

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = (UINT)params.size();
    rsDesc.pParameters = params.empty() ? nullptr : params.data();
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> err;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &blob, &err);
    if (FAILED(hr)) {
        XConsole::PrintError("D3D12: failed to serialize root signature");
        delete rec;
        return RHIPipelineLayout{};
    }
    hr = m_Impl->device->CreateRootSignature(0, blob->GetBufferPointer(),
        blob->GetBufferSize(), IID_PPV_ARGS(&rec->rootSig));
    if (FAILED(hr)) {
        XConsole::PrintError("D3D12: failed to create root signature");
        delete rec;
        return RHIPipelineLayout{};
    }

    RHIPipelineLayout out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyPipelineLayout(RHIPipelineLayout layout) {
    delete reinterpret_cast<PipelineLayoutRec*>(layout.handle);
}

RHIDescriptorSetLayout D3D12Backend::CreateDescriptorSetLayout(
    const std::vector<RHIDescriptorBinding>& bindings) {
    DescSetLayoutRec* rec = new DescSetLayoutRec();
    if (!bindings.empty()) {
        rec->type = bindings[0].type;
        rec->stage = bindings[0].stage;
        for (const auto& b : bindings)
            if (b.bindless) rec->bindless = true;
    }
    RHIDescriptorSetLayout out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyDescriptorSetLayout(RHIDescriptorSetLayout layout) {
    delete reinterpret_cast<DescSetLayoutRec*>(layout.handle);
}

RHIDescriptorPool D3D12Backend::CreateDescriptorPool(
    const std::vector<RHIDescriptorBinding>& poolBindings, uint32_t maxSets) {
    (void)poolBindings; (void)maxSets;
    DescPoolRec* rec = new DescPoolRec();
    RHIDescriptorPool out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyDescriptorPool(RHIDescriptorPool pool) {
    delete reinterpret_cast<DescPoolRec*>(pool.handle);
}

RHIDescriptorSet D3D12Backend::AllocateDescriptorSet(
    RHIDescriptorPool pool, RHIDescriptorSetLayout layout) {
    (void)pool;
    DescSetLayoutRec* lr = reinterpret_cast<DescSetLayoutRec*>(layout.handle);
    DescSetRec* rec = new DescSetRec();
    rec->kind = lr->type == DescriptorType::UniformBuffer ? SetKind::UniformBuffer : SetKind::Sampler;
    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}

void D3D12Backend::WriteDescriptorSets(const std::vector<RHIDescriptorWrite>& writes) {
    Impl& im = *m_Impl;
    for (const auto& w : writes) {
        DescSetRec* set = reinterpret_cast<DescSetRec*>(w.dstSet.handle);
        if (w.type == DescriptorType::CombinedImageSampler && w.imageInfo.valid) {
            ImageViewRec* view = reinterpret_cast<ImageViewRec*>(w.imageInfo.imageView.handle);
            SamplerRec* samp = reinterpret_cast<SamplerRec*>(w.imageInfo.sampler.handle);

            if (set->srvSlot == UINT_MAX)
                set->srvSlot = im.AllocSrv();

            D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
            srv.Format = view->format;
            srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv.Texture2D.MipLevels = 1;
            srv.Texture2D.MostDetailedMip = 0;
            im.device->CreateShaderResourceView(view->image->res.Get(), &srv, im.SrvCpu(set->srvSlot));

            set->srvGpu = im.SrvGpu(set->srvSlot);
            set->samplerGpu = samp->gpu;
            set->kind = SetKind::Sampler;
        } else if (w.type == DescriptorType::UniformBuffer && w.bufferInfo.valid) {
            set->ubo = reinterpret_cast<BufferRec*>(w.bufferInfo.buffer.handle);
            set->kind = SetKind::UniformBuffer;
        }
    }
}

uint32_t D3D12Backend::RegisterBindlessTexture(const RHIDescriptorImageInfo& info) {
    Impl& im = *m_Impl;
    if (!im.caps.bindless) {
        XConsole::PrintError("D3D12: bindless not supported");
        return 0;
    }
    uint32_t index = im.NextBindlessIndex();
    UpdateBindlessTexture(index, info);
    return index;
}

void D3D12Backend::UpdateBindlessTexture(uint32_t index, const RHIDescriptorImageInfo& info) {
    Impl& im = *m_Impl;
    if (index >= Impl::kBindless) return;
    if (!info.valid || info.imageView.handle == 0 || info.sampler.handle == 0) return;

    ImageViewRec* view = reinterpret_cast<ImageViewRec*>(info.imageView.handle);
    SamplerRec* samp = reinterpret_cast<SamplerRec*>(info.sampler.handle);

    // Rewrite the SRV + sampler at the texture's own bindless slot in place —
    // no new descriptor is allocated, so resizing a render target never grows
    // the heaps (the old single-sampler path leaked an SRV per resize).
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = view->format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    srv.Texture2D.MostDetailedMip = 0;
    im.device->CreateShaderResourceView(view->image->res.Get(), &srv, im.SrvCpu(index));
    im.device->CreateSampler(&samp->desc, im.SamplerCpu(index));
}

void D3D12Backend::UnregisterBindlessTexture(uint32_t index) {
    Impl& im = *m_Impl;
    if (index >= Impl::kBindless) return;
    im.bindlessFree.push_back(index);
}

RHIDescriptorSet D3D12Backend::GetBindlessDescriptorSet() const {
    // The whole shader-visible heaps act as the bindless table: SRV table at
    // offset 0 (space1), sampler table at offset 0 (space2).
    Impl& im = *m_Impl;
    if (!im.bindlessSetRec) {
        im.bindlessSetRec = new DescSetRec();
        im.bindlessSetRec->kind = SetKind::Sampler;
        im.bindlessSetRec->srvGpu = im.SrvGpu(0);
        im.bindlessSetRec->samplerGpu = im.SamplerGpu(0);
    }
    RHIDescriptorSet out;
    out.handle = reinterpret_cast<uint64_t>(im.bindlessSetRec);
    return out;
}

uint32_t D3D12Backend::GetBindlessMaxTextures() const { return Impl::kBindless; }

RHIBuffer D3D12Backend::CreateBuffer(uint32_t size, BufferUsage usage,
    MemoryProperty properties, RHIDeviceMemory& memory) {
    Impl& im = *m_Impl;

    bool hostVisible = (static_cast<uint8_t>(properties) & static_cast<uint8_t>(MemoryProperty::HostVisible)) != 0;

    D3D12_HEAP_TYPE heapType = hostVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = (UINT64)size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    BufferRec* buf = new BufferRec();
    buf->size = size;
    D3D12_HEAP_PROPERTIES hp = HeapProps(heapType);
    HRESULT hr = im.device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE,
        &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf->res));
    if (FAILED(hr)) {
        XConsole::PrintError("D3D12: failed to create buffer ({}) hr=0x{:08X} reason=0x{:08X}",
            size, (unsigned)hr,
            (unsigned)(im.device->GetDeviceRemovedReason()));
        delete buf;
        return RHIBuffer{};
    }
    if (hostVisible) {
        D3D12_RANGE range{ 0, 0 };
        buf->res->Map(0, &range, &buf->mapped);
    }

    // Memory record shares the same resource (AddRef); both refs must be
    // released (DestroyBuffer + DestroyMemory) for the resource to die.
    MemoryRec* memRec = new MemoryRec();
    memRec->res = buf->res;
    memRec->mapped = buf->mapped;

    RHIBuffer out;
    out.handle = reinterpret_cast<uint64_t>(buf);
    memory.handle = reinterpret_cast<uint64_t>(memRec);
    (void)usage;
    return out;
}
void D3D12Backend::DestroyBuffer(RHIBuffer buffer) {
    delete reinterpret_cast<BufferRec*>(buffer.handle);
}
void D3D12Backend::DestroyMemory(RHIDeviceMemory memory) {
    delete reinterpret_cast<MemoryRec*>(memory.handle);
}
void D3D12Backend::CopyBuffer(RHIBuffer src, RHIBuffer dst, uint32_t size) {
    BufferRec* s = reinterpret_cast<BufferRec*>(src.handle);
    BufferRec* d = reinterpret_cast<BufferRec*>(dst.handle);
    m_Impl->ExecuteImmediate([&](ID3D12GraphicsCommandList* cl) {
        cl->CopyBufferRegion(d->res.Get(), 0, s->res.Get(), 0, size);
    });
}
bool D3D12Backend::MapMemory(RHIDeviceMemory memory, uint32_t offset,
    uint32_t size, void** data) {
    (void)size;
    MemoryRec* m = reinterpret_cast<MemoryRec*>(memory.handle);
    if (!m->mapped) return false;
    *data = static_cast<uint8_t*>(m->mapped) + offset;
    return true;
}
void D3D12Backend::UnmapMemory(RHIDeviceMemory memory) {
    (void)memory; // upload resources stay mapped for the lifetime of the buffer
}

RHIImage D3D12Backend::CreateImage(uint32_t width, uint32_t height, Format format,
    ImageUsage usage, MemoryProperty properties, RHIDeviceMemory& memory) {
    Impl& im = *m_Impl;
    (void)properties;

    ImageRec* img = new ImageRec();
    img->format = ToDxgi(format);
    img->width = width;
    img->height = height;
    img->isRT = (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::ColorAttachment)) != 0;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = width;
    rd.Height = height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = img->format;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::ColorAttachment))
        flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::DepthStencilAttachment))
        flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    rd.Flags = flags;

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (img->isRT) {
        // Render targets start in RENDER_TARGET (the RT pass clears + renders
        // immediately; EndRender moves it to PIXEL_SHADER_RESOURCE for sampling).
        initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    } else if (static_cast<uint8_t>(usage) & static_cast<uint8_t>(ImageUsage::DepthStencilAttachment)) {
        initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    } else {
        // Sampled textures are uploaded right after creation (COPY_DEST).
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    img->state = initialState;

    D3D12_HEAP_PROPERTIES hp = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = im.device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE,
        &rd, initialState, nullptr, IID_PPV_ARGS(&img->res));
    if (FAILED(hr)) {
        XConsole::PrintError("D3D12: failed to create image ({}x{})", width, height);
        delete img;
        return RHIImage{};
    }

    MemoryRec* memRec = new MemoryRec();
    memRec->res = img->res;
    memRec->mapped = nullptr;

    RHIImage out;
    out.handle = reinterpret_cast<uint64_t>(img);
    memory.handle = reinterpret_cast<uint64_t>(memRec);
    return out;
}
void D3D12Backend::DestroyImage(RHIImage image) {
    delete reinterpret_cast<ImageRec*>(image.handle);
}

RHIImageView D3D12Backend::CreateImageView(RHIImage image, Format format, Aspect aspect) {
    ImageRec* img = reinterpret_cast<ImageRec*>(image.handle);
    ImageViewRec* rec = new ImageViewRec();
    rec->image = img;
    rec->format = ToDxgi(format);

    if (static_cast<uint8_t>(aspect) & static_cast<uint8_t>(Aspect::Color)) {
        if (img->isRT) {
            rec->isRTV = true;
            rec->rtvSlot = m_Impl->AllocRtv();
            m_Impl->device->CreateRenderTargetView(img->res.Get(), nullptr, m_Impl->RtvCpu(rec->rtvSlot));
        }
    } else if (static_cast<uint8_t>(aspect) & static_cast<uint8_t>(Aspect::Depth)) {
        rec->isDSV = true;
        rec->dsvSlot = m_Impl->AllocDsv();
        m_Impl->device->CreateDepthStencilView(img->res.Get(), nullptr, m_Impl->DsvCpu(rec->dsvSlot));
    }

    RHIImageView out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyImageView(RHIImageView imageView) {
    ImageViewRec* rec = reinterpret_cast<ImageViewRec*>(imageView.handle);
    if (rec->isRTV) m_Impl->rtvFree[rec->rtvSlot] = false;
    if (rec->isDSV) m_Impl->dsvFree[rec->dsvSlot] = false;
    delete rec;
}

RHISampler D3D12Backend::CreateSampler(Filter filter, SamplerAddressMode addressMode) {
    D3D12_SAMPLER_DESC sd{};
    sd.Filter = filter == Filter::Nearest ? D3D12_FILTER_MIN_MAG_MIP_POINT
                                          : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = addressMode == SamplerAddressMode::ClampToEdge ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
                                                                 : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sd.AddressV = sd.AddressU;
    sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sd.MaxLOD = D3D12_FLOAT32_MAX;

    // No heap slot is allocated here: bindless registration re-creates the
    // sampler from `desc` at each texture's own bindless index (the old
    // per-(filter,address) cache slots are no longer referenced).
    SamplerRec* rec = new SamplerRec();
    rec->desc = sd;
    RHISampler out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroySampler(RHISampler sampler) {
    delete reinterpret_cast<SamplerRec*>(sampler.handle);
}

void D3D12Backend::TransitionImageLayout(RHIImage image, Format format,
    ImageLayout oldLayout, ImageLayout newLayout) {
    (void)format; (void)oldLayout;
    ImageRec* img = reinterpret_cast<ImageRec*>(image.handle);
    D3D12_RESOURCE_STATES after = ToState(newLayout);
    if (after == img->state) return;

    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = img->res.Get();
    b.Transition.StateBefore = img->state;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_Impl->ExecuteImmediate([&](ID3D12GraphicsCommandList* cl) {
        cl->ResourceBarrier(1, &b);
    });
    img->state = after;
}

void D3D12Backend::CopyBufferToImage(RHIBuffer buffer, RHIImage image,
    uint32_t width, uint32_t height) {
    BufferRec* buf = reinterpret_cast<BufferRec*>(buffer.handle);
    ImageRec* img = reinterpret_cast<ImageRec*>(image.handle);

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = img->res.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = buf->res.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Footprint.Width = width;
    src.PlacedFootprint.Footprint.Height = height;
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.Format = img->format;
    src.PlacedFootprint.Footprint.RowPitch = AlignUp(width * 4, 256);

    m_Impl->ExecuteImmediate([&](ID3D12GraphicsCommandList* cl) {
        cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    });
}

RHIRenderPass D3D12Backend::CreateRenderPass(const std::vector<Format>& colorFormats,
    Format depthFormat, bool overlay) {
    RenderPassRec* rec = new RenderPassRec();
    for (auto f : colorFormats) rec->colorFormats.push_back(ToDxgi(f));
    rec->hasDepth = depthFormat == Format::D32_SFLOAT;
    if (rec->hasDepth) rec->depthFormat = ToDxgi(depthFormat);
    rec->overlay = overlay;
    RHIRenderPass out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyRenderPass(RHIRenderPass renderPass) {
    delete reinterpret_cast<RenderPassRec*>(renderPass.handle);
}

RHIPassTemplate D3D12Backend::CreatePassTemplate(const RHIPassTemplateDesc& desc) {
    PassTemplateRec* rec = new PassTemplateRec();
    rec->clears = desc.clearValues;
    rec->viewport = { desc.viewport.x, desc.viewport.y, desc.viewport.width,
                      desc.viewport.height, desc.viewport.minDepth, desc.viewport.maxDepth };
    rec->scissor = { (LONG)desc.scissor.x, (LONG)desc.scissor.y,
                     (LONG)(desc.scissor.x + desc.scissor.width),
                     (LONG)(desc.scissor.y + desc.scissor.height) };
    RHIPassTemplate out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyPassTemplate(RHIPassTemplate passTemplate) {
    delete reinterpret_cast<PassTemplateRec*>(passTemplate.handle);
}

RHIFramebuffer D3D12Backend::CreateFramebuffer(RHIRenderPass renderPass,
    uint32_t width, uint32_t height, const std::vector<RHIImageView>& attachments) {
    FramebufferRec* rec = new FramebufferRec();
    rec->rp = reinterpret_cast<RenderPassRec*>(renderPass.handle);
    rec->width = width;
    rec->height = height;
    for (const auto& a : attachments) {
        ImageViewRec* iv = reinterpret_cast<ImageViewRec*>(a.handle);
        if (iv->isRTV) rec->colorAttachments.push_back(iv);
        if (iv->isDSV) rec->depthAttachment = iv;
    }
    RHIFramebuffer out;
    out.handle = reinterpret_cast<uint64_t>(rec);
    return out;
}
void D3D12Backend::DestroyFramebuffer(RHIFramebuffer framebuffer) {
    delete reinterpret_cast<FramebufferRec*>(framebuffer.handle);
}

// ---- Command recording ----

void D3D12Backend::CmdBeginRenderPass(RHICommandBuffer cmd, RHIPassTemplate passTemplate,
    RHIFramebuffer framebuffer) {
    (void)cmd;
    Impl& im = *m_Impl;
    FramebufferRec* fb = reinterpret_cast<FramebufferRec*>(framebuffer.handle);
    PassTemplateRec* tpl = reinterpret_cast<PassTemplateRec*>(passTemplate.handle);

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    for (auto* att : fb->colorAttachments)
        rtvs.push_back(im.RtvCpu(att->rtvSlot));

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
    bool hasDsv = fb->depthAttachment != nullptr;
    if (hasDsv) dsv = im.DsvCpu(fb->depthAttachment->dsvSlot);

    im.cmdList->OMSetRenderTargets((UINT)rtvs.size(),
        rtvs.empty() ? nullptr : rtvs.data(), FALSE, hasDsv ? &dsv : nullptr);

    for (size_t i = 0; i < rtvs.size() && i < tpl->clears.size(); ++i) {
        const auto& cv = tpl->clears[i];
        float c[4] = { cv.color.x, cv.color.y, cv.color.z, cv.color.w };
        im.cmdList->ClearRenderTargetView(rtvs[i], c, 0, nullptr);
    }
    if (hasDsv) {
        for (const auto& cv : tpl->clears) {
            if (cv.isDepth) {
                im.cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH,
                    cv.depth, (UINT8)cv.stencil, 0, nullptr);
                break;
            }
        }
    }

    im.cmdList->RSSetViewports(1, &tpl->viewport);
    im.cmdList->RSSetScissorRects(1, &tpl->scissor);
}
void D3D12Backend::CmdEndRenderPass(RHICommandBuffer cmd) {
    (void)cmd; // D3D12 has no render-pass objects; the RT is transitioned via CmdTransitionImageLayout
}

void D3D12Backend::CmdBindPipeline(RHICommandBuffer cmd, RHIPipeline pipeline) {
    (void)cmd;
    PipelineRec* rec = reinterpret_cast<PipelineRec*>(pipeline.handle);
    m_Impl->cmdList->SetPipelineState(rec->pso.Get());
    m_Impl->cmdList->IASetPrimitiveTopology(rec->primitiveTopology);
    m_Impl->currentPipeline = rec;
    // Root signature must be bound before any root-argument setters; binding it
    // here (only once per pipeline change) preserves root arguments (e.g. push
    // constants) written between the pipeline bind and the descriptor-set bind.
    m_Impl->cmdList->SetGraphicsRootSignature(rec->layout->rootSig.Get());
    m_Impl->currentRootSig = rec->layout->rootSig;
}

void D3D12Backend::CmdBindDescriptorSets(RHICommandBuffer cmd, RHIPipelineLayout layout,
    uint32_t firstSet, const std::vector<RHIDescriptorSet>& sets) {
    (void)cmd;
    Impl& im = *m_Impl;
    PipelineLayoutRec* pl = reinterpret_cast<PipelineLayoutRec*>(layout.handle);
    // Bind the root signature only when it changes. Re-binding the same root
    // signature invalidates previously set root arguments (including push
    // constants), which made UIRenderer's push-then-bind-descriptor-sets order
    // drop screenSize -> all quads collapsed -> empty window.
    if (pl->rootSig.Get() != im.currentRootSig.Get()) {
        im.cmdList->SetGraphicsRootSignature(pl->rootSig.Get());
        im.currentRootSig = pl->rootSig;
    }
    for (size_t i = 0; i < sets.size(); ++i) {
        uint32_t setIndex = firstSet + (uint32_t)i;
        DescSetRec* set = reinterpret_cast<DescSetRec*>(sets[i].handle);
        if (setIndex >= pl->setParams.size()) continue;
        const RootParamInfo& info = pl->setParams[setIndex];
        if (set->kind == SetKind::UniformBuffer) {
            if (set->ubo && set->ubo->res.Get())
                im.cmdList->SetGraphicsRootConstantBufferView(info.srvParam,
                    set->ubo->res->GetGPUVirtualAddress());
        } else {
            im.cmdList->SetGraphicsRootDescriptorTable(info.srvParam, set->srvGpu);
            im.cmdList->SetGraphicsRootDescriptorTable(info.samplerParam, set->samplerGpu);
        }
    }
}

void D3D12Backend::CmdBindVertexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    (void)cmd;
    BufferRec* buf = reinterpret_cast<BufferRec*>(buffer.handle);
    if (!m_Impl->currentPipeline || m_Impl->currentPipeline->vbStride == 0) return;
    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = buf->res->GetGPUVirtualAddress();
    view.SizeInBytes = buf->size;
    view.StrideInBytes = m_Impl->currentPipeline->vbStride;
    m_Impl->cmdList->IASetVertexBuffers(0, 1, &view);
}

void D3D12Backend::CmdBindIndexBuffer(RHICommandBuffer cmd, RHIBuffer buffer) {
    (void)cmd;
    BufferRec* buf = reinterpret_cast<BufferRec*>(buffer.handle);
    D3D12_INDEX_BUFFER_VIEW view{};
    view.BufferLocation = buf->res->GetGPUVirtualAddress();
    view.SizeInBytes = buf->size;
    view.Format = DXGI_FORMAT_R32_UINT;
    m_Impl->cmdList->IASetIndexBuffer(&view);
}

void D3D12Backend::CmdDraw(RHICommandBuffer cmd, uint32_t vertexCount, uint32_t firstVertex) {
    (void)cmd;
    m_Impl->cmdList->DrawInstanced(vertexCount, 1, firstVertex, 0);
}
void D3D12Backend::CmdDrawIndexed(RHICommandBuffer cmd, uint32_t indexCount,
    uint32_t instanceCount, uint32_t firstIndex) {
    (void)cmd;
    m_Impl->cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, 0, 0);
}

void D3D12Backend::CmdPushConstants(RHICommandBuffer cmd, RHIPipelineLayout layout,
    ShaderStageMask stage, uint32_t offset, uint32_t size, const void* data) {
    (void)cmd; (void)stage;
    PipelineLayoutRec* pl = reinterpret_cast<PipelineLayoutRec*>(layout.handle);
    if (pl->pushParam == UINT_MAX) return;
    m_Impl->cmdList->SetGraphicsRoot32BitConstants(pl->pushParam, size / 4, data, offset / 4);
}

void D3D12Backend::CmdSetViewport(RHICommandBuffer cmd, const RHIViewport& viewport) {
    (void)cmd;
    D3D12_VIEWPORT vp{};
    vp.TopLeftX = viewport.x;
    vp.TopLeftY = viewport.y;
    vp.Width = viewport.width;
    vp.Height = viewport.height;
    vp.MinDepth = viewport.minDepth;
    vp.MaxDepth = viewport.maxDepth;
    m_Impl->cmdList->RSSetViewports(1, &vp);
}
void D3D12Backend::CmdSetScissor(RHICommandBuffer cmd, const RHIRect2D& scissor) {
    (void)cmd;
    D3D12_RECT sc{ (LONG)scissor.x, (LONG)scissor.y,
        (LONG)scissor.x + (LONG)scissor.width, (LONG)scissor.y + (LONG)scissor.height };
    m_Impl->cmdList->RSSetScissorRects(1, &sc);
}
void D3D12Backend::CmdBarrier(RHICommandBuffer cmd) {
    (void)cmd;
}

void D3D12Backend::CmdTransitionImageLayout(RHICommandBuffer cmd, RHIImage image,
    Format format, ImageLayout oldLayout, ImageLayout newLayout, Aspect aspect) {
    (void)cmd; (void)format; (void)oldLayout; (void)aspect;
    Impl& im = *m_Impl;
    ImageRec* img = reinterpret_cast<ImageRec*>(image.handle);
    D3D12_RESOURCE_STATES after = ToState(newLayout);
    if (after == img->state) return;

    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = img->res.Get();
    b.Transition.StateBefore = img->state;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    im.cmdList->ResourceBarrier(1, &b);
    img->state = after;
}

} // namespace RHI
} // namespace Leir
