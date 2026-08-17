#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/Log.h>
#include <LeirEngine/Math/Quaternion.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
#include <LeirEngine/Rendering/RenderTexture.h>
#include <LeirEngine/Rendering/Shader.h>
#include <LeirEngine/Rendering/Mesh.h>
#include <LeirEngine/Rendering/Material.h>
#include <LeirEngine/Rendering/Texture2D.h>
#include <LeirEngine/Components/MeshRenderer.h>
#include <LeirEngine/Components/Camera.h>
#include <LeirEngine/Components/Light.h>
#include <LeirEngine/Physics/PhysicsWorld.h>
#include <LeirEngine/Physics/RigidBody.h>
#include <LeirEngine/Physics/Collider.h>
#include <LeirEngine/UI/UICanvas.h>
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIViewportPanel.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/UI/UIRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

std::shared_ptr<Leir::Texture2D> MakeCheckerTexture(Leir::RHI::RenderBackend* backend,
    int size, const std::array<uint8_t, 3>& light, const std::array<uint8_t, 3>& dark)
{
    const int cells = 8;
    const int cellPx = size / cells;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const bool isLight = ((x / cellPx) + (y / cellPx)) % 2 == 0;
            const auto& c = isLight ? light : dark;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            pixels[i + 0] = c[0];
            pixels[i + 1] = c[1];
            pixels[i + 2] = c[2];
            pixels[i + 3] = 255;
        }
    }
    return std::make_shared<Leir::Texture2D>(backend,
        (uint32_t)size, (uint32_t)size, pixels.data());
}

constexpr float kBottomBarHeight = 30.0f;
constexpr float kDegToRad = 3.14159265358979f / 180.0f;

} // namespace

class WebEngineDemoApp : public Leir::CoreApplication {
public:
    WebEngineDemoApp()
        : CoreApplication("LeirEngine Web Demo (Fase 6 / M3)", 1280, 720)
    {
    }

    ~WebEngineDemoApp() override
    {
        if (m_Backend)
            m_Backend->WaitIdle();
    }

protected:
    void OnInit() override
    {
        Leir::XConsole::Println("WebEngineDemo: OnInit");
        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            "webgpu", GetWindow(), GetWidth(), GetHeight(), true, "LeirEngine Web Demo"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("WebEngineDemo: failed to create webgpu backend");
            return;
        }

        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_Backend.get(),
            shaderDir + "/Basic.vert" + m_Backend->GetShaderFileExtension(),
            shaderDir + "/Basic.frag" + m_Backend->GetShaderFileExtension());

        m_CheckerA = MakeCheckerTexture(m_Backend.get(), 256, {230, 230, 240}, {120, 120, 140});
        m_CheckerB = MakeCheckerTexture(m_Backend.get(), 256, {205, 85, 70}, {80, 28, 24});
        m_CheckerFloor = MakeCheckerTexture(m_Backend.get(), 256, {95, 95, 110}, {55, 55, 65});
        m_MaterialA = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_MaterialA->SetTexture("texSampler", m_CheckerA);
        m_MaterialB = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_MaterialB->SetTexture("texSampler", m_CheckerB);
        m_MaterialFloor = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_MaterialFloor->SetTexture("texSampler", m_CheckerFloor);

        auto [verts, idxs] = Leir::Primitives::CreateCube();
        m_Mesh = std::make_shared<Leir::Mesh>(m_Backend.get(), verts, idxs);
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_Backend.get());

        auto& sceneManager = Leir::SceneManager::GetInstance();
        auto& scene = sceneManager.CreateScene("Web Scene");
        sceneManager.SetActiveScene(&scene);

        m_ViewportW = (uint32_t)std::max(1.0f, (float)GetWidth());
        m_ViewportH = (uint32_t)std::max(1.0f, (float)GetHeight() - kBottomBarHeight);

        float dpr = GetContentScale();
        m_ViewportRT = std::make_unique<Leir::RenderTexture>(
            m_Backend.get(),
            (uint32_t)std::max(1.0f, (float)std::lround(m_ViewportW * dpr)),
            (uint32_t)std::max(1.0f, (float)std::lround(m_ViewportH * dpr)));
        m_MaterialA->RecreatePipeline(m_ViewportRT->GetRenderPass());
        m_MaterialB->RecreatePipeline(m_ViewportRT->GetRenderPass());
        m_MaterialFloor->RecreatePipeline(m_ViewportRT->GetRenderPass());

        Leir::PhysicsWorld::GetInstance().Init();

        auto* cameraObj = scene.CreateObject3D("Camera");
        auto& camera = cameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)m_ViewportW / (float)m_ViewportH, 0.1f, 100.0f);
        camera.SetPrimary(true);
        m_CameraObj = cameraObj;

        auto* lightObj = scene.CreateObject3D("Light");
        lightObj->GetTransform().SetLocalPosition({2.0f, 4.0f, -2.0f});
        lightObj->GetTransform().SetLocalRotation(
            Leir::Quaternion::Euler(-45.0f, 30.0f, 0.0f));
        auto& light = lightObj->AddComponent<Leir::Light>();
        light.SetType(Leir::LightType::Directional);
        light.SetColor({1.0f, 0.95f, 0.9f});
        light.SetIntensity(1.5f);

        m_Floor = scene.CreateObject3D("Floor");
        m_Floor->GetTransform().SetLocalPosition({0.0f, -1.5f, 0.0f});
        m_Floor->GetTransform().SetLocalScale({12.0f, 1.0f, 12.0f});
        auto& floorMesh = m_Floor->AddComponent<Leir::MeshRenderer>();
        floorMesh.SetMesh(m_Mesh);
        floorMesh.SetMaterial(m_MaterialFloor);
        m_Floor->AddComponent<Leir::Collider>().SetBox({6.0f, 0.5f, 6.0f});
        m_Floor->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Static);

        m_CubeA = scene.CreateObject3D("CubeA");
        m_CubeA->GetTransform().SetLocalPosition({-1.5f, 2.0f, 0.0f});
        auto& ra = m_CubeA->AddComponent<Leir::MeshRenderer>();
        ra.SetMesh(m_Mesh);
        ra.SetMaterial(m_MaterialA);
        m_CubeA->AddComponent<Leir::Collider>().SetBox({0.5f, 0.5f, 0.5f});
        m_CubeA->AddComponent<Leir::RigidBody>();

        m_CubeB = scene.CreateObject3D("CubeB");
        m_CubeB->GetTransform().SetLocalPosition({1.5f, 3.2f, 0.0f});
        auto& rb = m_CubeB->AddComponent<Leir::MeshRenderer>();
        rb.SetMesh(m_Mesh);
        rb.SetMaterial(m_MaterialB);
        m_CubeB->AddComponent<Leir::Collider>().SetBox({0.5f, 0.5f, 0.5f});
        m_CubeB->AddComponent<Leir::RigidBody>();

        m_UIRenderer = std::make_unique<Leir::UIRenderer>(m_Backend.get());
        m_UIRenderer->SetContentScale(GetContentScale());

        m_Font = std::make_unique<Leir::Font>(m_Backend.get(), "/assets/Roboto-Regular.ttf", 16, dpr);
        m_FontSmall = std::make_unique<Leir::Font>(m_Backend.get(), "/assets/Roboto-Regular.ttf", 13, dpr);

        m_Canvas = std::make_unique<Leir::UICanvas>();
        m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
        m_Canvas->ConnectToInputSystem();

        m_ViewportPanel = new Leir::UIViewportPanel();
        m_ViewportPanel->SetName("Viewport");
        m_ViewportPanel->SetRenderTexture(m_ViewportRT.get());
        m_ViewportPanel->GetRect().anchor = Leir::AnchorSet::Stretch();
        m_ViewportPanel->GetRect().offset = {0.0f, 0.0f, 0.0f, -kBottomBarHeight};
        m_Canvas->AddChild(m_ViewportPanel);

        auto* bottomBar = new Leir::UIPanel();
        bottomBar->SetName("BottomBar");
        bottomBar->GetRect().anchor = {0.0f, 1.0f, 1.0f, 1.0f};
        bottomBar->GetRect().offset = {0.0f, -kBottomBarHeight, 0.0f, 0.0f};
        bottomBar->SetColor({0.1f, 0.1f, 0.12f, 1.0f});
        m_Canvas->AddChild(bottomBar);

        auto* title = new Leir::UILabel();
        title->SetName("Title");
        title->SetText("LeirEngine Web - Fase 6 / M3 (Scene + Camera + Light + Jolt Physics + UI + Font)");
        title->SetFont(m_FontSmall.get());
        title->SetColor({0.55f, 0.85f, 0.65f, 1.0f});
        title->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        title->GetRect().offset = {8.0f, -28.0f, 720.0f, 0.0f};
        m_Canvas->AddChild(title);

        m_Canvas->UpdateLayout();

        Leir::XConsole::Println("WebEngineDemo: scene + UI ready ({}x{}, dpr {:.2f})",
            m_ViewportW, m_ViewportH, dpr);
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        m_OrbitYaw += deltaTime * 20.0f;
        UpdateCamera();

        if (m_Canvas) {
            m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
            m_Canvas->UpdateLayout();
        }
        UpdateViewportRenderTarget();
    }

    void OnRender() override
    {
        if (!m_Backend || !m_Backend->BeginFrame(true)) return;

        auto cmd = m_Backend->GetCurrentCommandBuffer();
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();

        if (m_ViewportRT && scene) {
            Leir::RHI::RHIClearValue clearColor;
            clearColor.color = {0.13f, 0.15f, 0.2f, 1.0f};
            m_SceneGraph.Clear();
            m_ViewportRT->BeginRender(m_SceneGraph, clearColor, 1.0f);
            m_RenderPipeline->Render(m_SceneGraph, scene);
            m_ViewportRT->EndRender(m_SceneGraph);
            m_Backend->CmdExecuteGraph(cmd, m_SceneGraph);
        }

        m_Backend->BeginSwapchainOverlay();
        if (m_UIRenderer && m_Canvas) {
            m_UIGraph.Clear();
            m_UIRenderer->Render(m_UIGraph, m_Canvas.get());
            m_Backend->CmdExecuteGraph(cmd, m_UIGraph);
        }
        m_Backend->EndFrame();
    }

    void OnShutdown() override
    {
        Leir::XConsole::Println("WebEngineDemo: shutting down");
        if (m_Canvas && m_ViewportPanel) {
            m_Canvas->RemoveChild(m_ViewportPanel);
            delete m_ViewportPanel;
            m_ViewportPanel = nullptr;
        }
        m_ViewportRT.reset();
        auto& sm = Leir::SceneManager::GetInstance();
        sm.DestroyScene("Web Scene");
        sm.SetActiveScene(nullptr);
    }

    void OnWindowResized(int width, int height) override
    {
        (void)width;
        (void)height;
        if (m_Backend)
            m_Backend->NotifyResized();
    }

    void OnContentScaleChanged() override
    {
        if (m_UIRenderer)
            m_UIRenderer->SetContentScale(GetContentScale());
        if (m_Font) m_Font->SetContentScale(GetContentScale());
        if (m_FontSmall) m_FontSmall->SetContentScale(GetContentScale());
    }

private:
    void UpdateCamera()
    {
        const float pitchRad = m_OrbitPitch * kDegToRad;
        const float yawRad = m_OrbitYaw * kDegToRad;
        const float dist = 5.0f;
        const float cp = std::cos(pitchRad);
        Leir::Vector3 pos(
            dist * cp * std::sin(yawRad),
            -dist * std::sin(pitchRad),
            dist * cp * std::cos(yawRad));
        if (m_CameraObj) {
            m_CameraObj->GetTransform().SetLocalPosition(pos);
            m_CameraObj->GetTransform().SetLocalRotation(
                Leir::Quaternion::Euler(m_OrbitPitch, m_OrbitYaw, 0.0f));
        }
    }

    void UpdateViewportRenderTarget()
    {
        if (!m_ViewportRT || !m_ViewportPanel) return;
        const auto& cr = m_ViewportPanel->GetComputedRect();
        uint32_t w = (uint32_t)std::max(1.0f, cr.z);
        uint32_t h = (uint32_t)std::max(1.0f, cr.w);
        float dpr = GetContentScale();
        uint32_t fw = (uint32_t)std::max(1.0f, (float)std::lround(w * dpr));
        uint32_t fh = (uint32_t)std::max(1.0f, (float)std::lround(h * dpr));
        if (fw == m_ViewportRT->GetWidth() && fh == m_ViewportRT->GetHeight()) return;
        m_ViewportRT->Resize(fw, fh);
        if (m_CameraObj) {
            if (auto* cam = m_CameraObj->GetComponent<Leir::Camera>())
                cam->SetPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
        }
    }

    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;
    Leir::RHI::GCommandGraph m_SceneGraph;
    Leir::RHI::GCommandGraph m_UIGraph;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_Mesh;
    std::shared_ptr<Leir::Texture2D> m_CheckerA;
    std::shared_ptr<Leir::Texture2D> m_CheckerB;
    std::shared_ptr<Leir::Texture2D> m_CheckerFloor;
    std::shared_ptr<Leir::Material> m_MaterialA;
    std::shared_ptr<Leir::Material> m_MaterialB;
    std::shared_ptr<Leir::Material> m_MaterialFloor;
    std::unique_ptr<Leir::RenderTexture> m_ViewportRT;
    Leir::Object3D* m_CameraObj = nullptr;
    Leir::Object3D* m_Floor = nullptr;
    Leir::Object3D* m_CubeA = nullptr;
    Leir::Object3D* m_CubeB = nullptr;
    std::unique_ptr<Leir::UIRenderer> m_UIRenderer;
    std::unique_ptr<Leir::UICanvas> m_Canvas;
    std::unique_ptr<Leir::Font> m_Font;
    std::unique_ptr<Leir::Font> m_FontSmall;
    Leir::UIViewportPanel* m_ViewportPanel = nullptr;
    uint32_t m_ViewportW = 0;
    uint32_t m_ViewportH = 0;
    float m_OrbitYaw = 0.0f;
    float m_OrbitPitch = -25.0f;
};

int main()
{
    Leir::XConsole::SetLevel(Leir::LogLevel::Trace);
    WebEngineDemoApp app;
    app.Run();
    return 0;
}