#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/Settings.h>
#include <LeirEngine/Core/CoreObject.h>
#include <vector>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/RHI/VulkanBackend.h>
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
#include <LeirEngine/UI/Dock/DockManager.h>
#include "UI/UITestPanel.h"
#include "UI/CameraTestPanel.h"
#include "UI/DebugTextPanel.h"
#include "UI/TextAreaDebugPanel.h"
#include "UI/TextAreaWrapPanel.h"
#include "UI/ConsolePanel.h"
#include "UI/DebugPanel.h"
#include "UI/InspectorTransformPanel.h"
#include "Camera/EditorCamera.h"

#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>

#include "LeirEngine/Core/Log.h"

#include <memory>
#include <algorithm>
#include <chrono>
#include <exception>
#include <typeinfo>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "CrashDiagnostics.h"

namespace {
    const float kBottomBarHeight = 30.0f;
}

// Forward decl (mutually recursive helpers).
void DeleteUiSubtree(Leir::UIElement* element);

// Recursively freed contents of a composite widget that owns its direct
// children (ScrollView/UITextArea): their OWN descendants (e.g. the content
// under a ScrollView's viewport) are still editor-owned and must be freed here,
// while the owned children themselves are left for the widget's destructor.
void DeleteNonOwnedSubtree(Leir::UIElement* node)
{
    if (!node)
        return;
    auto grandchildren = node->GetChildren();
    for (auto* g : grandchildren) {
        if (node->OwnsChild(g))
            DeleteNonOwnedSubtree(g);
        else {
            node->RemoveChild(g);
            DeleteUiSubtree(g);
        }
    }
}

// UIElement's dtor only nulls child parent pointers; it does not free children.
// The editor owns the dock content subtrees, so they are freed here recursively.
// Children a widget deletes in its own destructor (ScrollView's viewport/
// scrollbars, UIScrollbar's thumb, ...) are skipped â€” deleting them again would
// be a double free (crash 0xC0000005 in LeirEngine.dll â†’ 5s shutdown).
void DeleteUiSubtree(Leir::UIElement* element)
{
    if (!element)
        return;
    auto children = element->GetChildren();
    for (auto* c : children) {
        if (element->OwnsChild(c))
            DeleteNonOwnedSubtree(c);
        else {
            element->RemoveChild(c);
            DeleteUiSubtree(c);
        }
    }
    delete element;
}

// ---- Crash diagnostics ----
// All crash/failure reporting lives in CrashDiagnostics.h/.cpp (Windows:
// terminate handler + invalid-parameter handler + >512MB alloc hook with a
// DbgHelp stack walk; macOS/Linux skeletons ready to extend). main() only
// calls CrashDiagnostics::Init() once at startup.

class EditorApp : public Leir::CoreApplication {
public:
    EditorApp()
        : CoreApplication("LeirEngine Editor",
              Leir::LeirSettings::Get().window.width,
              Leir::LeirSettings::Get().window.height,
              Leir::LeirSettings::Get().window.fullscreen,
              Leir::LeirSettings::Get().window.pos_x,
              Leir::LeirSettings::Get().window.pos_y,
              Leir::LeirSettings::Get().window.maximized,
              Leir::LeirSettings::Get().window.hidpi)
    {
    }

    ~EditorApp()
    {
        if (m_Backend)
            m_Backend->WaitIdle();
    }

protected:
    void OnInit() override
    {
        Leir::XConsole::Println("Editor initialized");

        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            Leir::LeirSettings::Get().graphics.backend,
            GetWindow(), GetWidth(), GetHeight(),
            Leir::LeirSettings::Get().window.vsync, "LeirEngine Editor"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("Failed to create render backend");
            return;
        }

        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_Backend.get(),
            shaderDir + "/Basic.vert" + m_Backend->GetShaderFileExtension(),
            shaderDir + "/Basic.frag" + m_Backend->GetShaderFileExtension()
        );

        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(
            m_Backend.get(), 1, 1, whitePixel);

        m_Material = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_Material->SetTexture("texSampler", m_WhiteTexture);

        auto [verts, idxs] = Leir::Primitives::CreateCube();
        m_Mesh = std::make_shared<Leir::Mesh>(m_Backend.get(), verts, idxs);
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_Backend.get());

        auto& sceneManager = Leir::SceneManager::GetInstance();
        auto& scene = sceneManager.CreateScene("Main Scene");
        sceneManager.SetActiveScene(&scene);

        // Initial viewport size (refined each frame from the dock layout)
        m_ViewportW = (uint32_t)std::max(1.0f, GetWidth() - 600.0f);
        m_ViewportH = (uint32_t)std::max(1.0f, GetHeight() - kBottomBarHeight);

        // Create RenderTexture for the viewport. The UI/layout is in logical
        // units; the RT is physical (logical x DPI) so the 3D view is sharp.
        float dpr = GetContentScale();
        m_ViewportRT = std::make_unique<Leir::RenderTexture>(
            m_Backend.get(),
            (uint32_t)std::max(1.0f, (float)std::lround(m_ViewportW * dpr)),
            (uint32_t)std::max(1.0f, (float)std::lround(m_ViewportH * dpr)));
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
        auto sheetTex = std::make_shared<Leir::Texture2D>(m_Backend.get(), sheetImage);
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
        m_UIRenderer = std::make_unique<Leir::UIRenderer>(m_Backend.get());
        m_UIRenderer->SetContentScale(GetContentScale());

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
            Leir::XConsole::PrintWarning("No system font found, text will not render");
        }

        if (!fontPath.empty()) {
            m_Font = std::make_unique<Leir::Font>(m_Backend.get(), fontPath, 16);
            m_FontSmall = std::make_unique<Leir::Font>(m_Backend.get(), fontPath, 13);
        }

        m_Canvas = std::make_unique<Leir::UICanvas>();
        m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
        m_Canvas->ConnectToInputSystem();

        // ---- Editor Layout (dock system) ----
        // The dock manager is the full-screen root. It leaves the bottom 30px
        // free for the status bar.
        m_DockManager = new Leir::DockManager();
        m_DockManager->SetName("EditorDock");
        m_DockManager->SetFont(m_FontSmall.get());
        m_DockManager->GetRect().anchor = Leir::AnchorSet::Stretch();
        m_DockManager->GetRect().offset = {0.0f, 0.0f, 0.0f, -kBottomBarHeight};
        m_Canvas->AddChild(m_DockManager);

        // Viewport panel (rendered from the shared RenderTexture)
        m_ViewportPanel = new Leir::UIViewportPanel();
        m_ViewportPanel->SetName("Viewport");
        m_ViewportPanel->SetRenderTexture(m_ViewportRT.get());

        // Hierarchy panel (left dock pane)
        auto* hierarchy = new Leir::UIPanel();
        m_HierarchyPanel = hierarchy;
        hierarchy->SetName("Hierarchy");
        hierarchy->SetColor({0.16f, 0.16f, 0.18f, 1.0f});
        hierarchy->SetLayoutMode(Leir::LayoutMode::Column);
        hierarchy->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
        hierarchy->SetSpacing(2.0f);

        auto* hierarchyTitle = new Leir::UILabel();
        hierarchyTitle->SetName("HierarchyTitle");
        hierarchyTitle->SetText("-- Hierarchy --");
        hierarchyTitle->SetFont(m_FontSmall.get());
        hierarchyTitle->SetColor({0.7f, 0.7f, 0.7f, 1.0f});
        hierarchyTitle->SetSizePolicy(Leir::SizePolicy::Fixed);
        hierarchy->AddChild(hierarchyTitle);

        // Inspector panel (right dock pane)
        auto* inspector = new Leir::UIPanel();
        m_InspectorPanel = inspector;
        inspector->SetName("Inspector");
        inspector->SetColor({0.16f, 0.16f, 0.18f, 1.0f});
        inspector->SetLayoutMode(Leir::LayoutMode::Column);
        inspector->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
        inspector->SetSpacing(2.0f);

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

        // Bottom bar (sibling of the dock, pinned to the bottom)
        auto* bottomBar = new Leir::UIImage();
        bottomBar->SetName("BottomBar");
        bottomBar->GetRect().anchor = {0.0f, 1.0f, 1.0f, 1.0f};
        bottomBar->GetRect().offset = {0.0f, -30.0f, 0.0f, 0.0f};
        bottomBar->SetColor({0.1f, 0.1f, 0.12f, 1.0f});
        m_Canvas->AddChild(bottomBar);

        auto* statusLabel = new Leir::UILabel();
        statusLabel->SetName("StatusLabel");
        statusLabel->SetText("Editor Online | Click der. + WASD para camara libre | Arrastra tabs para reacomodar");
        statusLabel->SetFont(m_FontSmall.get());
        statusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
        statusLabel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f};
        statusLabel->GetRect().offset = {8.0f, -28.0f, 600.0f, 0.0f};
        m_Canvas->AddChild(statusLabel);

        // Debug test panels (docked, no longer floating)
        m_TestPanel = new UITestPanel();
        m_TestPanel->SetName("DebugTestPanel");
        m_TestPanel->SetFont(m_FontSmall.get());
        m_TestPanel->SetTargetObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Cube")));

        m_CameraTestPanel = new CameraTestPanel();
        m_CameraTestPanel->SetName("DebugCameraPanel");
        m_CameraTestPanel->SetFont(m_FontSmall.get());
        m_CameraTestPanel->SetCameraObject(
            dynamic_cast<Leir::Object3D*>(scene.FindObjectByName("Camera")));

        m_TextAreaDebugPanel = new TextAreaDebugPanel();
        m_TextAreaDebugPanel->SetName("DebugTextAreaPanel");
        m_TextAreaDebugPanel->SetFont(m_FontSmall.get());

        m_TextAreaWrapPanel = new TextAreaWrapPanel();
        m_TextAreaWrapPanel->SetName("DebugTextAreaWrapPanel");
        m_TextAreaWrapPanel->SetFont(m_FontSmall.get());

        m_DebugTextPanel = new DebugTextPanel();
        m_DebugTextPanel->SetName("DebugTextPanel");
        m_DebugTextPanel->SetFont(m_FontSmall.get());

        m_ConsolePanel = new ConsolePanel();
        m_ConsolePanel->SetName("ConsolePanel");
        m_ConsolePanel->SetFont(m_FontSmall.get());

        m_DebugPanel = new DebugPanel();
        m_DebugPanel->SetName("DebugPanel");
        m_DebugPanel->SetFont(m_FontSmall.get());

        // Register dockable panels (core ones are not closeable)
        m_DockManager->RegisterPanel("Hierarchy", "Hierarchy", hierarchy, false);
        m_DockManager->RegisterPanel("Viewport", "Viewport", m_ViewportPanel, false);
        m_DockManager->RegisterPanel("Inspector", "Inspector", inspector, false);
        m_DockManager->RegisterPanel("TestPanel", "Test", m_TestPanel, true);
        m_DockManager->RegisterPanel("CameraTestPanel", "Camera", m_CameraTestPanel, true);
        m_DockManager->RegisterPanel("DebugTextPanel", "Debug Text", m_DebugTextPanel, true);
        m_DockManager->RegisterPanel("TextAreaDebugPanel", "Text Area", m_TextAreaDebugPanel, true);
        m_DockManager->RegisterPanel("TextAreaWrapPanel", "Text Area Wrap", m_TextAreaWrapPanel, true);
        m_DockManager->RegisterPanel("ConsolePanel", "Console", m_ConsolePanel, true);
        m_DockManager->RegisterPanel("DebugPanel", "Debug Panel", m_DebugPanel, true);

        // Restore a persisted layout, or fall back to the default one
        const std::string& dockJson = Leir::LeirSettings::Get().dock.layout;
        if (dockJson.empty())
            m_DockManager->BuildDefaultLayout();
        else if (!m_DockManager->LoadLayout(dockJson))
            m_DockManager->BuildDefaultLayout();

        m_DockManager->SetOnLayoutChanged([this]() {
            try {
                Leir::LeirSettings::Get().dock.layout = m_DockManager->SerializeLayout();
                Leir::LeirSettings::Get().Save();
            } catch (const std::exception& e) {
                Leir::XConsole::PrintError("Dock layout serialization failed: {}", e.what());
            } catch (...) {
                Leir::XConsole::PrintError("Dock layout serialization failed (unknown error)");
            }
        });

        // Sync scene camera from EditorCamera initial position
        auto* camObj = scene.FindObjectByName("Camera");
        if (camObj) {
            camObj->GetTransform().SetLocalPosition(m_EditorCamera.GetPosition());
            camObj->GetTransform().SetLocalRotation(m_EditorCamera.GetRotation());
        }

        m_Canvas->UpdateLayout();

        m_DebugOverlay = std::make_unique<Leir::UIDebugOverlay>(m_Font.get(), m_Canvas.get());
        m_DebugOverlay->SetRenderStatsProvider([this]() {
            return m_UIRenderer ? m_UIRenderer->GetLastStats() : Leir::UIRenderStats{};
        });

        Leir::XConsole::Println("Scene hierarchy created with dock system");
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // Editor camera controls (right-click + WASD free-fly). The viewport is
        // now the content of a dock pane, so walk ancestors up to it.
        auto* hovered = m_Canvas->GetHoveredElement();
        bool inViewport = false;
        if (m_ViewportPanel && hovered) {
            for (Leir::UIElement* e = hovered; e; e = e->GetParent()) {
                if (e == m_ViewportPanel) { inViewport = true; break; }
            }
        }
        bool rightDown = inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Right);
        bool middleDown = inViewport && Leir::Mouse::IsDown(Leir::PointerButton::Middle);

        // Update EditorCamera state
        if (rightDown || middleDown)
            m_EditorCamera.Update(deltaTime);

        bool cameraControlled = rightDown || middleDown;

        // Bidirectional sync: EditorCamera â†” scene camera
        auto* cameraObj = scene->FindObjectByName("Camera");
        if (cameraObj) {
            if (cameraControlled) {
                // EditorCamera â†’ escena (durante control)
                cameraObj->GetTransform().SetLocalPosition(m_EditorCamera.GetPosition());
                cameraObj->GetTransform().SetLocalRotation(m_EditorCamera.GetRotation());
            } else {
                // escena â†’ EditorCamera (panel edits)
                auto& t = cameraObj->GetTransform();
                auto pos = t.GetLocalPosition();
                auto euler = Leir::Quaternion::ToEuler(t.GetLocalRotation());
                m_EditorCamera.SetPosition(pos);
                m_EditorCamera.SetYaw(euler.y);
                m_EditorCamera.SetPitch(euler.x);
            }
        }

        // Rebuild console lines BEFORE the layout pass so freshly created labels
        // get their computed rects in the same frame. If rebuilt after
        // UpdateLayout, the new content stays at {0,0,0,0} and gets culled by
        // the ScrollView clip for one frame (visible as a flash/flicker).
        if (m_Canvas)
            m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
        if (m_ConsolePanel)
            m_ConsolePanel->Refresh();

        // Update UI layout on resize
        if (m_Canvas)
            m_Canvas->UpdateLayout();

        // Keep the viewport render target in sync with the actual layout size
        UpdateViewportRenderTarget();

        if (m_DebugOverlay)
            m_DebugOverlay->Update(deltaTime);

        if (m_DockManager)
            m_DockManager->Process();

        if (m_TestPanel)
            m_TestPanel->Refresh();
        if (m_CameraTestPanel)
            m_CameraTestPanel->Refresh();
        if (m_DebugTextPanel)
            m_DebugTextPanel->Refresh();
        if (m_TextAreaDebugPanel)
            m_TextAreaDebugPanel->Refresh();
        if (m_TextAreaWrapPanel)
            m_TextAreaWrapPanel->Refresh();
        if (m_InspectorTransformPanel)
            m_InspectorTransformPanel->Refresh();
    }

    void OnRender() override
    {
        // BeginFrame(true) skips the swapchain 3D render pass
        if (!m_Backend || !m_Backend->BeginFrame(true)) return;

        auto cmd = m_Backend->GetCurrentCommandBuffer();
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();

        // 1. Render 3D scene + sprites to offscreen RenderTexture
        if (m_ViewportRT && scene) {
            Leir::RHI::RHIClearValue clearColor;
            clearColor.color = {0.15f, 0.15f, 0.2f, 1.0f};
            m_ViewportRT->BeginRender(cmd, clearColor, 1.0f);
            m_RenderPipeline->Render(cmd, scene);
            m_ViewportRT->EndRender(cmd);
        }

        // 2. Render UI to swapchain overlay directly
        m_Backend->BeginSwapchainOverlay();
        if (m_UIRenderer && m_Canvas)
            m_UIRenderer->Render(cmd, m_Canvas.get());
        m_Backend->EndFrame();
    }

    void OnShutdown() override
    {
        auto tStart = std::chrono::steady_clock::now();
        auto elapsedMs = [&]() -> double {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tStart).count();
        };
        Leir::XConsole::Println("Editor shutting down");
        auto& settings = Leir::LeirSettings::Get();
        // Persist the dock tree (tabs, splits, ratios, closed panels)
        settings.dock.layout = m_DockManager ? m_DockManager->SerializeLayout() : "";
        // Persist window placement so the next launch restores it. In fullscreen
        // the saved windowed rect is kept untouched (monitor size would be wrong).
        if (!settings.window.fullscreen) {
            if (GetNormalWindowRect(settings.window.pos_x, settings.window.pos_y,
                                    settings.window.width, settings.window.height))
                settings.window.maximized = IsMaximized();
        }
        settings.Save();
        Leir::XConsole::Debug("[Timing] settings saved: {:.1f} ms", elapsedMs());
        // Destroy the dock tree (content panels stay owned by the editor).
        // Remove it from the canvas first so the canvas dtor never sees it freed.
        if (m_DockManager) {
            m_Canvas->RemoveChild(m_DockManager);
            delete m_DockManager;
            m_DockManager = nullptr;
        }
        Leir::XConsole::Debug("[Timing] dock manager destroyed: {:.1f} ms", elapsedMs());
        // The dock tree only reparents the content panels out on delete; the
        // editor owns them (UIElement dtor doesn't free children), so free the
        // whole subtrees here. The InspectorTransformPanel is a child of the
        // Inspector content, so it is covered by m_InspectorPanel.
        DeleteUiSubtree(m_ViewportPanel);
        m_ViewportPanel = nullptr;
        DeleteUiSubtree(m_HierarchyPanel);
        m_HierarchyPanel = nullptr;
        DeleteUiSubtree(m_InspectorPanel);
        m_InspectorPanel = nullptr;
        DeleteUiSubtree(m_TestPanel);
        m_TestPanel = nullptr;
        DeleteUiSubtree(m_CameraTestPanel);
        m_CameraTestPanel = nullptr;
        DeleteUiSubtree(m_TextAreaDebugPanel);
        m_TextAreaDebugPanel = nullptr;
        DeleteUiSubtree(m_TextAreaWrapPanel);
        m_TextAreaWrapPanel = nullptr;
        DeleteUiSubtree(m_DebugTextPanel);
        m_DebugTextPanel = nullptr;
        DeleteUiSubtree(m_ConsolePanel);
        m_ConsolePanel = nullptr;
        DeleteUiSubtree(m_DebugPanel);
        m_DebugPanel = nullptr;
        m_InspectorTransformPanel = nullptr; // freed via m_InspectorPanel above
        Leir::XConsole::Debug("[Timing] UI subtrees freed: {:.1f} ms", elapsedMs());
        // Destroy viewport RT before the backend
        m_ViewportRT.reset();
        Leir::XConsole::Debug("[Timing] viewport RT destroyed: {:.1f} ms", elapsedMs());
        auto& sm = Leir::SceneManager::GetInstance();
        sm.DestroyScene("Main Scene");
        sm.SetActiveScene(nullptr);
        Leir::XConsole::Debug("[Timing] Editor OnShutdown total: {:.1f} ms", elapsedMs());
    }

    void OnWindowResized(int width, int height) override
    {
        (void)width;
        (void)height;
        // Notify the backend so the swapchain is recreated at next present
        if (m_Backend)
            m_Backend->NotifyResized();
    }

    void OnContentScaleChanged() override
    {
        // DPI changed (e.g. monitor moved / system scale changed). The logical
        // size is updated by the framebuffer callback; re-layout and re-size
        // the viewport RT happen each frame, so just log the change.
        Leir::XConsole::Println("Content scale changed to {:.2f} (logical {}x{})",
            GetContentScale(), GetWidth(), GetHeight());
        if (m_UIRenderer)
            m_UIRenderer->SetContentScale(GetContentScale());
    }

private:
    void UpdateViewportRenderTarget()
    {
        if (!m_ViewportRT || !m_ViewportPanel)
            return;

        const auto& cr = m_ViewportPanel->GetComputedRect();
        uint32_t w = (uint32_t)std::max(1.0f, cr.z); // logical
        uint32_t h = (uint32_t)std::max(1.0f, cr.w);

        // Physical render target size = logical x DPI
        float dpr = GetContentScale();
        uint32_t fw = (uint32_t)std::max(1.0f, (float)std::lround(w * dpr));
        uint32_t fh = (uint32_t)std::max(1.0f, (float)std::lround(h * dpr));

        if (fw == m_ViewportRT->GetWidth() && fh == m_ViewportRT->GetHeight()) {
            // Size settled: if a resize was applied but not yet logged, emit the
            // message once now (debounce). During a continuous splitter drag the
            // size changes every frame, so nothing is logged until it stops.
            if (m_HasPendingResizeLog &&
                (m_PendingW != m_LastLoggedW || m_PendingH != m_LastLoggedH)) {
                Leir::XConsole::Println("Viewport resized to {}x{} ({}x{} physical)",
                    m_PendingW, m_PendingH, m_PendingFw, m_PendingFh);
                m_LastLoggedW = m_PendingW;
                m_LastLoggedH = m_PendingH;
                m_HasPendingResizeLog = false;
            }
            return;
        }

        m_ViewportRT->Resize(fw, fh);

        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        auto* cameraObj = scene ? scene->FindObjectByName("Camera") : nullptr;
        if (cameraObj) {
            if (auto* cam = cameraObj->GetComponent<Leir::Camera>())
                cam->SetPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
        }

        m_PendingW = w;
        m_PendingH = h;
        m_PendingFw = fw;
        m_PendingFh = fh;
        m_HasPendingResizeLog = true;
    }

    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
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

    // Dock system
    Leir::DockManager* m_DockManager = nullptr;
    Leir::UIPanel* m_HierarchyPanel = nullptr;
    Leir::UIPanel* m_InspectorPanel = nullptr;

    // Viewport system
    std::unique_ptr<Leir::RenderTexture> m_ViewportRT;
    Leir::UIViewportPanel* m_ViewportPanel = nullptr;
    EditorCamera m_EditorCamera;

    UITestPanel* m_TestPanel = nullptr;
    CameraTestPanel* m_CameraTestPanel = nullptr;
    DebugTextPanel* m_DebugTextPanel = nullptr;
    ConsolePanel* m_ConsolePanel = nullptr;
    TextAreaDebugPanel* m_TextAreaDebugPanel = nullptr;
    TextAreaWrapPanel* m_TextAreaWrapPanel = nullptr;
    DebugPanel* m_DebugPanel = nullptr;
    InspectorTransformPanel* m_InspectorTransformPanel = nullptr;
    uint32_t m_ViewportW = 800;
    uint32_t m_ViewportH = 600;

    // Debounce for the "Viewport resized" log: track the last applied resize
    // and only print once the size stops changing (splitter drags settle).
    uint32_t m_PendingW = 0;
    uint32_t m_PendingH = 0;
    uint32_t m_PendingFw = 0;
    uint32_t m_PendingFh = 0;
    uint32_t m_LastLoggedW = 0;
    uint32_t m_LastLoggedH = 0;
    bool m_HasPendingResizeLog = false;
};

int main()
{
    Leir::XConsole::SetLevel(Leir::LogLevel::Trace);

    CrashDiagnostics::Init();

    Leir::LeirSettings::Get().Load();

    std::chrono::steady_clock::time_point tRunEnd;
    auto tMainStart = std::chrono::steady_clock::now();
    try {
        auto tCtor0 = std::chrono::steady_clock::now();
        EditorApp app;
        auto tCtor1 = std::chrono::steady_clock::now();
        Leir::XConsole::Debug("[Timing] EditorApp construction: {:.1f} ms",
            std::chrono::duration<double, std::milli>(tCtor1 - tCtor0).count());
        app.Run();
        tRunEnd = std::chrono::steady_clock::now();
        Leir::XConsole::Debug("[Timing] EditorApp::Run returned (OnShutdown done): {:.1f} ms in ctor+run",
            std::chrono::duration<double, std::milli>(tRunEnd - tMainStart).count());
        // `app` destructor (members + CoreApplication base) runs at scope end, measured below.
    } catch (const std::exception& e) {
        Leir::XConsole::PrintError("Uncaught exception in main: {}", e.what());
        return 1;
    } catch (...) {
        Leir::XConsole::PrintError("Uncaught unknown exception in main");
        return 1;
    }
    auto tDestroyEnd = std::chrono::steady_clock::now();
    Leir::XConsole::Debug("[Timing] Member destructors + CoreApplication teardown (RenderBackend, GLFW): {:.1f} ms",
        std::chrono::duration<double, std::milli>(tDestroyEnd - tRunEnd).count());

    return 0;
}
