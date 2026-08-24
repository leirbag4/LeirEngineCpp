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
        // Single-source: the generated *.web.wgsl (LEIR_BINDLESS=0, vs_main/
        // ps_main) are committed in engine/shaders and match the web backend's
        // single-texture layout + entry points. The old hand-written
        // Basic.vert.wgsl was removed during the single-source migration.
#if defined(__EMSCRIPTEN__)
        m_Vs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.vert.web.wgsl"));
        // Firefox's naga cannot compile binding_array, so the browser build
        // uses the single-texture variant (the backend binds the first
        // registered texture into the shared group).
        m_Fs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.frag.web.wgsl"));
#else
        m_Vs = m_Backend->CreateShaderModule(ReadFile("/shaders/Basic.vert.wgsl"));
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

        // ---- Three textures -> three bindless slots (multi-texture proof) ----
        static const uint8_t kChecker[2][2][4] = {
            { {255,255,255,255}, {128,128,128,255} },
            { {128,128,128,255}, {255,255,255,255} },
        };
        static const uint8_t kRed[2][2][4] = {
            { {255,255,255,255}, {200, 40, 40,255} },
            { {200, 40, 40,255}, {255,255,255,255} },
        };
        static const uint8_t kBlue[2][2][4] = {
            { {255,255,255,255}, { 40, 80,200,255} },
            { { 40, 80,200,255}, {255,255,255,255} },
        };
        m_Tex.resize(3);
        if (!CreateTextureFromPixels(reinterpret_cast<const uint8_t*>(kChecker), 2, 2, m_Tex[0]) ||
            !CreateTextureFromPixels(reinterpret_cast<const uint8_t*>(kRed), 2, 2, m_Tex[1]) ||
            !CreateTextureFromPixels(reinterpret_cast<const uint8_t*>(kBlue), 2, 2, m_Tex[2]))
            return false;
        XConsole::Println("WebDemo: 3 textures registered (slots {:d}, {:d}, {:d})",
            m_Tex[0].index, m_Tex[1].index, m_Tex[2].index);

        // ---- Static push data ----
        m_Push.lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        m_Push.lightColor = glm::vec3(1.0f, 0.95f, 0.9f) * 1.2f;
        m_Push.ambientColor = glm::vec3(0.25f, 0.25f, 0.3f);
        m_Push.color = glm::vec4(1.0f);
        m_Push.textureIndex = m_Tex[0].index;

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
        const float dist = 3.6f;
        const glm::vec3 camPos(dist * std::cos(pitch) * std::sin(yaw),
            dist * std::sin(pitch),
            dist * std::cos(pitch) * std::cos(yaw));
        const glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 proj = glm::perspective(glm::radians(60.0f),
            static_cast<float>(m_Width) / static_cast<float>(m_Height), 0.1f, 100.0f);
        m_ViewProjection = proj * view;

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
        m_Graph.SetViewport({ 0.0f, 0.0f, static_cast<float>(m_Width),
            static_cast<float>(m_Height), 0.0f, 1.0f });
        m_Graph.SetScissor({ 0, 0, static_cast<uint32_t>(m_Width),
            static_cast<uint32_t>(m_Height) });

        // Three cubes, one texture each. The web executor binds the per-draw
        // texture group (single-texture layout), so each cube must sample its
        // own checker despite sharing one pipeline.
        for (int i = 0; i < 3; ++i) {
            m_Push.model = glm::translate(glm::mat4(1.0f),
                glm::vec3(static_cast<float>(i - 1) * 1.2f, 0.0f, 0.0f))
                * glm::rotate(glm::mat4(1.0f), static_cast<float>(t) * (0.6f + 0.15f * i),
                    glm::normalize(glm::vec3(0.6f, 1.0f, 0.2f)));
            m_Push.textureIndex = m_Tex[i].index;
            m_Graph.PushConstants(m_Layout, ShaderStageMask::VertexFragment, 0,
                sizeof(PushConstants), &m_Push);
            m_Graph.SetSampledTextures({ m_Tex[i].index });
            m_Graph.DrawIndexed(m_VertexCount, 1, 0);
        }

        {
            static bool logged = false;
            if (!logged) {
                logged = true;
                XConsole::Println("WebDemo: recording {:d} draw records",
                    static_cast<int>(m_Graph.GetRecords().size()));
            }
        }

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
        for (const TexRes& tex : m_Tex) {
            m_Backend->UnregisterBindlessTexture(tex.index);
            m_Backend->DestroySampler(tex.sampler);
            m_Backend->DestroyImageView(tex.view);
            m_Backend->DestroyImage(tex.image);
            m_Backend->DestroyMemory(tex.memory);
            m_Backend->DestroyBuffer(tex.staging);
            m_Backend->DestroyMemory(tex.stagingMemory);
        }
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

    struct TexRes {
        RHIBuffer staging;       RHIDeviceMemory stagingMemory;
        RHIImage image;          RHIDeviceMemory memory;
        RHIImageView view;
        RHISampler sampler;
        uint32_t index = 0;
    };

    bool CreateTextureFromPixels(const uint8_t* pixels, uint32_t w, uint32_t h,
                                 TexRes& out) {
        const uint32_t kBytesPerRow = (w * 4 + 255u) & ~255u; // WebGPU pitch
        const uint32_t kStagingSize = kBytesPerRow * h;

        out.staging = m_Backend->CreateBuffer(kStagingSize, BufferUsage::TransferSrc,
            MemoryProperty::HostVisible, out.stagingMemory);
        if (!out.staging.IsValid()) return false;
        void* ptr = nullptr;
        if (m_Backend->MapMemory(out.stagingMemory, 0, kStagingSize, &ptr)) {
            uint8_t* dst = static_cast<uint8_t*>(ptr);
            for (uint32_t r = 0; r < h; ++r)
                std::memcpy(dst + r * kBytesPerRow, pixels + r * w * 4, w * 4);
            m_Backend->UnmapMemory(out.stagingMemory);
        }

        out.image = m_Backend->CreateImage(w, h, Format::R8G8B8A8_SRGB,
            ImageUsage::TransferDst | ImageUsage::Sampled,
            MemoryProperty::DeviceLocal, out.memory);
        if (!out.image.IsValid()) return false;

        m_Backend->CopyBufferToImage(out.staging, out.image, w, h);

        out.view = m_Backend->CreateImageView(out.image, Format::R8G8B8A8_SRGB, Aspect::Color);
        if (!out.view.IsValid()) return false;
        out.sampler = m_Backend->CreateSampler(Filter::Linear, SamplerAddressMode::Repeat);
        if (!out.sampler.IsValid()) return false;

        RHIDescriptorImageInfo info;
        info.imageView = out.view;
        info.sampler = out.sampler;
        info.image = out.image;
        info.valid = true;
        out.index = m_Backend->RegisterBindlessTexture(info);
        return true;
    }

    int m_Width = 0;
    int m_Height = 0;
    uint32_t m_VertexCount = 0;
    std::vector<TexRes> m_Tex;

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