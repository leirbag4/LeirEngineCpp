#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
#include <vector>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Rendering/VulkanDevice.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
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
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UISlider.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/ScrollView.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/UI/UIRenderer.h>

#include <spdlog/spdlog.h>

#include <memory>

class EditorApp : public Leir::CoreApplication {
public:
    EditorApp()
        : CoreApplication("LeirEngine Editor", 1280, 720)
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
        m_Material->RecreatePipeline(m_VulkanDevice->GetRenderPass());

        auto [verts, idxs] = Leir::Primitives::CreateCube();
        m_Mesh = std::make_shared<Leir::Mesh>(m_VulkanDevice.get(), verts, idxs);
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_VulkanDevice.get());

        auto& sceneManager = Leir::SceneManager::GetInstance();
        auto& scene = sceneManager.CreateScene("Main Scene");
        sceneManager.SetActiveScene(&scene);

        // Camera
        auto* cameraObj = scene.CreateObject3D("Camera");
        cameraObj->GetTransform().SetLocalPosition({0.0f, 2.0f, 5.0f});
        auto& camera = cameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)GetWidth() / (float)GetHeight(), 0.1f, 100.0f);
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

        // Sprites (unchanged)
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

        // Try loading a system font
        std::string fontPath;
        FILE* testFont = nullptr;
        if (fopen_s(&testFont, "C:/Windows/Fonts/Arial.ttf", "rb") == 0 && testFont) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/Arial.ttf";
        } else if (fopen_s(&testFont, "C:/Windows/Fonts/segoeui.ttf", "rb") == 0 && testFont) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/segoeui.ttf";
        } else if (fopen_s(&testFont, "C:/Windows/Fonts/consola.ttf", "rb") == 0 && testFont) {
            fclose(testFont);
            fontPath = "C:/Windows/Fonts/consola.ttf";
        } else {
            spdlog::warn("No system font found, text will not render");
        }

        if (!fontPath.empty()) {
            m_Font = std::make_unique<Leir::Font>(m_VulkanDevice.get(), fontPath, 16);
            m_FontTitle = std::make_unique<Leir::Font>(m_VulkanDevice.get(), fontPath, 22);
        }

        // Create canvas
        m_Canvas = std::make_unique<Leir::UICanvas>();
        m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());

        // Title
        auto* title = new Leir::UILabel();
        title->SetName("Title");
        title->SetText("LeirEngine UI Demo");
        title->SetFont(m_FontTitle.get());
        title->GetRect().anchor = Leir::AnchorSet::TopLeft();
        title->GetRect().offset = Leir::OffsetSet::All(10.0f);
        title->GetRect().offset.right = 400.0f;
        title->GetRect().offset.bottom = 50.0f;
        m_Canvas->AddChild(title);

        // Panel with vertical layout
        auto* panel = new Leir::UIElement();
        panel->SetName("Panel");
        panel->SetLayoutMode(Leir::LayoutMode::Column);
        panel->GetRect() = Leir::Rect2D::Absolute(20.0f, 60.0f, 300.0f, 300.0f);
        panel->SetPadding(8.0f, 8.0f, 8.0f, 8.0f);
        panel->SetSpacing(8.0f);
        m_Canvas->AddChild(panel);

        // Button
        auto* btn = new Leir::UIButton();
        btn->SetName("ClickBtn");
        btn->SetText("Click Me!");
        btn->SetFont(m_Font.get());
        btn->SetSizePolicy(Leir::SizePolicy::Fixed);
        btn->SetColors(
            {0.3f, 0.6f, 0.9f, 1.0f},
            {0.4f, 0.7f, 1.0f, 1.0f},
            {0.2f, 0.4f, 0.7f, 1.0f}
        );
        btn->SetOnClick([this]() {
            spdlog::info("Button clicked!");
        });
        panel->AddChild(btn);

        // Label
        auto* label = new Leir::UILabel();
        label->SetName("InfoLabel");
        label->SetText("Hello from UILabel!\nMulti-line support.");
        label->SetFont(m_Font.get());
        label->SetColor({0.8f, 0.9f, 1.0f, 1.0f});
        label->SetSizePolicy(Leir::SizePolicy::Fill);
        panel->AddChild(label);

        // Slider
        auto* slider = new Leir::UISlider();
        slider->SetName("TestSlider");
        slider->SetRange(0.0f, 100.0f);
        slider->SetValue(50.0f);
        slider->SetSizePolicy(Leir::SizePolicy::Fixed);
        slider->SetOnChange([](float v) {
            spdlog::info("Slider: {}", v);
        });
        panel->AddChild(slider);

        // Text input
        auto* input = new Leir::UITextInput();
        input->SetName("TextInput");
        input->SetFont(m_Font.get());
        input->SetPlaceholder("Type here...");
        input->SetSizePolicy(Leir::SizePolicy::Fill);
        panel->AddChild(input);

        // Bottom bar stretch across bottom
        auto* bottomBar = new Leir::UIImage();
        bottomBar->SetName("BottomBar");
        bottomBar->GetRect().anchor = {0.0f, 1.0f, 1.0f, 1.0f}; // left=0, top=1, right=1, bottom=1
        bottomBar->GetRect().offset = {0.0f, -30.0f, 0.0f, 0.0f};
        bottomBar->SetColor({0.1f, 0.1f, 0.15f, 1.0f});
        m_Canvas->AddChild(bottomBar);

        auto* statusLabel = new Leir::UILabel();
        statusLabel->SetName("StatusLabel");
        statusLabel->SetText("UI System Online");
        statusLabel->SetFont(m_Font.get());
        statusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
        statusLabel->GetRect().anchor = {0.0f, 1.0f, 0.0f, 1.0f}; // bottom-left point anchor
        statusLabel->GetRect().offset = {8.0f, -28.0f, 200.0f, 0.0f};
        m_Canvas->AddChild(statusLabel);

        m_Canvas->UpdateLayout();

        spdlog::info("Scene hierarchy created with Vulkan renderer + UI");
    }

    void OnUpdate(float deltaTime) override
    {
        (void)deltaTime;

        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (scene) {
            auto* cube = scene->FindObjectByName("Cube");
            if (cube) {
                auto& t = cube->GetTransform();
                t.SetLocalRotation(
                    glm::quat(glm::vec3(0.0f, deltaTime * 0.5f, 0.0f)) * t.GetLocalRotation());
            }
        }

        // Update UI layout on resize
        if (m_Canvas) {
            m_Canvas->SetScreenSize((float)GetWidth(), (float)GetHeight());
            m_Canvas->UpdateLayout();
        }
    }

    void OnRender() override
    {
        if (m_VulkanDevice && m_VulkanDevice->BeginFrame()) {
            VkCommandBuffer cmd = m_VulkanDevice->GetCurrentCommandBuffer();

            auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
            m_RenderPipeline->Render(cmd, scene);

            m_VulkanDevice->BeginOverlay();
            m_RenderPipeline->RenderOverlay(cmd, scene);

            // Render UI overlay
            if (m_UIRenderer && m_Canvas)
                m_UIRenderer->Render(cmd, m_Canvas.get());

            m_VulkanDevice->EndFrame();
        }
    }

    void OnShutdown() override
    {
        spdlog::info("Editor shutting down");
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
    std::unique_ptr<Leir::Font> m_FontTitle;
};

int main()
{
    spdlog::set_level(spdlog::level::trace);

    EditorApp app;
    app.Run();

    return 0;
}
