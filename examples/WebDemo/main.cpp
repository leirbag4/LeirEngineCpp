// LeirEngine Web Demo — Fase 6 / M1: raw RHI on Emscripten (WebGPU via
// emdawnwebgpu). Renders a rotating, lit, checkered cube using
// Basic.vert/frag.wgsl with the bindless texture table. Self-contained: it
// links the web-safe engine sources directly (WebGPUBackend + BackendFactory +
// XConsole), no engine library — that arrives in M2.
//
// Shaders are read from /shaders (mounted by --preload-file). The WebGPU
// surface is the "#canvas" element created by GLFW's Emscripten port.

#include <LeirEngine/Core/Log.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/RHI/WebGPUBackend.h>

#include <GLFW/glfw3.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace Leir;
using namespace Leir::RHI;

namespace {

// Push-constant struct mirroring Basic.vert/frag.wgsl (WGSL std430 layout:
// vec3 16-aligned, mat4 column-major). Struct size 144.
struct alignas(16) PushConstants {
    glm::vec3 lightDir;     float _pad0;     //  0..16
    glm::vec3 lightColor;   float _pad1;     // 16..32
    glm::vec3 ambientColor; float _pad2;     // 32..48
    glm::vec4 color;                         // 48..64
    glm::mat4 model;                         // 64..128
    uint32_t textureIndex;                   // 128..132
    float _pad3;                             // 132..144
};
static_assert(sizeof(PushConstants) == 144, "push size must match the WGSL struct");

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// Procedural unit cube (centered, -0.5..0.5), per-face normals + UVs, CCW
// front faces (front-face is CCW in every backend).
void BuildCube(std::vector<Vertex>& verts, std::vector<uint32_t>& indices) {
    const glm::vec3 faceNormals[6] = {
        { 1, 0, 0}, {-1, 0, 0}, {0,  1, 0}, {0, -1, 0}, {0, 0,  1}, {0, 0, -1},
    };
    const glm::vec3 faceU[6] = {
        {0, 0, -1}, {0, 0, 1}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {-1, 0, 0},
    };
    const glm::vec3 faceV[6] = {
        {0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0},
    };
    for (int f = 0; f < 6; ++f) {
        const glm::vec3 c = faceNormals[f] * 0.5f;
        const uint32_t base = static_cast<uint32_t>(verts.size());
        const glm::vec3 corners[4] = {
            c - faceU[f] * 0.5f - faceV[f] * 0.5f,
            c + faceU[f] * 0.5f - faceV[f] * 0.5f,
            c + faceU[f] * 0.5f + faceV[f] * 0.5f,
            c - faceU[f] * 0.5f + faceV[f] * 0.5f,
        };
        const glm::vec2 uvs[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
        for (int i = 0; i < 4; ++i)
            verts.push_back({ corners[i], faceNormals[f], uvs[i] });
        indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
}

std::vector<char> ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        XConsole::PrintError("WebDemo: cannot read {}", path);
        return {};
    }
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

} // namespace

class WebDemo {
public:
    bool Init(GLFWwindow* window) {
        glfwGetFramebufferSize(window, &m_Width, &m_Height);
        if (m_Width <= 0 || m_Height <= 0) { m_Width = 800; m_Height = 600; }

        m_Backend.reset(BackendFactory::Create(
            "webgpu", window, m_Width, m_Height, false, "LeirEngine Web Demo"));
        if (!m_Backend) {
            XConsole::PrintError("WebDemo: failed to create the WebGPU backend");
            return false;
        }
        XConsole::Println("WebDemo: backend ready ({:d}x{:d})", m_Width, m_Height);

        // ---- Shaders (preloaded to /shaders via --preload-file) ----
        m_Vs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.vert.wgsl"));
#if defined(__EMSCRIPTEN__)
        // Firefox's naga cannot compile binding_array, so the browser build
        // uses the single-texture variant (the backend binds the first
        // registered texture into the shared group).
        m_Fs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.web.frag.wgsl"));
#else
        m_Fs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.frag.wgsl"));
#endif
        if (!m_Vs.IsValid() || !m_Fs.IsValid()) {
            XConsole::PrintError("WebDemo: shader module compilation failed");
            return false;
        }

        // ---- Descriptor layouts ----
        RHIDescriptorBinding uboBinding;
        uboBinding.binding = 0;
        uboBinding.type = DescriptorType::UniformBuffer;
        uboBinding.count = 1;
        uboBinding.stage = ShaderStage::Vertex;
        m_UboLayout = m_Backend->CreateDescriptorSetLayout({ uboBinding });

        RHIDescriptorBinding texBinding;
        texBinding.binding = 0;
        texBinding.type = DescriptorType::CombinedImageSampler;
        texBinding.count = kBindless;
        texBinding.stage = ShaderStage::Fragment;
        texBinding.bindless = true;
        RHIDescriptorBinding samBinding;
        samBinding.binding = 1;
        samBinding.type = DescriptorType::CombinedImageSampler;
        samBinding.count = kBindless;
        samBinding.stage = ShaderStage::Fragment;
        samBinding.bindless = true;
        m_BindlessLayout = m_Backend->CreateDescriptorSetLayout({ texBinding, samBinding });
        if (!m_UboLayout.IsValid() || !m_BindlessLayout.IsValid()) return false;

        RHIPushConstantRange pushRange;
        pushRange.stage = ShaderStageMask::VertexFragment;
        pushRange.offset = 0;
        pushRange.size = sizeof(PushConstants);
        m_Layout = m_Backend->CreatePipelineLayout({ m_UboLayout, m_BindlessLayout }, { pushRange });
        if (!m_Layout.IsValid()) return false;

        // ---- Pipeline ----
        RHIShaderStageInfo vsInfo;
        vsInfo.stage = ShaderStage::Vertex;
        vsInfo.module = m_Vs;
        RHIShaderStageInfo fsInfo;
        fsInfo.stage = ShaderStage::Fragment;
        fsInfo.module = m_Fs;

        RHIPipelineDesc pd;
        pd.layout = m_Layout;
        pd.renderPass = m_Backend->GetRenderPass();
        pd.stages = { vsInfo, fsInfo };
        pd.vertexBinding.stride = sizeof(Vertex);
        pd.vertexAttributes = {
            { 0, 0, Format::R32G32B32_SFLOAT, 0 },
            { 1, 0, Format::R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal)) },
            { 2, 0, Format::R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, uv)) },
        };
        pd.topology = Topology::TriangleList;
        pd.cullMode = CullMode::Back;
        pd.depthTestEnable = true;
        m_Pipeline = m_Backend->CreateGraphicsPipeline(pd);
        if (!m_Pipeline.IsValid()) return false;

        // ---- Camera UBO (mat4 view-projection) ----
        m_Ubo = m_Backend->CreateBuffer(64, BufferUsage::Uniform,
            MemoryProperty::HostVisible, m_UboMemory);
        if (!m_Ubo.IsValid()) return false;
        m_UboPool = m_Backend->CreateDescriptorPool({ uboBinding }, 1);
        m_UboSet = m_Backend->AllocateDescriptorSet(m_UboPool, m_UboLayout);
        RHIDescriptorWrite uboWrite;
        uboWrite.dstSet = m_UboSet;
        uboWrite.dstBinding = 0;
        uboWrite.count = 1;
        uboWrite.type = DescriptorType::UniformBuffer;
        uboWrite.bufferInfo = { m_Ubo, 0, 64, true };
        m_Backend->WriteDescriptorSets({ uboWrite });

        // ---- Geometry ----
        std::vector<Vertex> verts;
        std::vector<uint32_t> indices;
        BuildCube(verts, indices);
        m_VertexCount = static_cast<uint32_t>(indices.size());
        m_VertBuf = CreateUploadBuffer(verts.data(),
            static_cast<uint32_t>(verts.size() * sizeof(Vertex)),
            BufferUsage::Vertex, m_VertMemory);
        m_IndexBuf = CreateUploadBuffer(indices.data(),
            static_cast<uint32_t>(indices.size() * sizeof(uint32_t)),
            BufferUsage::Index, m_IndexMemory);
        if (!m_VertBuf.IsValid() || !m_IndexBuf.IsValid()) return false;

        // ---- Checker texture -> bindless slot 0 ----
        if (!CreateCheckerTexture()) return false;

        // ---- Static push data ----
        m_Push.lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        m_Push.lightColor = glm::vec3(1.0f, 0.95f, 0.9f) * 1.2f;
        m_Push.ambientColor = glm::vec3(0.25f, 0.25f, 0.3f);
        m_Push.color = glm::vec4(1.0f);
        m_Push.textureIndex = m_TexIndex;

        XConsole::Println("WebDemo: initialized (cube, {:d} indices)", m_VertexCount);
        return true;
    }

    void Frame() {
        if (!m_Backend) return;
        glfwPollEvents();

        const double t = glfwGetTime();

        // Auto-orbit camera looking at the origin.
        const float yaw = static_cast<float>(t) * 0.5f;
        const float pitch = 0.35f;
        const float dist = 3.0f;
        const glm::vec3 camPos(dist * std::cos(pitch) * std::sin(yaw),
            dist * std::sin(pitch),
            dist * std::cos(pitch) * std::cos(yaw));
        const glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 proj = glm::perspective(glm::radians(60.0f),
            static_cast<float>(m_Width) / static_cast<float>(m_Height), 0.1f, 100.0f);
        m_ViewProjection = proj * view;

        m_Push.model = glm::rotate(glm::mat4(1.0f), static_cast<float>(t) * 0.6f,
            glm::normalize(glm::vec3(0.6f, 1.0f, 0.2f)));

        if (!m_Backend->BeginFrame(false)) return;
        const RHICommandBuffer cmd = m_Backend->GetCurrentCommandBuffer();

        void* ptr = nullptr;
        if (m_Backend->MapMemory(m_UboMemory, 0, 64, &ptr)) {
            std::memcpy(ptr, &m_ViewProjection, sizeof(glm::mat4));
            m_Backend->UnmapMemory(m_UboMemory);
        }

        m_Graph.Clear();
        m_Graph.BindPipeline(m_Pipeline);
        m_Graph.BindDescriptorSets(m_Layout, 0, { m_UboSet });
        m_Graph.BindDescriptorSets(m_Layout, 1, { m_Backend->GetBindlessDescriptorSet() });
        m_Graph.BindVertexBuffer(m_VertBuf);
        m_Graph.BindIndexBuffer(m_IndexBuf);
        m_Graph.PushConstants(m_Layout, ShaderStageMask::VertexFragment, 0,
            sizeof(PushConstants), &m_Push);
        m_Graph.SetViewport({ 0.0f, 0.0f, static_cast<float>(m_Width),
            static_cast<float>(m_Height), 0.0f, 1.0f });
        m_Graph.SetScissor({ 0, 0, static_cast<uint32_t>(m_Width),
            static_cast<uint32_t>(m_Height) });
        m_Graph.SetSampledTextures({ m_TexIndex });
        m_Graph.DrawIndexed(m_VertexCount, 1, 0);

        m_Backend->CmdExecuteGraph(cmd, m_Graph);
        m_Backend->EndFrame();
    }

    ~WebDemo() {
        if (!m_Backend) return;
        m_Backend->WaitIdle();
        m_Backend->DestroyPipeline(m_Pipeline);
        m_Backend->DestroyPipelineLayout(m_Layout);
        m_Backend->DestroyDescriptorSetLayout(m_BindlessLayout);
        m_Backend->DestroyDescriptorSetLayout(m_UboLayout);
        m_Backend->DestroyDescriptorPool(m_UboPool);
        m_Backend->UnregisterBindlessTexture(m_TexIndex);
        m_Backend->DestroySampler(m_Sampler);
        m_Backend->DestroyImageView(m_TexView);
        m_Backend->DestroyImage(m_TexImage);
        m_Backend->DestroyMemory(m_TexMemory);
        m_Backend->DestroyBuffer(m_Staging);
        m_Backend->DestroyMemory(m_StagingMemory);
        m_Backend->DestroyBuffer(m_IndexBuf);
        m_Backend->DestroyMemory(m_IndexMemory);
        m_Backend->DestroyBuffer(m_VertBuf);
        m_Backend->DestroyMemory(m_VertMemory);
        m_Backend->DestroyBuffer(m_Ubo);
        m_Backend->DestroyMemory(m_UboMemory);
        m_Backend->DestroyShaderModule(m_Fs);
        m_Backend->DestroyShaderModule(m_Vs);
        BackendFactory::Destroy(m_Backend.release());
    }

private:
    static constexpr uint32_t kBindless = 16;

    RHIBuffer CreateUploadBuffer(const void* data, uint32_t size, BufferUsage usage,
        RHIDeviceMemory& memOut) {
        RHIBuffer buf = m_Backend->CreateBuffer(size, usage,
            MemoryProperty::HostVisible, memOut);
        if (!buf.IsValid()) return buf;
        void* ptr = nullptr;
        if (m_Backend->MapMemory(memOut, 0, size, &ptr)) {
            std::memcpy(ptr, data, size);
            m_Backend->UnmapMemory(memOut);
        }
        return buf;
    }

    bool CreateCheckerTexture() {
        static const uint8_t kPixels[4][4] = {
            { 255, 255, 255, 255 }, { 128, 128, 128, 255 },
            { 128, 128, 128, 255 }, { 255, 255, 255, 255 },
        };
        const uint32_t kBytesPerRow = 256; // WebGPU buffer-texture row pitch
        const uint32_t kStagingSize = kBytesPerRow * 2;

        m_Staging = m_Backend->CreateBuffer(kStagingSize, BufferUsage::TransferSrc,
            MemoryProperty::HostVisible, m_StagingMemory);
        if (!m_Staging.IsValid()) return false;
        void* ptr = nullptr;
        if (m_Backend->MapMemory(m_StagingMemory, 0, kStagingSize, &ptr)) {
            std::memcpy(ptr, kPixels, sizeof(kPixels));
            m_Backend->UnmapMemory(m_StagingMemory);
        }

        m_TexImage = m_Backend->CreateImage(2, 2, Format::R8G8B8A8_SRGB,
            ImageUsage::TransferDst | ImageUsage::Sampled,
            MemoryProperty::DeviceLocal, m_TexMemory);
        if (!m_TexImage.IsValid()) return false;

        m_Backend->CopyBufferToImage(m_Staging, m_TexImage, 2, 2);

        m_TexView = m_Backend->CreateImageView(m_TexImage, Format::R8G8B8A8_SRGB, Aspect::Color);
        if (!m_TexView.IsValid()) return false;
        m_Sampler = m_Backend->CreateSampler(Filter::Linear, SamplerAddressMode::Repeat);
        if (!m_Sampler.IsValid()) return false;

        RHIDescriptorImageInfo info;
        info.imageView = m_TexView;
        info.sampler = m_Sampler;
        info.image = m_TexImage;
        info.valid = true;
        m_TexIndex = m_Backend->RegisterBindlessTexture(info);
        return true;
    }

    int m_Width = 0;
    int m_Height = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_TexIndex = 0;

    std::unique_ptr<RenderBackend> m_Backend;
    GCommandGraph m_Graph;

    RHIShaderModule m_Vs, m_Fs;
    RHIPipelineLayout m_Layout;
    RHIPipeline m_Pipeline;
    RHIDescriptorSetLayout m_UboLayout, m_BindlessLayout;
    RHIDescriptorPool m_UboPool;
    RHIDescriptorSet m_UboSet;
    RHIBuffer m_Ubo;      RHIDeviceMemory m_UboMemory;
    RHIBuffer m_VertBuf;  RHIDeviceMemory m_VertMemory;
    RHIBuffer m_IndexBuf; RHIDeviceMemory m_IndexMemory;
    RHIBuffer m_Staging;  RHIDeviceMemory m_StagingMemory;
    RHIImage m_TexImage;  RHIDeviceMemory m_TexMemory;
    RHIImageView m_TexView;
    RHISampler m_Sampler;

    glm::mat4 m_ViewProjection = glm::mat4(1.0f);
    PushConstants m_Push{};
};

#if defined(__EMSCRIPTEN__)
void FrameCallback(void* userData) {
    static_cast<WebDemo*>(userData)->Frame();
}
#endif

int main() {
    XConsole::SetLevel(LogLevel::Info);
    XConsole::Println("LeirEngine Web Demo starting (M1, raw RHI)");

    if (!glfwInit()) {
        XConsole::PrintError("WebDemo: glfwInit failed");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "LeirEngine Web Demo", nullptr, nullptr);
    if (!window) {
        XConsole::PrintError("WebDemo: glfwCreateWindow failed");
        return 1;
    }

    WebDemo demo;
    bool ok = false;
    try {
        ok = demo.Init(window);
    } catch (const std::exception& e) {
        XConsole::PrintError("WebDemo: init failed: {}", e.what());
    }
    if (!ok) return 1;

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(FrameCallback, &demo, 0, 1);
#else
    while (!glfwWindowShouldClose(window)) {
        demo.Frame();
    }
#endif
    return 0;
}