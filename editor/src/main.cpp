#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/Settings.h>
#include <LeirEngine/Core/CoreObject.h>
#include <vector>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Rendering/VulkanDevice.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
#include <LeirEngine/Rendering/RenderTexture.h>
#include <LeirEngine/Rendering/Shader.h>
#include <LeirEngine/Rendering/Mesh.h>
#include <LeirEngine/Rendering/Material.h>
#include <LeirEngine/Rendering/Texture2D.h>
#include <LeirEngine/Rendering/SpriteSheet.h>
#include <LeirEngine/Rendering/Image.h>
#include <LeirEngine/Components/MeshRenderer.h>
#include <LeirEngine/Components/SpriteRenderer.h>
#include <LeirEngine/Components/Camera.h>
#include <LeirEngine/Components/Light.h>

#include <LeirEngine/UI/UICanvas.h>
#include <LeirEngine/UI/UIImage.h>
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UISlider.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/ScrollView.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/UI/UIRenderer.h>
#include <LeirEngine/UI/UIViewportPanel.h>
#include <LeirEngine/UI/UIDebugOverlay.h>
#include "UI/UITestPanel.h"
#include "UI/CameraTestPanel.h"
#include "UI/DebugTextPanel.h"
#include "UI/TextAreaDebugPanel.h"
#include "UI/InspectorTransformPanel.h"
#include "Camera/EditorCamera.h"

#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>

#include <spdlog/spdlog.h>

#include <memory>
#include <algorithm>

namespace {
    const float kHierarchyWidth = 264.0f;
    const float kInspectorWidth = 290.0f;
    const float kBottomBarHeight = 30.0f;
    const float kDebugPanelMargin = 10.0f;
    const float kDebugPanelWidth = 280.0f;
}

class EditorApp : public Leir::CoreApplication {
public:
    EditorApp()
        : CoreApplication("LeirEngine Editor",
              Leir::LeirSettings::Get().window.width,
              Leir::LeirSettings::Get().window.height,
              Leir::LeirSettings::Get().window.fullscreen)
    {
    }

    ~EditorApp()
    {
        if (m_VulkanDevice)
            vkDeviceWaitIdle(m_VulkanDevice->GetDevice());
    }

protected:
    void OnInit() override
    {
        spdlog::info("Editor initialized");

        Leir::VulkanDeviceConfig config;
        config.appName = "LeirEngine Editor";
        config.windowWidth = GetWidth();
        config.windowHeight = GetHeight();
        m_VulkanDevice = std::make_unique<Leir::VulkanDevice>(GetWindow(), config);

        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_VulkanDevice.get(),
            shaderDir + "/Basic.vert.spv",
            shaderDir + "/Basic.frag.spv"
        );

        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(
            m_VulkanDevice.get(), 1, 1, whitePixel);

        m_Material = std::make_shared<Leir::Material>(m_VulkanDevice.get(), m_Shader);
        m_Material->SetTexture("texSampler", m_WhiteTexture);

        auto [verts, idxs] = Leir::Primitives::CreateCube();
        m_Mesh = std::make_shared<Leir::Mesh>(m_VulkanDevice.get(), verts, idxs);
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_VulkanDevice.get());

        auto& sceneManager = Leir::SceneManager::GetInstance();
        auto& scene = sceneManager.CreateScene("Main Scene");
        sceneManager.SetActiveScene(&scene);

        // Viewport size: real area between Hierarchy and Inspector, minus bottom bar
        m_ViewportW = (uint32_t)(GetWidth() - (int)kHierarchyWidth - (int)kInspectorWidth);
        m_ViewportH = (uint32_t)(GetHeight() - (int)kBottomBarHeight);

        // Create RenderTexture for the viewport
        m_ViewportRT = std::make_unique<Leir::RenderTexture>(
            m_VulkanDevice.get(), m_ViewportW, m_ViewportH);
        m_Material->RecreatePipeline(m_ViewportRT->GetRenderPass());

        // Camera (will be driven by EditorCamera)
        auto* cameraObj = scene.CreateObject3D("Camera");
        auto& camera = cameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)m_ViewportW / (float)m_ViewportH, 0.1f, 100.0f);
        camera.SetPrimary(true);

        // Light
        auto* lightObj = scene.CreateObject3D("Light");
        lightObj->GetTransform().SetLocalPosition({2.0f, 4.0f, -2.0f});
        lightObj->GetTransform().SetLocalRotation(
            Leir::Quaternion::Euler(-45.0f, 30.0f, 0.0f));
        auto& light = lightObj->AddComponent<Leir::Light>();
        light.SetType(Leir::LightType::Directional);
        light.SetColor({1.0f, 0.95f, 0.9f});
        light.SetIntensity(1.5f);

        // Cube
        auto* cubeObj = scene.CreateObject3D("Cube");
        cubeObj->GetTransform().SetLocalPosition({0.0f, 0.0f, 0.0f});
        auto& renderer = cubeObj->AddComponent<Leir::MeshRenderer>();
        renderer.SetMesh(m_Mesh);
        renderer.SetMaterial(m_Material);

        Leir::Object3D* child = scene.CreateObject3D("Child");
        child->GetTransform().SetLocalPosition({2.0f, 1.0f, 0.0f});
        child->SetParent(cubeObj);

        // Sprites
        auto* spriteObj = scene.CreateObject2D("TestSprite");
        spriteObj->GetTransform().SetLocalPosition(
            {GetWidth() * 0.5f, GetHeight() * 0.5f, 0.0f});
        spriteObj->GetTransform().SetLocalScale({200.0f, 200.0f, 1.0f});
        auto& spr = spriteObj->AddComponent<Leir::SpriteRenderer>();
        spr.SetColor({0.0f, 1.0f, 1.0f, 1.0f});

        auto* spriteTex = scene.CreateObject2D("TexSprite");
        spriteTex->GetTransform().SetLocalPosition({100.0f, 100.0f, 0.0f});
        spriteTex->GetTransform().SetLocalScale({100.0f, 100.0f, 1.0f});
        auto& sprTex = spriteTex->AddComponent<Leir::SpriteRenderer>();
        sprTex.SetTexture(m_WhiteTexture.get());
        sprTex.SetColor({1.0f, 0.0f, 0.0f, 1.0f});

        Leir::Image sheetImage("assets/sprite_sheet_64_64.png");
        auto sheetTex = std::make_shared<Leir::Texture2D>(m_VulkanDevice.get(), sheetImage);
        auto sheet = std::make_shared<Leir::SpriteSheet>(sheetTex.get(), 32, 32);

        auto* sheetSprite = scene.CreateObject2D("SheetSprite");
        sheetSprite->GetTransform().SetLocalPosition({GetWidth() * 0.75f, GetHeight() * 0.25f, 0.0f});
        sheetSprite->GetTransform().SetLocalScale({100.0f, 100.0f, 1.0f});
        auto& sSpr = sheetSprite->AddComponent<Leir::SpriteRenderer>();
        sSpr.SetSpriteSheet(sheet.get());
        sSpr.SetFrameIndex(0);
        sSpr.SetColor({1.0f, 1.0f, 1.0f, 1.0f});

        m_SheetTexture = sheetTex;
        m_SpriteSheet = sheet;
        m_SheetSprites.push_back(sheetSprite);

        // ---- UI System ----
        m_UIRenderer = std::make_unique<Leir::UIRenderer>(m_VulkanDevice.get());

        std::string fontPath;
        FILE* testFont = nullptr;
        if ((testFont = fopen("C:/Windows/Fonts/Arial.ttf", "rb")) != nullptr) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/Arial.ttf";
        } else if ((testFont = fopen("C:/Windows/Fonts/segoeui.ttf", "rb")) != nullptr) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/segoeui.ttf";
        } else if ((testFont = fopen("C:/Windows/Fonts/consola.ttf", "rb")) != nullptr) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/consola.ttf";
        } else {
            spdlog::warn("No system font found, text will not render");
        }

        if (!fontPath.empty()) {
            m_Font = std::make_unique<Leir::Font>(m_VulkanDevice.get(), fontPath, 16);
            m_FontSmall = std::make_unique<Leir::Font>(m_VulkanDevice.get(), fontPath, 13);
        }

        m_Canvas = std::make_unique<Leir::UICanvas>();
        m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
        m_Canvas->ConnectToInputSystem();

        // ---- Editor Layout ----
        // Root editor panel (full screen)
        auto* root = new Leir::UIPanel();
        root->SetName("EditorRoot");
        root->SetColor({0.12f, 0.12f, 0.14f, 1.0f});
        root->GetRect().anchor = Leir::AnchorSet::Stretch();
        root->GetRect().offset = {};
        m_Canvas->AddChild(root);

        // Viewport panel (center-right area)
        m_ViewportPanel = new Leir::UIViewportPanel();
        m_ViewportPanel->SetName("Viewport");
        m_ViewportPanel->SetRenderTexture(m_ViewportRT.get());
        m_ViewportPanel->GetRect().anchor = {0.0f, 0.0f, 1.0f, 1.0f};
        m_ViewportPanel->GetRect().offset = {kHierarchyWidth, 0.0f, -kInspectorWidth, -kBottomBarHeight};
        root->AddChild(m_ViewportPanel);

        // Hierarchy panel (left)
        auto* hierarchy = new Leir::UIPanel();
        hierarchy->SetName("Hierarchy");
        hierarchy->SetColor({0.16f, 0.16f, 0.18f, 1.0f});
        hierarchy->GetRect().anchor = {0.0f, 0.0f, 0.0f, 1.0f};
        hierarchy->GetRect().offset = {0.0f, 0.0f, kHierarchyWidth, -kBottomBarHeight};
        hierarchy->SetLayoutMode(Leir::LayoutMode::Column);
        hierarchy->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
        hierarchy->SetSpacing(2.0f);
        root->AddChild(hierarchy);

        auto* hierarchyTitle = new Leir::UILabel();
        hierarchyTitle->SetName("HierarchyTitle");
        hierarchyTitle->SetText("-- Hierarchy --");
        hierarchyTitle->SetFont(m_FontSmall.get());
        hierarchyTitle->SetColor({0.7f, 0.7f, 0.7f, 1.0f});
        hierarchyTitle->SetSizePolicy(Leir::SizePolicy::Fixed);
        hierarchy->AddChild(hierarchyTitle);

        // Inspector panel (right)
        auto* inspector = new Leir::UIPanel();
        inspector->SetName("Inspector");
        inspector->SetColor({0.16f, 0.16f, 0.18f, 1.0f});
        inspector->GetRect().anchor = {1.0f, 0.0f, 1.0f, 1.0f};
        inspector->GetRect().offset = {-kInspectorWidth, 0.0f, 0.0f, -kBottomBarHeight};
        inspector->SetLayoutMode(Leir::LayoutMode::Column);
        inspector->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
        inspector->SetSpacing(2.0f);
        root->AddChild(inspector);

        auto* inspectorTitle = new Leir::UILabel();
        inspectorTitle->SetName("InspectorTitle");
        inspectorTitle->SetText("-- Inspector --");
        inspectorTitle->SetFont(m_FontSmall.get());
        inspectorTitle->SetColor({0.7f, 0.7f, 0.7f, 1.0f});
        inspectorTitle->SetSizePolicy(Leir::SizePolicy::Fixed);
        inspector->AddChild(inspectorTitle);

        // Transform panel (inside Inspector)
        m_InspectorTransformPanel = new InspectorTransformPanel();
        m_InspectorTransformPanel->SetName("InspectorTransformPanel");
        m_InspectorTransformPanel->SetSizePolicy(Leir::SizePolicy::Content);
        m_InspectorTransformPanel->SetFont(m_FontSmall.get());
        inspector->AddChild(m_InspectorTransformPanel);

        m_InspectorTransformPanel->SetTargetObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Cube")));

        // Bottom bar
        auto* bottomBar = new Leir::UIImage();
        bottomBar->SetName("BottomBar");
        bottomBar->GetRect().anchor = {0.0f, 1.0f, 1.0f, 1.0f};
        bottomBar->GetRect().offset = {0.0f, -30.0f, 0.0f, 0.0f};
        bottomBar->SetColor({0.1f, 0.1f, 0.12f, 1.0f});
        root->AddChild(bottomBar);

        auto* statusLabel = new Leir::UILabel();
        statusLabel->SetName("StatusLabel");
        statusLabel->SetText("Editor Online | Click der. + WASD para camara libre");
        statusLabel->SetFont(m_FontSmall.get());
        statusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
        statusLabel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        statusLabel->GetRect().offset = {8.0f, -28.0f, 600.0f, 0.0f};
        root->AddChild(statusLabel);

        m_Canvas->UpdateLayout();

        m_DebugOverlay = std::make_unique<Leir::UIDebugOverlay>(m_Font.get(), m_Canvas.get());

        // Test panel (bottom-left inside viewport)
        m_TestPanel = new UITestPanel();
        m_TestPanel->SetName("DebugTestPanel");
        m_TestPanel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        m_TestPanel->GetRect().offset = {kHierarchyWidth + kDebugPanelMargin, -260.0f, kHierarchyWidth + kDebugPanelMargin + kDebugPanelWidth, -30.0f};
        m_TestPanel->SetFont(m_FontSmall.get());
        root->AddChild(m_TestPanel);

        m_TestPanel->SetTargetObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Cube")));

        // Camera Test panel (bottom-right inside viewport)
        m_CameraTestPanel = new CameraTestPanel();
        m_CameraTestPanel->SetName("DebugCameraPanel");
        m_CameraTestPanel->GetRect().anchor = {1.0f, 1.0f, 1.0f, 1.0f};
        m_CameraTestPanel->GetRect().offset = {-(kInspectorWidth + kDebugPanelMargin) - kDebugPanelWidth, -200.0f, -(kInspectorWidth + kDebugPanelMargin), -30.0f};
        m_CameraTestPanel->SetFont(m_FontSmall.get());
        root->AddChild(m_CameraTestPanel);

        m_CameraTestPanel->SetCameraObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Camera")));

        // TextArea Debug Panel (top-right, above CameraTestPanel)
        m_TextAreaDebugPanel = new TextAreaDebugPanel();
        m_TextAreaDebugPanel->SetName("DebugTextAreaPanel");
        m_TextAreaDebugPanel->GetRect().anchor = {1.0f, 1.0f, 1.0f, 1.0f};
        m_TextAreaDebugPanel->GetRect().offset = {-(kInspectorWidth + kDebugPanelMargin) - kDebugPanelWidth, -400.0f, -(kInspectorWidth + kDebugPanelMargin), -210.0f};
        m_TextAreaDebugPanel->SetFont(m_FontSmall.get());
        root->AddChild(m_TextAreaDebugPanel);

        // Debug Text Panel (on the left side of viewport, below TestPanel)
        m_DebugTextPanel = new DebugTextPanel();
        m_DebugTextPanel->SetName("DebugTextPanel");
        m_DebugTextPanel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        m_DebugTextPanel->GetRect().offset = {kHierarchyWidth + kDebugPanelMargin, -430.0f, kHierarchyWidth + kDebugPanelMargin + kDebugPanelWidth, -270.0f};
        m_DebugTextPanel->SetFont(m_FontSmall.get());
        root->AddChild(m_DebugTextPanel);

        // Sync scene camera from EditorCamera initial position
        auto* camObj = scene.FindObjectByName("Camera");
        if (camObj) {
            camObj->GetTransform().SetLocalPosition(m_EditorCamera.GetPosition());
            camObj->GetTransform().SetLocalRotation(m_EditorCamera.GetRotation());
        }

        spdlog::info("Scene hierarchy created with viewport system");
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // Editor camera controls (right-click + WASD free-fly)
        auto* hovered = m_Canvas->GetHoveredElement();
        bool inViewport = m_ViewportPanel && hovered &&
            (hovered == m_ViewportPanel || hovered->GetParent() == m_ViewportPanel);
        bool rightDown = inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Right);
        bool middleDown = inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Middle);

        // Update EditorCamera state
        if (rightDown || middleDown)
            m_EditorCamera.Update(deltaTime);

        bool cameraControlled = rightDown || middleDown;

        // Bidirectional sync: EditorCamera ↔ scene camera
        auto* cameraObj = scene->FindObjectByName("Camera");
        if (cameraObj) {
            if (cameraControlled) {
                // EditorCamera → escena (durante control)
                cameraObj->GetTransform().SetLocalPosition(m_EditorCamera.GetPosition());
                cameraObj->GetTransform().SetLocalRotation(m_EditorCamera.GetRotation());
            } else {
                // escena → EditorCamera (panel edits)
                auto& t = cameraObj->GetTransform();
                auto pos = t.GetLocalPosition();
                auto euler = Leir::Quaternion::ToEuler(t.GetLocalRotation());
                m_EditorCamera.SetPosition(pos);
                m_EditorCamera.SetYaw(euler.y);
                m_EditorCamera.SetPitch(euler.x);
            }
        }

        // Update UI layout on resize
        if (m_Canvas) {
            m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
            m_Canvas->UpdateLayout();
        }

        // Keep the viewport render target in sync with the actual layout size
        UpdateViewportRenderTarget();

        if (m_DebugOverlay)
            m_DebugOverlay->Update(deltaTime);

        if (m_TestPanel)
            m_TestPanel->Refresh();
        if (m_CameraTestPanel)
            m_CameraTestPanel->Refresh();
        if (m_DebugTextPanel)
            m_DebugTextPanel->Refresh();
        if (m_TextAreaDebugPanel)
            m_TextAreaDebugPanel->Refresh();
        if (m_InspectorTransformPanel)
            m_InspectorTransformPanel->Refresh();
    }

    void OnRender() override
    {
        // BeginFrame(true) skips the swapchain 3D render pass
        if (!m_VulkanDevice || !m_VulkanDevice->BeginFrame(true)) return;

        VkCommandBuffer cmd = m_VulkanDevice->GetCurrentCommandBuffer();
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();

        // 1. Render 3D scene + sprites to offscreen RenderTexture
        if (m_ViewportRT && scene) {
            VkClearValue clearColor;
            clearColor.color = { {0.15f, 0.15f, 0.2f, 1.0f} };
            m_ViewportRT->BeginRender(cmd, clearColor, 1.0f);
            m_RenderPipeline->Render(cmd, scene);
            m_ViewportRT->EndRender(cmd);
        }

        // 2. Render UI to swapchain overlay directly
        m_VulkanDevice->BeginSwapchainOverlay();
        if (m_UIRenderer && m_Canvas)
            m_UIRenderer->Render(cmd, m_Canvas.get());
        m_VulkanDevice->EndFrame();
    }

    void OnShutdown() override
    {
        spdlog::info("Editor shutting down");
        // Destroy viewport RT before VulkanDevice
        m_ViewportRT.reset();
        auto& sm = Leir::SceneManager::GetInstance();
        sm.DestroyScene("Main Scene");
        sm.SetActiveScene(nullptr);
    }

    void OnWindowResized(int width, int height) override
    {
        (void)width;
        (void)height;
        // Notify VulkanDevice so the swapchain is recreated at next present
        if (m_VulkanDevice)
            m_VulkanDevice->NotifyResized();
    }

private:
    void UpdateViewportRenderTarget()
    {
        if (!m_ViewportRT || !m_ViewportPanel)
            return;

        const auto& cr = m_ViewportPanel->GetComputedRect();
        uint32_t w = (uint32_t)std::max(1.0f, cr.z);
        uint32_t h = (uint32_t)std::max(1.0f, cr.w);

        if (w == m_ViewportRT->GetWidth() && h == m_ViewportRT->GetHeight())
            return;

        m_ViewportRT->Resize(w, h);
        if (m_UIRenderer)
            m_UIRenderer->InvalidateViewportDescriptor(m_ViewportRT.get());

        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        auto* cameraObj = scene ? scene->FindObjectByName("Camera") : nullptr;
        if (cameraObj) {
            if (auto* cam = cameraObj->GetComponent<Leir::Camera>())
                cam->SetPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
        }

        spdlog::info("Viewport resized to {}x{}", w, h);
    }

    std::unique_ptr<Leir::VulkanDevice> m_VulkanDevice;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_Mesh;
    std::shared_ptr<Leir::Material> m_Material;
    std::shared_ptr<Leir::Texture2D> m_WhiteTexture;
    std::shared_ptr<Leir::Texture2D> m_SheetTexture;
    std::shared_ptr<Leir::SpriteSheet> m_SpriteSheet;
    std::vector<Leir::Object2D*> m_SheetSprites;

    std::unique_ptr<Leir::UIRenderer> m_UIRenderer;
    std::unique_ptr<Leir::UICanvas> m_Canvas;
    std::unique_ptr<Leir::Font> m_Font;
    std::unique_ptr<Leir::Font> m_FontSmall;
    std::unique_ptr<Leir::UIDebugOverlay> m_DebugOverlay;

    // Viewport system
    std::unique_ptr<Leir::RenderTexture> m_ViewportRT;
    Leir::UIViewportPanel* m_ViewportPanel = nullptr;
    EditorCamera m_EditorCamera;

    UITestPanel* m_TestPanel = nullptr;
    CameraTestPanel* m_CameraTestPanel = nullptr;
    DebugTextPanel* m_DebugTextPanel = nullptr;
    TextAreaDebugPanel* m_TextAreaDebugPanel = nullptr;
    InspectorTransformPanel* m_InspectorTransformPanel = nullptr;
    uint32_t m_ViewportW = 800;
    uint32_t m_ViewportH = 600;
};

int main()
{
    spdlog::set_level(spdlog::level::trace);

    Leir::LeirSettings::Get().Load();

    EditorApp app;
    app.Run();

    return 0;
}
