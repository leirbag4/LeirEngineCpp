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
#include <LeirEngine/UI/UIWindowInternal.h>
#include <LeirEngine/UI/UISlider.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/ScrollView.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/UI/UIRenderer.h>
#include <LeirEngine/UI/UIViewportPanel.h>
#include <LeirEngine/UI/UIDebugOverlay.h>
#include <LeirEngine/UI/Dock/DockManager.h>
#include "UI/UITestPanel.h"
#include "UI/GizmoLineTestPanel.h"
#include "UI/GridPanel.h"
#include "UI/CameraTestPanel.h"
#include "UI/DebugTextPanel.h"
#include "UI/TextAreaDebugPanel.h"
#include "UI/TextAreaWrapPanel.h"
#include "UI/ConsolePanel.h"
#include "UI/DebugPanel.h"
#include "UI/InspectorTransformPanel.h"
#include "UI/ToolbarPanel.h"
#include "UI/AboutWindow.h"
#include "LeirEngine/UI/UIMenuBar.h"
#include "LeirEngine/UI/UIContextMenu.h"
#include "LeirEngine/UI/UIWindowExternal.h"
#include "UI/GizmoLogPanel.h"
#include "UI/TreeViewDebugPanel.h"
#include "UI/HierarchyPanel.h"
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UITextureCache.h>
#include "Camera/EditorCamera.h"
#include "Grid/EditorGrid.h"
#include "Gizmos/GizmoRenderer.h"
#include "Gizmos/TransformGizmo.h"

#ifdef LEIR_EDITOR_SLANG
#include "Shaders/SlangShaderCompiler.h"
#include "Shaders/ShaderExporter.h"
#include "Shaders/ShaderHotReloader.h"
#endif

#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Input/Mouse.h>
#include <LeirEngine/Input/InputManager.h>
#include <LeirEngine/Input/EventQueue.h>

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
const float kTopToolbarHeight = 30.0f;
const float kTopMenuBarHeight = 28.0f;
// Editor-wide background. The canvas paints it first (bottom layer) so every
// physical pixel is covered every frame — the overlay render pass uses
// LOAD_OP_LOAD (it shares the swapchain with the 3D demos), so any pixel the
// UI never draws keeps stale/garbage content (the 1px seam between the toolbar
// and the dock at fractional DPI, plus the colored pixels left behind by
// floating windows moving over that seam).
const Leir::Vector4 kEditorBackgroundColor = {0.12f, 0.12f, 0.15f, 1.0f};
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
// scrollbars, UIScrollbar's thumb, ...) are skipped — deleting them again would
// be a double free (crash 0xC0000005 in LeirEngine.dll → 5s shutdown).
void DeleteUiSubtree(Leir::UIElement* element)
{
    if (!element)
        return;
    // Detach from the parent FIRST so the parent's child list never holds a
    // dangling pointer (the canvas's dtor iterates m_Children and nulls each
    // child's parent — a freed child there caused an intermittent AV at close,
    // e.g. the toolbar which was deleted but not removed from the canvas).
    if (element->GetParent())
        element->GetParent()->RemoveChild(element);
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

        // App icon: Windows embeds LeirEditor.ico in the exe via res/LeirEditor.rc;
        // this sets the window/taskbar icon at runtime (cross-platform GLFW).
        // assets/leir_icon.png is copied next to the exe by the editor POST_BUILD.
        SetWindowIcon("assets/leir_icon.png");

        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            Leir::LeirSettings::Get().graphics.backend,
            GetWindow(), GetWidth(), GetHeight(),
            Leir::LeirSettings::Get().window.vsync, "LeirEngine Editor"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("Failed to create render backend");
            return;
        }

        // The window title reflects the backend actually created (settings
        // may hold an invalid name that BackendFactory silently falls back to
        // the compile-time default for).
        const char* backendName = m_Backend->GetBackendName();
        SetWindowTitle((std::string("LeirEngine Editor - ") + backendName).c_str());

        // Auto-correct an invalid/empty backend in settings so the saved value
        // always matches reality (self-healing config).
        auto& settings = Leir::LeirSettings::Get();
        if (settings.graphics.backend != backendName) {
            Leir::XConsole::PrintWarning(
                "Graphics backend '{}' not valid; using '{}' (settings corrected)",
                settings.graphics.backend, backendName);
            settings.graphics.backend = backendName;
            settings.Save();
        }

        const auto& caps = m_Backend->GetCaps();
        Leir::XConsole::Println(
            "[GCaps] backend={} textures={} ubo={} samplers={} ssbo={} push={}B MRT={} maxRT={} maxTex={} "
            "bindless={} instancing={} compute={} storage={} sRGB={} wireframe={} aniso={}",
            m_Backend->GetShaderFileExtension(),
            caps.maxTexturesPerTable, caps.maxUniformBuffersPerTable, caps.maxSamplersPerTable,
            caps.maxStorageBuffersPerTable, caps.maxPushConstantsSize, caps.multiRenderTarget,
            caps.maxColorAttachments, caps.maxTextureSize,
            caps.bindless, caps.instancing, caps.compute, caps.storageBuffers, caps.sRGB,
            caps.wireframe, caps.anisotropicFiltering);

#ifdef LEIR_EDITOR_SLANG
        // ---- Shader compiler (Plan A) ----
        // Created before the Shader/Material so the reflection sidecars are
        // written to LEIR_SHADER_DIR first: pipeline layouts are then derived
        // from the shader signature (Plan B, Fase 2). The engine itself never
        // links Slang; this is editor-only dev tooling.
        m_ShaderCompiler = std::make_unique<Leir::RHI::SlangShaderCompiler>();
        if (m_ShaderCompiler->IsAvailable()) {
            Leir::XConsole::Println("Shader compiler ready: {} (libslang)",
                m_ShaderCompiler->GetVersion());
            auto sidecarLines = ShaderExporter::WriteRuntimeSidecars(m_ShaderCompiler.get());
            for (const auto& line : sidecarLines)
                Leir::XConsole::Println("{}", line);

            // Single-source WGSL for the WebGPU backend (grid first): generate
            // Grid.vert/frag.wgsl from the .slang (post-processed for the
            // backend) so the wgpu-native backend never loads stale hand-written
            // mirrors. Must run before the grid pipeline is created below.
            auto wgpuLines = ShaderExporter::WriteRuntimeWebGpuShaders(m_ShaderCompiler.get());
            for (const auto& line : wgpuLines)
                Leir::XConsole::Println("{}", line);

            // Web export single-source: generate the .web.wgsl (LEIR_BINDLESS=0) into the
            // engine/shaders SOURCE dir — the web build preloads
            // engine/shaders@/shaders, so the exported demo always uses the
            // generated shaders (git diff on the committed .web.wgsl is the
            // drift check). LEIR_SHADER_SOURCE_DIR is compile-time (engine).
            auto webLines = ShaderExporter::WriteWebShaders(m_ShaderCompiler.get(),
                LEIR_SHADER_SOURCE_DIR);
            for (const auto& line : webLines)
                Leir::XConsole::Println("{}", line);
        }
#endif

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

        // Editor ground grid (Unity-style, Y=0): procedural line grid drawn
        // into the viewport RT using the same constant-pixel-width gizmo
        // technique as the Test2 line (Grid.vert/frag). Its pipeline targets
        // the viewport render pass, so it must be (re)created after the
        // RenderTexture exists. Follows the camera (infinite) and fades the
        // minor 1u lines by distance, Unity-style.
        m_Grid = std::make_unique<EditorGrid>(m_Backend.get(), m_ViewportRT->GetRenderPass());

        // Gizmo renderer (procedural 3D lines/boxes/circles/spheres, constant
        // screen-pixel width). Drawn into the viewport RT on top of the scene.
        // PHASE-1 TEST: enabled with the 3 test lines (red X axis, blue Z axis,
        // white diagonal) in DrawGizmoShowcase. On success the grid returns.
        m_Gizmos = std::make_unique<GizmoRenderer>(m_Backend.get(), m_ViewportRT->GetRenderPass());

        // Camera (will be driven by EditorCamera)
        auto* cameraObj = scene.CreateObject3D("Camera");
        auto& camera = cameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)m_ViewportW / (float)m_ViewportH, 0.1f, 2000.0f);
        camera.SetPrimary(true);
        m_PrimaryCamera = &camera;

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
        // PHASE-1 TEST: the demo sprites are disabled � the screen-center
        // "TestSprite" quad writes depth and was occluding the gizmo test
        // lines (the blue Z axis had a "gap" exactly over its 200x200 area).
        // Re-enable after the line validation.
        // auto* spriteObj = scene.CreateObject2D("TestSprite");
        // spriteObj->GetTransform().SetLocalPosition(
        //     {GetWidth() * 0.5f, GetHeight() * 0.5f, 0.0f});
        // spriteObj->GetTransform().SetLocalScale({200.0f, 200.0f, 1.0f});
        // auto& spr = spriteObj->AddComponent<Leir::SpriteRenderer>();
        // spr.SetColor({0.0f, 1.0f, 1.0f, 1.0f});

        // auto* spriteTex = scene.CreateObject2D("TexSprite");
        // spriteTex->GetTransform().SetLocalPosition({100.0f, 100.0f, 0.0f});
        // spriteTex->GetTransform().SetLocalScale({100.0f, 100.0f, 1.0f});
        // auto& sprTex = spriteTex->AddComponent<Leir::SpriteRenderer>();
        // sprTex.SetTexture(m_WhiteTexture.get());
        // sprTex.SetColor({1.0f, 0.0f, 0.0f, 1.0f});

        // Leir::Image sheetImage("assets/sprite_sheet_64_64.png");
        // auto sheetTex = std::make_shared<Leir::Texture2D>(m_Backend.get(), sheetImage);
        // auto sheet = std::make_shared<Leir::SpriteSheet>(sheetTex.get(), 32, 32);

        // auto* sheetSprite = scene.CreateObject2D("SheetSprite");
        // sheetSprite->GetTransform().SetLocalPosition({GetWidth() * 0.75f, GetHeight() * 0.25f, 0.0f});
        // sheetSprite->GetTransform().SetLocalScale({100.0f, 100.0f, 1.0f});
        // auto& sSpr = sheetSprite->AddComponent<Leir::SpriteRenderer>();
        // sSpr.SetSpriteSheet(sheet.get());
        // sSpr.SetFrameIndex(0);
        // sSpr.SetColor({1.0f, 1.0f, 1.0f, 1.0f});

        // m_SheetTexture = sheetTex;
        // m_SpriteSheet = sheet;
        // m_SheetSprites.push_back(sheetSprite);

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
            float dpr = GetContentScale();
            m_Font = std::make_unique<Leir::Font>(m_Backend.get(), fontPath, 16, dpr);
            m_FontSmall = std::make_unique<Leir::Font>(m_Backend.get(), fontPath, 13, dpr);
        }

        m_Canvas = std::make_unique<Leir::UICanvas>();
        m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
        // Bind the canvas to the MAIN window: events from external windows
        // (e.g. the Leir Test Window) must not reach the editor's UI.
        m_Canvas->SetInputWindow(GetWindow());
        m_Canvas->ConnectToInputSystem();

        // Full-screen background: covers every pixel so the overlay render pass
        // (LOAD_OP_LOAD, shared with the 3D demos) never shows garbage in the
        // 1px seam between the toolbar and the dock at fractional DPI, or the
        // colored pixels left behind by floating windows moving over that seam.
        auto* editorBg = new Leir::UIPanel();
        editorBg->SetName("EditorBackground");
        editorBg->SetColor(kEditorBackgroundColor);
        editorBg->GetRect().anchor = Leir::AnchorSet::Stretch();
        m_Canvas->AddChild(editorBg);

        // ---- Gizmo log recorder: capture REAL input events ----
        // Only records when a user input event actually happens (mouse move /
        // click / wheel / key), like the console: no event -> no log line. The
        // hooks coexist with the canvas (which uses Set*Hook; we Add*Hook).
        {
            auto& eq = Leir::EventQueue::Get();
            eq.AddPointerHook([this](const Leir::PointerEvent& e) {
                OnInputEvent("POINTER", (int)e.action, (int)e.button,
                             e.position.x, e.position.y, e.delta.x, e.delta.y);
            });
            eq.AddScrollHook([this](const Leir::ScrollEvent& e) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "SCROLL x=%.1f y=%.1f", e.offset.x, e.offset.y);
                RecordGizmoLog(buf);
            });
            eq.AddKeyHook([this](const Leir::KeyEvent& e) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "KEY action=%d key=%d", (int)e.action, (int)e.key);
                RecordGizmoLog(buf);
            });
            eq.AddCharHook([this](const Leir::CharEvent& e) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "CHAR cp=%u", e.codepoint);
                RecordGizmoLog(buf);
            });
        }

        // ---- Editor Layout (dock system) ----
        // The dock manager is the full-screen root. It leaves the bottom 30px
        // free for the status bar and the top (menu bar + toolbar) for the
        // non-dockable top chrome.
        m_DockManager = new Leir::DockManager();
        m_DockManager->SetName("EditorDock");
        m_DockManager->SetFont(m_FontSmall.get());
        m_DockManager->GetRect().anchor = Leir::AnchorSet::Stretch();
        m_DockManager->GetRect().offset = {0.0f, kTopMenuBarHeight + kTopToolbarHeight, 0.0f, -kBottomBarHeight};
        m_Canvas->AddChild(m_DockManager);

        // Menu bar: the very top row (File / Help …), sibling of the toolbar.
        m_MenuBar = new Leir::UIMenuBar();
        m_MenuBar->SetName("MenuBar");
        m_MenuBar->SetFont(m_FontSmall.get());
        m_MenuBar->GetRect().anchor = {0.0f, 0.0f, 1.0f, 0.0f};
        m_MenuBar->GetRect().offset = {0.0f, 0.0f, 0.0f, kTopMenuBarHeight};
        m_Canvas->AddChild(m_MenuBar);

        // File menu.
        {
            auto* fileItem = m_MenuBar->AddItem("File");
            fileItem->AddMenuItem("New Scene", [this]() {
                Leir::XConsole::Println("New Scene (pendiente TODO_FILE_SYSTEM.md)");
            });
            fileItem->AddMenuItem("Open Scene...", [this]() {
                Leir::XConsole::Println("Open Scene... (pendiente TODO_FILE_SYSTEM.md)");
            });
            fileItem->AddMenuSeparator();
            fileItem->AddMenuItem("Save Scene", [this]() {
                Leir::XConsole::Println("Save Scene (pendiente TODO_FILE_SYSTEM.md)");
            });
            fileItem->AddMenuItem("Save All", [this]() {
                Leir::XConsole::Println("Save All (pendiente TODO_FILE_SYSTEM.md)");
            });
            fileItem->AddMenuSeparator();
            fileItem->AddMenuItem("Exit", [this]() {
                Quit();
            });
        }

        // Edit menu with a nested submenu (exercises the "›" arrow path).
        {
            auto* editItem = m_MenuBar->AddItem("Edit");
            auto* toolsSub = new Leir::UIContextMenu();
            toolsSub->AddItem("Transform", [this]() {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Translate);
                if (m_Toolbar) m_Toolbar->SetTool(ToolbarPanel::Tool::Translate);
            });
            toolsSub->AddItem("Rotate", [this]() {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Rotate);
                if (m_Toolbar) m_Toolbar->SetTool(ToolbarPanel::Tool::Rotate);
            });
            toolsSub->AddItem("Scale", [this]() {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Scale);
                if (m_Toolbar) m_Toolbar->SetTool(ToolbarPanel::Tool::Scale);
            });
            editItem->AddSubMenu("Transform Tool", toolsSub);
            editItem->AddMenuSeparator();
            editItem->AddMenuItem("Test Internal Window", [this]() {
                ShowInternalTestWindow();
            });
            editItem->AddMenuSeparator();
            editItem->AddMenuItem("Preferences...", [this]() {
                Leir::XConsole::Println("Preferences... (pendiente)");
            });
        }

        // Help menu.
        {
            auto* helpItem = m_MenuBar->AddItem("Help");
            helpItem->AddMenuItem("About LeirEngine", [this]() {
                // Create the About window. The backend decides whether it can
                // create an external window: CreateSwapchainTarget returns
                // nullptr on backends without support, and Show() logs it.
                if (m_Backend) {
                    // If already open, just focus it (a second click with the
                    // About visible would otherwise leak the previous window).
                    if (m_AboutWindow && m_AboutWindow->IsVisible()) {
                        m_AboutWindow->BringToFront();
                        return;
                    }
                    m_AboutWindow = new AboutWindow(m_Backend.get(), "1.0.0");
                    m_AboutWindow->SetFont(m_FontSmall.get());
                    m_AboutWindow->Show();
                }
            });
        }

        // Load submenu arrow icon and propagate to all menus.
        ApplyMenuIcons();

        // Fase D — external window test (only with backends that support
        // CreateSwapchainTarget; Show() logs when a backend can't create one).
        // Fase 1 (2026-09-02): re-activado tras el diagnóstico del bug del grid
        // (los recursos dinámicos ahora tienen un ring de 3 frames, MAX_FRAMES_IN_FLIGHT=3).
        if (m_Backend) {
            m_TestWindow = new Leir::UIWindowExternal(m_Backend.get(), "Leir Test Window");
            m_TestWindow->Show();
            // Give the window real content so the render path is visible.
            if (Leir::UICanvas* c = m_TestWindow->GetCanvas()) {
                auto* title = new Leir::UILabel();
                title->SetName("TestWinTitle");
                title->SetText("External Window");
                title->SetFont(m_FontSmall.get());
                title->SetColor({0.9f, 0.9f, 0.95f, 1.0f});
                title->GetRect().anchor = {0.0f, 0.0f, 0.0f, 0.0f};
                title->GetRect().offset = {10.0f, 8.0f, 210.0f, 28.0f};
                c->AddChild(title);

                auto* body = new Leir::UIPanel();
                body->SetName("TestWinBody");
                body->SetColor({0.20f, 0.22f, 0.28f, 1.0f});
                body->GetRect().anchor = {0.0f, 0.0f, 1.0f, 1.0f};
                body->GetRect().offset = {4.0f, 34.0f, -4.0f, -4.0f};
                c->AddChild(body);

                auto* label = new Leir::UILabel();
                label->SetName("TestWinLabel");
                label->SetText("Vulkan multi-window OK");
                label->SetFont(m_FontSmall.get());
                label->SetColor({0.8f, 0.8f, 0.85f, 1.0f});
                label->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
                label->GetRect().offset = {-90.0f, -10.0f, 90.0f, 12.0f};
                body->AddChild(label);

                // Test button: verifies that widgets in the EXTERNAL window
                // receive input independently of the main editor (Fase B input
                // routing). Click toggles the label text.
                auto* button = new Leir::UIButton();
                button->SetName("TestWinButton");
                button->SetText("Click me");
                button->SetFont(m_FontSmall.get());
                button->SetColors({0.35f, 0.45f, 0.65f, 1.0f},
                                  {0.45f, 0.55f, 0.75f, 1.0f},
                                  {0.25f, 0.35f, 0.55f, 1.0f});
                button->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
                button->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
                button->GetRect().offset = {-60.0f, 16.0f, 60.0f, 40.0f};
                button->SetOnClick([label]() {
                    label->SetText(label->GetText() == "Vulkan multi-window OK"
                                       ? "Button clicked in EXTERNAL window"
                                       : "Vulkan multi-window OK");
                });
                body->AddChild(button);
            }
        }

        // Fase 3 (2026-09-02) — segunda ventana externa (verificación 3.6.2):
        // confirma que el frame lógico único escala con 2 ventanas externas
        // (una UI + otra UI). Cada ventana tiene su propio SwapchainTarget, su
        // propio command pool y su propio canvas; todas renderizan ANTES de
        // EndFrame() del principal (regla 3.1).
        if (m_Backend) {
            m_TestWindow2 = new Leir::UIWindowExternal(m_Backend.get(), "Leir Test Window 2");
            m_TestWindow2->Show();
            if (Leir::UICanvas* c2 = m_TestWindow2->GetCanvas()) {
                auto* body2 = new Leir::UIPanel();
                body2->SetName("TestWinBody2");
                body2->SetColor({0.12f, 0.30f, 0.22f, 1.0f});
                body2->GetRect().anchor = {0.0f, 0.0f, 1.0f, 1.0f};
                body2->GetRect().offset = {0.0f, 0.0f, 0.0f, 0.0f};
                c2->AddChild(body2);

                auto* label2 = new Leir::UILabel();
                label2->SetName("TestWinLabel2");
                label2->SetText("External window #2 — multi-window OK");
                label2->SetFont(m_FontSmall.get());
                label2->SetColor({0.85f, 0.95f, 0.9f, 1.0f});
                label2->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
                label2->GetRect().offset = {-140.0f, -10.0f, 140.0f, 12.0f};
                body2->AddChild(label2);
            }
        }
        m_Toolbar = new ToolbarPanel();
        m_Toolbar->SetName("TransformToolbar");
        m_Toolbar->SetFont(m_FontSmall.get());
        m_Toolbar->GetRect().anchor = {0.0f, 0.0f, 1.0f, 0.0f};
        m_Toolbar->GetRect().offset = {0.0f, kTopMenuBarHeight, 0.0f, kTopMenuBarHeight + kTopToolbarHeight};
        m_Toolbar->SetOnToolChanged([this](ToolbarPanel::Tool t) {
            m_TransformGizmo.SetTool(static_cast<TransformGizmo::Tool>(t));
            m_Toolbar->SetScaleMode(m_TransformGizmo.IsScaleTool());
        });
        m_Toolbar->SetOnSpaceChanged([this](ToolbarPanel::Space s) {
            m_TransformGizmo.SetSpace(static_cast<TransformGizmo::Space>(s));
        });
        m_Canvas->AddChild(m_Toolbar);

        m_TransformGizmo.SetTool(TransformGizmo::Tool::Translate);
        m_TransformGizmo.SetSpace(TransformGizmo::Space::Global);

        // Viewport panel (rendered from the shared RenderTexture)
        m_ViewportPanel = new Leir::UIViewportPanel();
        m_ViewportPanel->SetName("Viewport");
        m_ViewportPanel->SetRenderTexture(m_ViewportRT.get());

        // Grid LOD debug HUD: an overlay label pinned to the viewport's
        // top-right corner. It is a CHILD of the viewport panel so the editor's
        // hover->ancestor walk still finds the viewport (camera controls keep
        // working over it); SetOverlayLayer routes it to the debug batch so it
        // draws ABOVE the RenderTexture viewport quad. Because the viewport
        // panel is a Free-layout element, its ComputeFreeLayout ADDS the
        // panel's computed x/y into child offsets each frame � so the label's
        // offset is re-pinned every frame in OnUpdate, never accumulated.
        m_GridLodLabel = new Leir::UILabel();
        m_GridLodLabel->SetName("GridLodDebug");
        m_GridLodLabel->SetFont(m_FontSmall.get());
        m_GridLodLabel->SetColor({0.6f, 0.95f, 0.6f, 1.0f});
        m_GridLodLabel->SetOverlayLayer(true);
        m_GridLodLabel->GetRect().anchor = {1.0f, 0.0f, 1.0f, 0.0f}; // top-right
        m_GridLodLabel->GetRect().offset = {-280.0f, 8.0f, -8.0f, 100.0f};
        m_GridLodLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
        m_ViewportPanel->AddChild(m_GridLodLabel);

        // Hierarchy panel (left dock pane) — real scene hierarchy (Fase 0.2):
        // a virtualized UITreeView grouped by family, with per-family icons.
        // Family/selection/rename/drag wiring is added in later fases; this
        // step populates and refreshes the tree from the active scene.
        auto* hierarchy = new HierarchyPanel();
        m_HierarchyPanel = hierarchy;
        hierarchy->SetFont(m_FontSmall.get());
        hierarchy->SetBackend(m_Backend.get());
        hierarchy->SetContentScale(GetContentScale());

        // Fase 0.2 Paso 3: Hierarchy -> gizmo + inspector. Primary = the most
        // recently selected Object3D (reverse scan); empty = deselect. The
        // m_SyncingHierarchySelection guard prevents feedback loops (the gizmo
        // change would otherwise bounce back via the OnUpdate scene->hierarchy
        // sync). Gizmo/inspector are Object3D-only for now (Object2D/UI sync
        // arrives with the 2D/UI gizmos in P1).
        hierarchy->SetOnSelectionChanged([this](const std::vector<Leir::CoreObject*>& objs) {
            if (m_SyncingHierarchySelection) return;
            m_SyncingHierarchySelection = true;
            Leir::Object3D* primary = nullptr;
            for (auto it = objs.rbegin(); it != objs.rend(); ++it)
                if (auto* o3d = dynamic_cast<Leir::Object3D*>(*it)) { primary = o3d; break; }
            m_TransformGizmo.SetSelected(primary);
            m_InspectorTransformPanel->SetTargetObject(primary);
            m_LastGizmoSelection = primary;
            m_SyncingHierarchySelection = false;
        });

        // "+" context menu (UIContextMenu): create an Object3D cube like the
        // scene's one, at the origin. Object2D/UIElement items are placeholders.
        hierarchy->SetOnAddObject3D([this]() {
            auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
            if (!scene || !m_Mesh || !m_Material) return;
            auto* cube = scene->CreateObject3D("Cube");
            cube->GetTransform().SetLocalPosition({0.0f, 0.0f, 0.0f});
            auto& renderer = cube->AddComponent<Leir::MeshRenderer>();
            renderer.SetMesh(m_Mesh);
            renderer.SetMaterial(m_Material);
        });

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

        // Default selection for the transform gizmo: the Cube.
        m_TransformGizmo.SetSelected(
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

        // Gizmo/selection recorder (DBG panel): records camera + viewport input
        // + gizmo/object state to records/record_gizmo_log.txt while armed.
        m_GizmoLogPanel = new GizmoLogPanel();
        m_GizmoLogPanel->SetName("GizmoLogPanel");
        m_GizmoLogPanel->SetFont(m_FontSmall.get());

        m_TreeViewDebugPanel = new TreeViewDebugPanel();
        m_TreeViewDebugPanel->SetName("TreeViewDebugPanel");
        m_TreeViewDebugPanel->SetFont(m_FontSmall.get());
        ApplyTreeIcons();

        // Gizmo-line live knobs (color / alpha / width), "Test2" tab.
        m_GizmoTestPanel = new GizmoLineTestPanel();
        m_GizmoTestPanel->SetName("GizmoTestPanel");
        m_GizmoTestPanel->SetFont(m_FontSmall.get());

        // Grid LOD live knobs + Manual/Auto toggle, "Grid" tab.
        m_GridPanel = new GridPanel();
        m_GridPanel->SetName("GridPanel");
        m_GridPanel->SetFont(m_FontSmall.get());

        // Register dockable panels (core ones are not closeable)
        m_DockManager->RegisterPanel("Hierarchy", "Hierarchy", hierarchy, false);
        m_DockManager->RegisterPanel("Viewport", "Viewport", m_ViewportPanel, false);
        m_DockManager->RegisterPanel("Inspector", "Inspector", inspector, false);
        m_DockManager->RegisterPanel("TestPanel", "Test", m_TestPanel, true);
        m_DockManager->RegisterPanel("GizmoTestPanel", "Test2", m_GizmoTestPanel, true);
        m_DockManager->RegisterPanel("GridPanel", "Grid", m_GridPanel, true);
        m_DockManager->RegisterPanel("CameraTestPanel", "Camera", m_CameraTestPanel, true);
        m_DockManager->RegisterPanel("DebugTextPanel", "Debug Text", m_DebugTextPanel, true);
        m_DockManager->RegisterPanel("TextAreaDebugPanel", "Text Area", m_TextAreaDebugPanel, true);
        m_DockManager->RegisterPanel("TextAreaWrapPanel", "Text Area Wrap", m_TextAreaWrapPanel, true);
        m_DockManager->RegisterPanel("ConsolePanel", "Console", m_ConsolePanel, true);
        m_DockManager->RegisterPanel("DebugPanel", "Debug Panel", m_DebugPanel, true);
        m_DockManager->RegisterPanel("GizmoLogPanel", "DBG", m_GizmoLogPanel, true);
        m_DockManager->RegisterPanel("TreeViewDebugPanel", "TreeView", m_TreeViewDebugPanel, true);

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

        // Fase E — Detach to Window: when a panel is detached the dock manager
        // calls this to create an external window hosting the SAME content
        // reference (no copy). The window closes -> re-dock via ReattachPanel.
        m_DockManager->SetOnPanelDetached([this](Leir::DockPanel* panel) {
            CreateDetachedWindow(panel);
        });

#ifdef LEIR_EDITOR_SLANG
        // ---- Shader hot-reload + export wiring (Plan A) ----
        // m_ShaderCompiler was created earlier in OnInit (before the Shader,
        // so the reflection sidecars already exist); here we only wire up the
        // per-frame poller and the DebugPanel buttons.
        if (m_ShaderCompiler && m_ShaderCompiler->IsAvailable()) {
            m_HotReloader = std::make_unique<ShaderHotReloader>();
            m_HotReloader->SetCompiler(m_ShaderCompiler.get());
            m_HotReloader->SetOnReload([this]() {
                if (m_Shader)
                    m_Shader->Reload();
                if (m_Material && m_ViewportRT)
                    m_Material->ReloadShaders(m_ViewportRT->GetRenderPass());
                if (m_RenderPipeline)
                    m_RenderPipeline->ReloadSpritePipeline();
                if (m_UIRenderer)
                    m_UIRenderer->ReloadShaders();
            });
            m_HotReloader->Snap();

            // DebugPanel: Export writes all targets to LEIR_SHADER_EXPORT_DIR;
            // Reload recompiles + reloads every pipeline.
            if (m_DebugPanel) {
                m_DebugPanel->SetOnExportShaders([this]() {
                    auto lines = ShaderExporter::ExportAll(m_ShaderCompiler.get());
                    for (const auto& line : lines)
                        Leir::XConsole::Println("{}", line);
                });
                m_DebugPanel->SetOnReloadShaders([this]() {
                    if (m_HotReloader)
                        m_HotReloader->ForceReload(ActiveShaderTarget());
                });
            }
        }
#endif

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
        // The minimized Stats panel pins to the 3D viewport's bottom-right corner.
        m_DebugOverlay->SetViewportRectProvider([this]() {
            return m_ViewportPanel ? m_ViewportPanel->GetComputedRect()
                                   : Leir::Vector4{};
        });

        Leir::XConsole::Println("Scene hierarchy created with dock system");
    }

    void OnUpdate(float deltaTime) override
    {
        // Deferred delete for the About window: when it closes (OK button),
        // UIWindowExternal::Close() destroys the native window but the C++ object
        // is editor-owned. Delete it next frame to avoid a use-after-free inside
        // the button's own click callback.
        if (m_AboutWindow && !m_AboutWindow->IsVisible()) {
            delete m_AboutWindow;
            m_AboutWindow = nullptr;
        }

        // Fase E — deferred delete of detached windows: when one closes its
        // OnClosed already re-docked the panel; free the window object here
        // (outside any dispatch). Erase-remove to keep the vector stable.
        for (size_t i = m_DetachedWindows.size(); i-- > 0;) {
            Leir::UIWindowExternal* w = m_DetachedWindows[i];
            if (w && !w->IsVisible()) {
                delete w;
                m_DetachedWindows.erase(m_DetachedWindows.begin() + i);
            }
        }

        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // Editor camera controls (right-click + WASD free-fly). The viewport is
        // now the content of a dock pane, so walk ancestors up to it. When the
        // viewport is DETACHED to an external window, the hover lives in that
        // window's canvas (its own content scale), not the main canvas — check
        // every detached window's canvas too.
        auto* hovered = m_Canvas->GetHoveredElement();
        bool inViewport = false;
        if (m_ViewportPanel) {
            for (Leir::UIElement* e = hovered; e; e = e->GetParent()) {
                if (e == m_ViewportPanel) { inViewport = true; break; }
            }
            if (!inViewport) {
                for (auto* w : m_DetachedWindows) {
                    if (!w) continue;
                    Leir::UICanvas* c = w->GetCanvas();
                    if (!c) continue;
                    for (Leir::UIElement* e = c->GetHoveredElement(); e; e = e->GetParent()) {
                        if (e == m_ViewportPanel) { inViewport = true; break; }
                    }
                    if (inViewport) break;
                }
            }
        }
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
                // escena → EditorCamera (panel edits). Use the exact inverse of
                // GetRotation() (roll=0) — Quaternion::ToEuler/glm::eulerAngles
                // returns an Euler ALIAS (yaw->180-yaw, roll->±180) for
                // |yaw|>90, which used to corrupt the camera on the readback.
                auto& t = cameraObj->GetTransform();
                m_EditorCamera.SetPosition(t.GetLocalPosition());
                m_EditorCamera.SetFromRotation(t.GetLocalRotation());
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

        // Grid LOD debug HUD: refresh the label text from the grid's debug state
        // and re-pin its offset. The label is a child of the Free-layout viewport
        // panel, whose ComputeFreeLayout adds the panel's absolute x/y to child
        // offsets every frame � resetting the anchor offset here (right before
        // UpdateLayout) makes the net position stable instead of drifting.
        if (m_GridLodLabel && m_Grid) {
            // Grid tuning comes from the GridPanel. Manual: the inputs drive the
            // grid. Auto: the inputs are greyed out (do NOT influence); the grid
            // uses the cell-fade/width defaults + an AUTO horizon fade computed
            // from the camera height, which is written back into the inputs so
            // you can see what auto chose.
            if (m_GridPanel) {
                if (m_GridPanel->IsManual()) {
                    m_Grid->SetFadeThresholds(m_GridPanel->GetGridFadeStartPx(),
                                              m_GridPanel->GetGridFadeEndPx());
                    m_Grid->SetChunkWidth(m_GridPanel->GetGridChunkWidth());
                    m_Grid->SetHorizonFade(m_GridPanel->GetGridHorizonFadeStart(),
                                           m_GridPanel->GetGridHorizonFadeEnd());
                } else {
                    float hs, he;
                    GridPanel::ComputeAutoHorizon(m_Grid->GetDebugCamHeight(), hs, he);
                    m_Grid->SetFadeThresholds(15.0f, 30.0f);
                    m_Grid->SetChunkWidth(0.9f);
                    m_Grid->SetHorizonFade(hs, he);
                    m_GridPanel->SetAutoValues(15.0f, 30.0f, 0.9f, -1.0f, hs, he);
                }
            }

            // Diagnostic knobs (always applied; the toggles are read-only in auto).
            if (m_GridPanel) {
                m_Grid->SetChunkOnly(m_GridPanel->IsChunkOnly());
                m_Grid->SetDisableClip(m_GridPanel->IsDisableClip());
                m_Grid->SetThinChunks(m_GridPanel->IsThinChunks());
                for (int li = 0; li < 4; ++li)
                    m_Grid->SetLevelEnabled(li, (m_GridPanel->GetLevelMask() & (1u << li)) != 0);
                m_GridPanel->SetDiagnostics(
                    m_Grid->GetNanSkippedCount(),
                    m_Grid->GetQuadCount(),
                    EditorGrid::kMaxQuads,
                    m_Grid->GetSegCount());
            }
            m_GridLodLabel->GetRect().anchor = {1.0f, 0.0f, 1.0f, 0.0f};
            m_GridLodLabel->GetRect().offset = {-280.0f, 8.0f, -8.0f, 100.0f};
            char buf[220];
            std::snprintf(buf, sizeof(buf),
                "LOD fine %g / chunk %g\ncamH %.1f  ref %.2f px/u\nfade %.0f..%.0f px  thick %gpx  lines %u\nhorizon %.0f..%.0f  role 1u:%.2f 10u:%.2f 100u:%.2f 1000u:%.2f",
                m_Grid->GetDebugFineSpacing(), m_Grid->GetDebugChunkSpacing(),
                m_Grid->GetDebugCamHeight(), m_Grid->GetDebugRefPxPerUnit(),
                m_Grid->GetFadeStartPx(), m_Grid->GetFadeEndPx(),
                m_Grid->GetChunkWidth(),
                m_Grid->GetDebugLineCount(),
                m_Grid->GetHorizonFadeStart(), m_Grid->GetHorizonFadeEnd(),
                m_Grid->GetDebugLevelAlpha(0), m_Grid->GetDebugLevelAlpha(1),
                m_Grid->GetDebugLevelAlpha(2), m_Grid->GetDebugLevelAlpha(3));
            m_GridLodLabel->SetText(buf);
        }

        // Update UI layout on resize
        if (m_Canvas)
            m_Canvas->UpdateLayout();

        // Keep the viewport render target in sync with the actual layout size
        UpdateViewportRenderTarget();

        // ---- Transform gizmo: input + shortcuts ----
        // W/E/R switch the active tool (translate/rotate/scale) like Unity. The
        // shortcuts are suppressed while flying the camera (right/middle held:
        // W/E are also WASD/QE camera keys), while typing in a text input, or
        // while a gizmo drag is active.
        auto* focus = m_Canvas ? m_Canvas->GetFocus() : nullptr;
        const bool typing = focus && dynamic_cast<Leir::UITextInput*>(focus) != nullptr;
        if (!typing && !rightDown && !middleDown && !m_TransformGizmo.IsDragging()) {
            if (Leir::Keyboard::WasPressed(Leir::Key::W)) {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Translate);
            } else if (Leir::Keyboard::WasPressed(Leir::Key::E)) {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Rotate);
            } else if (Leir::Keyboard::WasPressed(Leir::Key::R)) {
                m_TransformGizmo.SetTool(TransformGizmo::Tool::Scale);
            } else if (Leir::Keyboard::WasPressed(Leir::Key::Q) &&
                       !m_TransformGizmo.IsScaleTool()) {
                // Toggle Global/Local (translate & rotate only; scale is always local).
                m_TransformGizmo.SetSpace(
                    m_TransformGizmo.GetSpace() == TransformGizmo::Space::Global
                        ? TransformGizmo::Space::Local
                        : TransformGizmo::Space::Global);
            }
        }
        if (m_Toolbar) {
            m_Toolbar->SetTool(static_cast<ToolbarPanel::Tool>(m_TransformGizmo.GetTool()));
            m_Toolbar->SetSpace(static_cast<ToolbarPanel::Space>(m_TransformGizmo.GetSpace()));
            m_Toolbar->SetScaleMode(m_TransformGizmo.IsScaleTool());
        }

        // Gizmo handles its own hover/drag. Picks (left press) that land on a
        // handle are consumed; otherwise the click is object selection.
        // Update runs every frame so the gizmo renders even when the mouse is
        // not over the viewport; press-to-drag is gated to inViewport below.
        bool consumed = false;
        if (m_PrimaryCamera && m_ViewportPanel) {
            TransformGizmo::Frame gframe;
            gframe.camera = m_PrimaryCamera;
            const auto& cr = m_ViewportPanel->GetComputedRect();
            gframe.viewportRect = cr;
            gframe.contentScale = GetContentScale();
            gframe.mousePos = Leir::Mouse::GetPos();
            gframe.leftPressed = inViewport && Leir::Mouse::WasPressed(Leir::PointerButton::Left);
            gframe.leftDown = Leir::Mouse::IsDown(Leir::PointerButton::Left);
            gframe.leftReleased = Leir::Mouse::WasReleased(Leir::PointerButton::Left);
            consumed = m_TransformGizmo.Update(gframe);
        }

        // Click-pick selection: left click in the viewport that did NOT hit a
        // gizmo handle selects the object under the cursor (or deselects when
        // clicking empty space).
        if (inViewport && !consumed &&
            Leir::Mouse::WasPressed(Leir::PointerButton::Left) && m_PrimaryCamera) {
            Leir::Object3D* picked = PickObjectAtCursor(scene);
            m_TransformGizmo.SetSelected(picked);
        }

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
        if (m_TreeViewDebugPanel)
            m_TreeViewDebugPanel->Refresh();
        if (m_HierarchyPanel)
            m_HierarchyPanel->Refresh();

        // Fase 0.2 Paso 3: scene -> hierarchy + inspector sync. When the gizmo
        // selection changed externally (viewport click-pick, gizmo pick), reflect
        // it in the hierarchy highlight and the inspector. Runs AFTER the panel
        // Refresh so the tree is built. The guard avoids echoing a programmatic
        // change back (the hierarchy handler already updated m_LastGizmoSelection).
        if (m_TransformGizmo.GetSelected() != m_LastGizmoSelection) {
            m_LastGizmoSelection = m_TransformGizmo.GetSelected();
            if (!m_SyncingHierarchySelection) {
                m_SyncingHierarchySelection = true;
                if (m_HierarchyPanel) {
                    std::vector<Leir::CoreObject*> objs;
                    if (m_LastGizmoSelection) objs.push_back(m_LastGizmoSelection);
                    m_HierarchyPanel->SetSelectedObjects(objs);
                }
                if (m_InspectorTransformPanel)
                    m_InspectorTransformPanel->SetTargetObject(m_LastGizmoSelection);
                m_SyncingHierarchySelection = false;
            }
        }

#ifdef LEIR_EDITOR_SLANG
        // Shader hot-reload poll (cheap: one stat per .slang file per frame).
        if (m_HotReloader)
            m_HotReloader->Update(ActiveShaderTarget());
#endif
    }

    void OnRender() override
    {
        // BeginFrame(true) skips the swapchain 3D render pass
        if (!m_Backend || !m_Backend->BeginFrame(true)) return;

        auto cmd = m_Backend->GetCurrentCommandBuffer();
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();

        // 1. Scene graph: render 3D scene + sprites to the offscreen
        //    RenderTexture. The graph owns the render pass; the backend
        //    transitions the RT attachments automatically in CmdExecuteGraph.
        if (m_ViewportRT && scene) {
            Leir::RHI::RHIClearValue clearColor;
            clearColor.color = {0.15f, 0.15f, 0.2f, 1.0f};
            m_SceneGraph.Clear();
            m_ViewportRT->BeginRender(m_SceneGraph, clearColor, 1.0f);
            m_RenderPipeline->Render(m_SceneGraph, scene);
            // Ground grid AFTER the scene objects (depth-tested so objects
            // occlude the lines underneath). Procedural line grid using the
            // same constant-pixel-width gizmo technique as the Test2 line
            // (Grid.vert/frag): generated every frame, recentered on the
            // camera's XZ, minor 1u lines fade by distance (Unity-style) while
            // the 10u chunk lines reach the horizon.
            if (m_Grid && m_PrimaryCamera) {
                m_PrimaryCamera->RecalculateViewMatrix();
                auto* camOwner = m_PrimaryCamera->GetOwner();
                const Leir::Vector3 camPos =
                    camOwner ? camOwner->GetTransform().GetWorldPosition()
                             : Leir::Vector3(0.0f, 0.0f, 0.0f);
                m_Grid->Render(m_SceneGraph, m_PrimaryCamera->GetViewProjectionMatrix(),
                    camPos, (float)m_ViewportRT->GetWidth(),
                    (float)m_ViewportRT->GetHeight(),
                    (m_GridPanel && m_GridPanel->IsManual())
                        ? m_GridPanel->GetGridDensityOverride() : -1.0f);
            }
            // Gizmos on top of the scene (depth-tested), one draw call for all.
            if (m_Gizmos && m_PrimaryCamera) {
                DrawGizmoShowcase();
                m_Gizmos->Render(m_SceneGraph, m_PrimaryCamera->GetViewProjectionMatrix(),
                    (float)m_ViewportRT->GetWidth(), (float)m_ViewportRT->GetHeight());
            }
            m_ViewportRT->EndRender(m_SceneGraph);
            m_Backend->CmdExecuteGraph(cmd, m_SceneGraph);
        }

        // 2. UI graph: draws into the native swapchain overlay pass (begun by
        //    BeginSwapchainOverlay) � no pass records, just draw records.
        m_Backend->BeginSwapchainOverlay();
        if (m_UIRenderer && m_Canvas) {
            m_UIGraph.Clear();
            m_UIRenderer->Render(m_UIGraph, m_Canvas.get());
            m_Backend->CmdExecuteGraph(cmd, m_UIGraph);
        }

        // Fase D — render the external test window (its own swapchain/device).
        // Fase 1+2 (2026-09-02): la externa debe renderizar ANTES de EndFrame()
        // del principal. Renderizar DESPUÉS de EndFrame() causaba que el submit
        // de la externa en el mismo queue desincronizara el fence del device
        // principal, produciendo un write-after-read hazard en el buffer del
        // grid (líneas chunk en posiciones random). Verificado por el usuario.
        if (m_TestWindow)
            m_TestWindow->RenderFrame();
        if (m_TestWindow2)
            m_TestWindow2->RenderFrame();
        if (m_AboutWindow)
            m_AboutWindow->RenderFrame();

        // Fase E — render detached dock panel windows (same frame-logical rule:
        // before the main EndFrame, sharing the backend's queue).
        for (auto* w : m_DetachedWindows)
            if (w) w->RenderFrame();

        m_Backend->EndFrame();
    }

    // Sample gizmos to exercise the gizmo renderer (removable): origin
    // tri-axis, the Cube's wireframe bounding box, and a ground ring.
    // The PHASE-1 validation lines + thickness/alpha fans were removed once
    // the gizmo technique was verified; the grid now provides the reference
    // axes. Only the live "Test2" line (color/alpha/width knobs) remains.
    void DrawGizmoShowcase()
    {
        if (!m_Gizmos)
            return;
        m_Gizmos->BeginFrame();

        // Live line controlled by the "Test2" dock panel (color/alpha/width).
        if (m_GizmoTestPanel) {
            m_Gizmos->DrawLine(m_GizmoTestPanel->GetStart(),
                m_GizmoTestPanel->GetEnd(), m_GizmoTestPanel->GetColor(),
                m_GizmoTestPanel->GetWidth());
        }

        // Selection highlight: violet wireframe around the selected object's
        // world-space AABB (interim feedback until the outline shader phase).
        if (m_TransformGizmo.GetSelected()) {
            auto* sel = m_TransformGizmo.GetSelected();
            const auto& bmin = sel->GetBoundsMin();
            const auto& bmax = sel->GetBoundsMax();
            Leir::Matrix4x4 m = sel->GetTransform().GetLocalToWorldMatrix();
            Leir::Vector3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
            for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) for (int z = 0; z < 2; ++z) {
                Leir::Vector3 c(x ? bmax.x : bmin.x, y ? bmax.y : bmin.y, z ? bmax.z : bmin.z);
                Leir::Vector3 w = m.MultiplyPoint3x4(c);
                mn.x = std::min(mn.x, w.x); mn.y = std::min(mn.y, w.y); mn.z = std::min(mn.z, w.z);
                mx.x = std::max(mx.x, w.x); mx.y = std::max(mx.y, w.y); mx.z = std::max(mx.z, w.z);
            }
            Leir::Vector3 center = (mn + mx) * 0.5f;
            Leir::Vector3 size = mx - mn;
            m_Gizmos->DrawBox(center, size, {0.8f, 0.45f, 1.0f, 1.0f}, 2.0f);
        }

        // Transform gizmo (translate/rotate/scale handles) on the selection.
        m_TransformGizmo.Draw(*m_Gizmos,
            m_PrimaryCamera ? m_PrimaryCamera->GetViewProjectionMatrix()
                            : Leir::Matrix4x4::Identity(),
            (float)m_ViewportRT->GetWidth(), (float)m_ViewportRT->GetHeight());
    }

    void OnShutdown() override
    {
        auto tStart = std::chrono::steady_clock::now();
        auto elapsedMs = [&]() -> double {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - tStart).count();
        };
        Leir::XConsole::Println("Editor shutting down");
        // HARDENING (2026-08-24): wait for the GPU to finish before destroying
        // any D3D12 resource (grid/gizmo pipelines, viewport RT, ...). Releasing
        // pipelines while the GPU may still reference them made the D3D12 debug
        // layer raise 0x87D (device-removed) at teardown (~240 accumulated crash
        // entries in crash_diagnostics.log). The earlier RenderTexture teardown
        // crash was the same class, fixed the same way. Cost: ~0-16ms once.
        // The external window must die BEFORE the backend (it shares the device).
        if (m_TestWindow) {
            delete m_TestWindow;
            m_TestWindow = nullptr;
        }
        if (m_TestWindow2) {
            delete m_TestWindow2;
            m_TestWindow2 = nullptr;
        }
        if (m_AboutWindow) {
            delete m_AboutWindow;
            m_AboutWindow = nullptr;
        }
        // Fase E — detached dock panel windows share the backend device; free
        // them BEFORE WaitIdle/backend teardown. Their content is reparented
        // back out by ~UIWindowExternal (canvas dtor nulls child parents, it
        // does not delete them), so the editor-owned panel contents below are
        // still freed exactly once by the DeleteUiSubtree calls.
        for (auto* w : m_DetachedWindows)
            delete w;
        m_DetachedWindows.clear();
        if (m_Backend)
            m_Backend->WaitIdle();
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
        DeleteUiSubtree(m_GizmoTestPanel);
        m_GizmoTestPanel = nullptr;
        DeleteUiSubtree(m_GridPanel);
        m_GridPanel = nullptr;
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
        DeleteUiSubtree(m_GizmoLogPanel); // dtor closes the log file if recording
        m_GizmoLogPanel = nullptr;
        DeleteUiSubtree(m_TreeViewDebugPanel);
        m_TreeViewDebugPanel = nullptr;
        DeleteUiSubtree(m_Toolbar);
        m_Toolbar = nullptr;
        DeleteUiSubtree(m_MenuBar);
        m_MenuBar = nullptr;
        m_InspectorTransformPanel = nullptr; // freed via m_InspectorPanel above
        Leir::XConsole::Debug("[Timing] UI subtrees freed: {:.1f} ms", elapsedMs());
        // Destroy the ground grid and gizmos before the viewport RT they target.
        m_Grid.reset();
        m_Gizmos.reset();
        m_PrimaryCamera = nullptr;
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
        // Re-rasterize the font atlases at the new DPI (in place, so all
        // Font* holders stay valid) for crisp text on the new scale.
        if (m_Font) m_Font->SetContentScale(GetContentScale());
        if (m_FontSmall) m_FontSmall->SetContentScale(GetContentScale());
        // Re-load tree icons at the new DPI (cache keys on the scale) so they stay crisp.
        ApplyTreeIcons();
        // Re-load the menu submenu arrow at the new DPI.
        ApplyMenuIcons();
        if (m_HierarchyPanel)
            m_HierarchyPanel->SetContentScale(GetContentScale()); // reloads its icons + rebuilds
    }

private:
#ifdef LEIR_EDITOR_SLANG
    Leir::RHI::ShaderTarget ActiveShaderTarget() const
    {
        // The bytecode the engine reads is .spv (Vulkan) or .dxil (D3D12).
        const char* ext = m_Backend ? m_Backend->GetShaderFileExtension() : ".spv";
        return (ext && std::string(ext) == ".dxil")
            ? Leir::RHI::ShaderTarget::DXIL
            : Leir::RHI::ShaderTarget::SpirV;
    }
#endif

    // Gizmo-log recorder hook for pointer events: log a POINTER line plus the
    // full gizmo/selection state (camera, hover, drag, object transform) so the
    // log shows exactly what the gizmo picked under the cursor.
    void OnInputEvent(const char* kind, int action, int button,
                      float px, float py, float dx, float dy)
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s action=%d btn=%d pos=(%.1f,%.1f) d=(%.1f,%.1f)",
            kind, action, button, px, py, dx, dy);
        RecordGizmoLog(buf);
    }

    // Appends the current gizmo/selection state to the log (only meaningful
    // while the recorder is armed). Includes camera, gizmo tool/space/hover/
    // drag and the selected object transform — the full context of the event.
    void RecordGizmoLog(const char* eventLine)
    {
        if (!m_GizmoLogPanel || !m_GizmoLogPanel->IsRecording())
            return;
        if (eventLine)
            m_GizmoLogPanel->RecordLine(eventLine);

        Leir::Vector3 camPos = Leir::Vector3::Zero();
        if (m_PrimaryCamera && m_PrimaryCamera->GetOwner()) {
            camPos = m_PrimaryCamera->GetOwner()->GetTransform().GetWorldPosition();
        }
        Leir::Vector2 mpos = Leir::Mouse::GetPos();

        char buf[512];
        Leir::Object3D* sel = m_TransformGizmo.GetSelected();
        if (sel) {
            auto& t = sel->GetTransform();
            auto p = t.GetLocalPosition();
            auto e = Leir::Quaternion::ToEuler(t.GetLocalRotation());
            auto sc = t.GetLocalScale();
            const char* tool =
                m_TransformGizmo.GetTool() == TransformGizmo::Tool::Translate ? "T" :
                m_TransformGizmo.GetTool() == TransformGizmo::Tool::Rotate ? "R" :
                m_TransformGizmo.GetTool() == TransformGizmo::Tool::Scale ? "S" : "?";
            const char* space =
                m_TransformGizmo.GetSpace() == TransformGizmo::Space::Global ? "G" : "L";
            std::snprintf(buf, sizeof(buf),
                "  cam=(%.3f,%.3f,%.3f) mouse=(%.1f,%.1f) tool=%s space=%s hover=%s drag=%s "
                "SEL '%s' pos=(%.4f,%.4f,%.4f) rot=(%.2f,%.2f,%.2f) scale=(%.4f,%.4f,%.4f)",
                camPos.x, camPos.y, camPos.z, mpos.x, mpos.y, tool, space,
                m_TransformGizmo.GetHoverName(), m_TransformGizmo.GetDragName(),
                sel->GetName().c_str(), p.x, p.y, p.z, e.x, e.y, e.z, sc.x, sc.y, sc.z);
        } else {
            std::snprintf(buf, sizeof(buf),
                "  cam=(%.3f,%.3f,%.3f) mouse=(%.1f,%.1f) SEL <none>",
                camPos.x, camPos.y, camPos.z, mpos.x, mpos.y);
        }
        m_GizmoLogPanel->RecordLine(buf);
    }

    // Raycast the viewport cursor against scene objects (world-space AABB of
    // their mesh bounds). Returns the nearest hit, or nullptr (empty click ->
    // deselect). Minimal object picking for the gizmo phase.
    Leir::Object3D* PickObjectAtCursor(Leir::Scene* scene)
    {
        if (!scene || !m_ViewportPanel || !m_PrimaryCamera)
            return nullptr;
        const auto& cr = m_ViewportPanel->GetComputedRect();
        const float vw = std::max(cr.z, 1.0f);
        const float vh = std::max(cr.w, 1.0f);
        Leir::Vector2 mp = Leir::Mouse::GetPos();
        const float ndcX = ((mp.x - cr.x) / vw) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((mp.y - cr.y) / vh) * 2.0f;
        const Leir::Matrix4x4 invVP = m_PrimaryCamera->GetViewProjectionMatrix().Inverse();
        auto unproj = [&](float z) -> Leir::Vector3 {
            const Leir::Vector4 c = invVP * Leir::Vector4(ndcX, ndcY, z, 1.0f);
            return Leir::Vector3(c.x / c.w, c.y / c.w, c.z / c.w);
        };
        const Leir::Vector3 origin = unproj(-1.0f);
        const Leir::Vector3 dir = (unproj(1.0f) - origin).Normalized();

        float bestT = 1e30f;
        Leir::Object3D* best = nullptr;
        for (Leir::CoreObject* objPtr : scene->GetRenderables()) {
            Leir::Object3D* obj = dynamic_cast<Leir::Object3D*>(objPtr);
            if (!obj)
                continue;
            if (!obj->GetComponent<Leir::MeshRenderer>())
                continue;
            const auto& bmin = obj->GetBoundsMin();
            const auto& bmax = obj->GetBoundsMax();
            // World-space AABB: transform the 8 corners of the local bounds.
            Leir::Matrix4x4 m = obj->GetTransform().GetLocalToWorldMatrix();
            Leir::Vector3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
            for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) for (int z = 0; z < 2; ++z) {
                Leir::Vector3 c(x ? bmax.x : bmin.x, y ? bmax.y : bmin.y, z ? bmax.z : bmin.z);
                Leir::Vector3 w = m.MultiplyPoint3x4(c);
                mn.x = std::min(mn.x, w.x); mn.y = std::min(mn.y, w.y); mn.z = std::min(mn.z, w.z);
                mx.x = std::max(mx.x, w.x); mx.y = std::max(mx.y, w.y); mx.z = std::max(mx.z, w.z);
            }
            // Slab ray/AABB intersection.
            float tmin = -1e30f, tmax = 1e30f;
            bool ok = true;
            auto axis = [&](float o, float d, float lo, float hi, int i) {
                (void)i;
                if (std::fabs(d) < 1e-12f) {
                    if (o < lo || o > hi) ok = false;
                    return;
                }
                float t1 = (lo - o) / d, t2 = (hi - o) / d;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) ok = false;
            };
            axis(origin.x, dir.x, mn.x, mx.x, 0);
            axis(origin.y, dir.y, mn.y, mx.y, 1);
            axis(origin.z, dir.z, mn.z, mx.z, 2);
            if (!ok || tmax < 0.0f)
                continue;
            if (tmin < bestT) { bestT = tmin; best = obj; }
        }
        return best;
    }

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
                cam->SetPerspective(60.0f, (float)w / (float)h, 0.1f, 2000.0f);
        }

        m_PendingW = w;
        m_PendingH = h;
        m_PendingFw = fw;
        m_PendingFh = fh;
        m_HasPendingResizeLog = true;
    }

    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;

    // Fase D — external window test (UIWindowExternal). A second OS window that
    // renders through the shared Vulkan device, proving the multi-window path.
    Leir::UIWindowExternal* m_TestWindow = nullptr;
    // Fase 3 — segunda ventana externa (verificación 3.6.2: frame lógico con 2 ventanas).
    Leir::UIWindowExternal* m_TestWindow2 = nullptr;
    // About dialog (external window, Fase D).
    AboutWindow* m_AboutWindow = nullptr;
    // Fase F — internal floating window test (UIWindowInternal). Floating inside
    // the main canvas with chrome (title bar, buttons, drag, resize, modal).
    Leir::UIWindowInternal* m_InternalWindow = nullptr;
    // Fase E — detached dock panels hosted in external windows (created by
    // CreateDetachedWindow; deleted deferred in OnUpdate when !IsVisible).
    std::vector<Leir::UIWindowExternal*> m_DetachedWindows;

    // Per-frame command graphs (see GCommandGraph): the scene graph owns the
    // RenderTexture pass; the UI graph records draws into the swapchain
    // overlay pass. Both are executed by the backend each frame.
    Leir::RHI::GCommandGraph m_SceneGraph;
    Leir::RHI::GCommandGraph m_UIGraph;

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
    HierarchyPanel* m_HierarchyPanel = nullptr;
    Leir::UIPanel* m_InspectorPanel = nullptr;
    ToolbarPanel* m_Toolbar = nullptr;
    Leir::UIMenuBar* m_MenuBar = nullptr;

    // Transform gizmo system (toolbar + gizmos + selection)
    TransformGizmo m_TransformGizmo;
    // Fase 0.2 Paso 3: bidirectional hierarchy selection sync (guards + last seen).
    bool m_SyncingHierarchySelection = false;
    Leir::Object3D* m_LastGizmoSelection = nullptr;

    // Viewport system
    std::unique_ptr<Leir::RenderTexture> m_ViewportRT;
    Leir::UIViewportPanel* m_ViewportPanel = nullptr;
    Leir::UILabel* m_GridLodLabel = nullptr; // LOD debug HUD (child of the viewport)
    EditorCamera m_EditorCamera;
    std::unique_ptr<EditorGrid> m_Grid;
    std::unique_ptr<GizmoRenderer> m_Gizmos;
    Leir::Camera* m_PrimaryCamera = nullptr;

    UITestPanel* m_TestPanel = nullptr;
    GizmoLineTestPanel* m_GizmoTestPanel = nullptr;
    GridPanel* m_GridPanel = nullptr;
    CameraTestPanel* m_CameraTestPanel = nullptr;
    DebugTextPanel* m_DebugTextPanel = nullptr;
    ConsolePanel* m_ConsolePanel = nullptr;
    TextAreaDebugPanel* m_TextAreaDebugPanel = nullptr;
    TextAreaWrapPanel* m_TextAreaWrapPanel = nullptr;
    DebugPanel* m_DebugPanel = nullptr;
    GizmoLogPanel* m_GizmoLogPanel = nullptr;
    TreeViewDebugPanel* m_TreeViewDebugPanel = nullptr;
    InspectorTransformPanel* m_InspectorTransformPanel = nullptr;

#ifdef LEIR_EDITOR_SLANG
    std::unique_ptr<Leir::RHI::SlangShaderCompiler> m_ShaderCompiler;
    std::unique_ptr<ShaderHotReloader> m_HotReloader;
#endif

    // Fase 0.1: sample icons for the TreeViewDebugPanel, colored by depth via the
    // UITextureCache (decode-once by path+scale hash). Re-applied on content-scale
    // change so icons stay crisp at any DPI (cache keys on the scale). The real
    // HierarchyPanel will set icons per object family instead.
    void ApplyTreeIcons()
    {
        if (!m_TreeViewDebugPanel) return;
        if (auto* tv = m_TreeViewDebugPanel->GetTreeView()) {
            const float scale = GetContentScale();
            tv->SetIconsEnabled(true);
            auto icon3D = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/object3d.png", scale);
            auto icon2D = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/object2d.png", scale);
            auto iconUI = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/uielement.png", scale);
            std::function<void(Leir::UITreeViewItem*, int)> color = [&](Leir::UITreeViewItem* it, int depth) {
                it->SetIcon(depth == 0 ? icon3D : (depth == 1 ? icon2D : iconUI));
                for (auto* c : it->GetTreeChildren()) color(c, depth + 1);
            };
            for (auto* r : tv->GetRoots()) color(r, 0);
        }
    }

    // Submenu arrow icon for the UIMenuBar dropdowns (assets/icons/arrow_right.png,
    // 13x13, cached by UITextureCache). Re-applied on content-scale change.
    void ApplyMenuIcons()
    {
        if (!m_MenuBar) return;
        const float scale = GetContentScale();
        auto arrowRight = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/arrow_right.png", scale);
        m_MenuBar->SetSubMenuIcon(arrowRight);
    }

    // Opens a floating internal window with chrome (title bar, close, drag,
    // resize, modal). This tests the UIWindowInternal + Fase C chrome path.
    void CreateDetachedWindow(Leir::DockPanel* panel)
    {
        if (!panel || !panel->content || !m_Backend)
            return;

        // The external window hosts the panel's SAME content reference (no
        // copy). The dock manager already removed the panel from its pane, so
        // panel->content is detached; reparent it into the window's canvas.
        auto* win = new Leir::UIWindowExternal(m_Backend.get(), panel->title);
        win->Show();

        if (Leir::UICanvas* c = win->GetCanvas()) {
            panel->content->SetActive(true);
            panel->content->SetSizePolicy(Leir::SizePolicy::Fill);
            panel->content->GetRect().anchor = Leir::AnchorSet::Stretch();
            panel->content->GetRect().offset = {};
            c->AddChild(panel->content);
        }

        // When the window closes (X button or other close path), re-dock the
        // panel into the dock tree and mark the window for deferred delete.
        win->SetOnClosed([this, panel, win]() {
            if (m_DockManager)
                m_DockManager->ReattachPanel(panel);
            // Re-dock reparents panel->content back into the dock; the window
            // is deleted next frame in OnUpdate (safe, outside any dispatch).
        });

        m_DetachedWindows.push_back(win);
        Leir::XConsole::Println("Panel '{}' detached to window", panel->title);
    }

    void ShowInternalTestWindow()
    {
        // Persistent window: create once, reuse with Show/Hide. Never delete
        // inside a UI callback (use-after-free) — Close() detaches from the
        // canvas and OnUpdate frees it next frame.
        if (!m_InternalWindow) {
            m_InternalWindow = new Leir::UIWindowInternal("Internal Test");
            m_InternalWindow->SetSize({300.0f, 200.0f});
            m_InternalWindow->SetPosition({100.0f, 100.0f});
            m_InternalWindow->SetFont(m_FontSmall.get()); // title bar text

            // Chrome icons (14x14 PNGs via UITextureCache).
            const float scale = GetContentScale();
            auto closeIcon = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/window_close.png", scale);
            auto minIcon = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/window_minimize.png", scale);
            auto maxIcon = Leir::UITextureCache::Load(m_Backend.get(), "assets/icons/window_maximize.png", scale);
            m_InternalWindow->SetWindowButtonIcons(closeIcon, minIcon, maxIcon);

            // Visible 1px border (Plan A: decorative, drawn on top of the content;
            // the resize/cursor events come from the transparent hit ring).
            m_InternalWindow->SetVisualBorderSize(1.0f);
            m_InternalWindow->SetBorderColor({0.55f, 0.58f, 0.68f, 1.0f});

            // Add some content: a label + a close button (the title bar × closes too).
            auto* body = new Leir::UIPanel();
            body->SetName("IntWinBody");
            body->SetColor({0.18f, 0.20f, 0.25f, 1.0f});
            body->GetRect().anchor = {0.0f, 0.0f, 1.0f, 1.0f};
            body->GetRect().offset = {0.0f, 0.0f, 0.0f, 0.0f};
            m_InternalWindow->SetContent(body);

            auto* label = new Leir::UILabel();
            label->SetName("IntWinLabel");
            label->SetText("Floating internal window");
            label->SetFont(m_FontSmall.get());
            label->SetColor({0.85f, 0.85f, 0.9f, 1.0f});
            label->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
            label->GetRect().offset = {-90.0f, -10.0f, 90.0f, 14.0f};
            body->AddChild(label);

            auto* btn = new Leir::UIButton();
            btn->SetName("IntWinCloseBtn");
            btn->SetText("Close");
            btn->SetFont(m_FontSmall.get());
            btn->SetColors({0.35f, 0.45f, 0.65f, 1.0f},
                           {0.45f, 0.55f, 0.75f, 1.0f},
                           {0.25f, 0.35f, 0.55f, 1.0f});
            btn->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
            btn->GetRect().anchor = {0.5f, 0.5f, 0.5f, 0.5f};
            btn->GetRect().offset = {-50.0f, 20.0f, 50.0f, 50.0f};
            // Only Close() — do NOT delete here (the window is being processed).
            btn->SetOnClick([this]() {
                if (m_InternalWindow) m_InternalWindow->Close();
            });
            body->AddChild(btn);
        }

        if (!m_InternalWindow->IsVisible())
            m_InternalWindow->ShowIn(m_Canvas.get());
        else
            m_InternalWindow->BringToFront();
    }

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
