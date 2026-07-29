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

#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <spdlog/spdlog.h>

#include <memory>

struct EditorCamera {
    float yaw = 0.0f;
    float pitch = -20.0f;
    float distance = 8.0f;
    glm::vec3 target = {0.0f, 0.0f, 0.0f};

    glm::vec3 GetPosition() const {
        float r = distance * cos(glm::radians(pitch));
        float y = distance * sin(glm::radians(pitch));
        float x = r * sin(glm::radians(yaw));
        float z = r * cos(glm::radians(yaw));
        return target + glm::vec3(x, y, z);
    }

    void Orbit(float dx, float dy) {
        yaw += dx * 0.5f;
        pitch = glm::clamp(pitch + dy * 0.5f, -89.0f, 89.0f);
    }

    void Zoom(float delta) {
        distance = glm::clamp(distance - delta * 2.0f, 1.0f, 50.0f);
    }

    void Pan(float dx, float dy) {
        glm::vec3 forward = glm::normalize(target - GetPosition());
        glm::vec3 right = glm::normalize(glm::cross(forward, {0,1,0}));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        float speed = distance * 0.005f;
        target += right * dx * speed + up * dy * speed;
    }
};

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

        // Viewport size (80% of window width, full height minus bottom bar)
        m_ViewportW = (uint32_t)(GetWidth() * 0.78f);
        m_ViewportH = (uint32_t)(GetHeight() - 30);

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
            glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f)));
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
        root->GetRect() = Leir::Rect2D::Absolute(0, 0, (float)GetWidth(), (float)GetHeight());
        m_Canvas->AddChild(root);

        // Viewport panel (center-right area)
        m_ViewportPanel = new Leir::UIViewportPanel();
        m_ViewportPanel->SetName("Viewport");
        m_ViewportPanel->SetRenderTexture(m_ViewportRT.get());
        m_ViewportPanel->GetRect().anchor = {0.0f, 0.0f, 1.0f, 1.0f};
        m_ViewportPanel->GetRect().offset = {200.0f, 0.0f, -220.0f, -30.0f};
        root->AddChild(m_ViewportPanel);

        // Hierarchy panel (left)
        auto* hierarchy = new Leir::UIPanel();
        hierarchy->SetName("Hierarchy");
        hierarchy->SetColor({0.16f, 0.16f, 0.18f, 1.0f});
        hierarchy->GetRect().anchor = {0.0f, 0.0f, 0.0f, 1.0f};
        hierarchy->GetRect().offset = {0.0f, 0.0f, 200.0f, -30.0f};
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
        inspector->GetRect().offset = {-220.0f, 0.0f, 0.0f, -30.0f};
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

        // Bottom bar
        auto* bottomBar = new Leir::UIImage();
        bottomBar->SetName("BottomBar");
        bottomBar->GetRect().anchor = {0.0f, 1.0f, 1.0f, 1.0f};
        bottomBar->GetRect().offset = {0.0f, -30.0f, 0.0f, 0.0f};
        bottomBar->SetColor({0.1f, 0.1f, 0.12f, 1.0f});
        root->AddChild(bottomBar);

        auto* statusLabel = new Leir::UILabel();
        statusLabel->SetName("StatusLabel");
        statusLabel->SetText("Editor Online | Arrastra para orbitar, scroll para zoom");
        statusLabel->SetFont(m_FontSmall.get());
        statusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
        statusLabel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        statusLabel->GetRect().offset = {8.0f, -28.0f, 600.0f, 0.0f};
        root->AddChild(statusLabel);

        m_Canvas->UpdateLayout();

        m_DebugOverlay = std::make_unique<Leir::UIDebugOverlay>(m_Font.get(), m_Canvas.get());

        // Test panel (floating over the viewport, centered)
        m_TestPanel = new UITestPanel();
        m_TestPanel->SetName("DebugTestPanel");
        m_TestPanel->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
        m_TestPanel->GetRect().offset = {-150.0f, -125.0f, 150.0f, 125.0f};
        m_TestPanel->SetFont(m_FontSmall.get());
        root->AddChild(m_TestPanel);

        m_TestPanel->SetTargetObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Cube")));
        m_TestPanel->SetCameraObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Camera")));

        spdlog::info("Scene hierarchy created with viewport system");
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // Editor camera controls (only when not interacting with UI)
        auto* hovered = m_Canvas->GetHoveredElement();
        bool inViewport = m_ViewportPanel && hovered &&
            (hovered == m_ViewportPanel || hovered->GetParent() == m_ViewportPanel);

        // Orbit: left mouse drag in viewport
        if (inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Left)) {
            auto delta = Leir::Mouse::GetDelta();
            m_EditorCamera.Orbit(delta.x, -delta.y);
        }

        // Pan: middle mouse drag in viewport
        if (inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Middle)) {
            auto delta = Leir::Mouse::GetDelta();
            m_EditorCamera.Pan(-delta.x, -delta.y);
        }

        // Zoom: scroll in viewport
        if (inViewport) {
            float scroll = Leir::Mouse::GetScrollDelta();
            if (scroll != 0.0f)
                m_EditorCamera.Zoom(scroll);
        }

        // Apply editor camera to scene camera
        auto* cameraObj = scene->FindObjectByName("Camera");
        if (cameraObj) {
            auto pos = m_EditorCamera.GetPosition();
            glm::vec3 forward = glm::normalize(m_EditorCamera.target - pos);
            glm::quat rot = glm::quatLookAt(forward, glm::vec3(0, 1, 0));
            cameraObj->GetTransform().SetLocalPosition(pos);
            cameraObj->GetTransform().SetLocalRotation(rot);
        }

        // Remove old rotation animation (the cube rotation was for demo;
        // commented out so camera controls feel natural)
        // auto* cube = scene->FindObjectByName("Cube");
        // if (cube) { ... }

        // Update UI layout on resize
        if (m_Canvas) {
            m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
            m_Canvas->UpdateLayout();
        }

        if (m_DebugOverlay)
            m_DebugOverlay->Update(deltaTime);

        if (m_TestPanel)
            m_TestPanel->Refresh();
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

private:
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
