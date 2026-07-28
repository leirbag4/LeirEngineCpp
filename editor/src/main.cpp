#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
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
#include <LeirEngine/Components/MeshRenderer.h>
#include <LeirEngine/Components/SpriteRenderer.h>
#include <LeirEngine/Components/Camera.h>
#include <LeirEngine/Components/Light.h>

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

        // Init Vulkan
        Leir::VulkanDeviceConfig config;
        config.appName = "LeirEngine Editor";
        config.windowWidth = GetWidth();
        config.windowHeight = GetHeight();
        m_VulkanDevice = std::make_unique<Leir::VulkanDevice>(GetWindow(), config);

        // Create shader
        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_VulkanDevice.get(),
            shaderDir + "/Basic.vert.spv",
            shaderDir + "/Basic.frag.spv"
        );

        // Create a default white texture
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(
            m_VulkanDevice.get(), 1, 1, whitePixel);

        // Create a test material
        m_Material = std::make_shared<Leir::Material>(m_VulkanDevice.get(), m_Shader);
        m_Material->SetTexture("texSampler", m_WhiteTexture);
        m_Material->RecreatePipeline(m_VulkanDevice->GetRenderPass());

        // Create a test cube mesh
        auto [verts, idxs] = Leir::Primitives::CreateCube();
        m_Mesh = std::make_shared<Leir::Mesh>(m_VulkanDevice.get(), verts, idxs);

        // Create render pipeline
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_VulkanDevice.get());

        // Create a test scene
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

        // Test hierarchy
        Leir::Object3D* child = scene.CreateObject3D("Child");
        child->GetTransform().SetLocalPosition({2.0f, 1.0f, 0.0f});
        child->SetParent(cubeObj);

        // Test 2D sprite overlay — bright cyan quad at center (no texture)
        auto* spriteObj = scene.CreateObject2D("TestSprite");
        spriteObj->GetTransform().SetLocalPosition(
            {GetWidth() * 0.5f, GetHeight() * 0.5f, 0.0f});
        spriteObj->GetTransform().SetLocalScale({200.0f, 200.0f, 1.0f});
        auto& spr = spriteObj->AddComponent<Leir::SpriteRenderer>();
        // No texture — uses internal white fallback, color tint makes it cyan
        spr.SetColor({0.0f, 1.0f, 1.0f, 1.0f});

        // Second sprite with explicit white texture — red tint at top-left
        auto* spriteTex = scene.CreateObject2D("TexSprite");
        spriteTex->GetTransform().SetLocalPosition({100.0f, 100.0f, 0.0f});
        spriteTex->GetTransform().SetLocalScale({100.0f, 100.0f, 1.0f});
        auto& sprTex = spriteTex->AddComponent<Leir::SpriteRenderer>();
        sprTex.SetTexture(m_WhiteTexture.get());
        sprTex.SetColor({1.0f, 0.0f, 0.0f, 1.0f});

        spdlog::info("Scene hierarchy created with Vulkan renderer");
    }

    void OnUpdate(float deltaTime) override
    {
        (void)deltaTime;

        // Rotate the cube
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (scene) {
            auto* cube = scene->FindObjectByName("Cube");
            if (cube) {
                auto& t = cube->GetTransform();
                t.SetLocalRotation(
                    glm::quat(glm::vec3(0.0f, deltaTime * 0.5f, 0.0f)) * t.GetLocalRotation());
            }
        }
    }

    void OnRender() override
    {
        if (m_VulkanDevice && m_VulkanDevice->BeginFrame()) {
            VkCommandBuffer cmd = m_VulkanDevice->GetCurrentCommandBuffer();

            auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
            m_RenderPipeline->Render(cmd, scene);

            // 2D overlay (UI / sprites rendered on top)
            m_VulkanDevice->BeginOverlay();
            m_RenderPipeline->RenderOverlay(cmd, scene);

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
};

int main()
{
    spdlog::set_level(spdlog::level::trace);

    EditorApp app;
    app.Run();

    return 0;
}
