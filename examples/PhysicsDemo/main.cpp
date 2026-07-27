#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Rendering/VulkanDevice.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
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
#include <LeirEngine/Input/InputManager.h>

#include <spdlog/spdlog.h>

#include <memory>
#include <vector>
#include <cmath>

class PhysicsDemo : public Leir::CoreApplication {
public:
    PhysicsDemo()
        : CoreApplication("LeirEngine Physics Demo", 1280, 720)
    {
    }

    ~PhysicsDemo()
    {
        if (m_VulkanDevice)
            vkDeviceWaitIdle(m_VulkanDevice->GetDevice());
    }

protected:
    void OnInit() override
    {
        spdlog::info("Physics Demo initializing");

        // ---- Vulkan ----
        Leir::VulkanDeviceConfig config;
        config.appName = "LeirEngine Physics Demo";
        config.windowWidth = GetWidth();
        config.windowHeight = GetHeight();
        m_VulkanDevice = std::make_unique<Leir::VulkanDevice>(GetWindow(), config);

        // ---- Shaders ----
        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_VulkanDevice.get(),
            shaderDir + "/Basic.vert.spv",
            shaderDir + "/Basic.frag.spv"
        );

        // ---- Default white texture ----
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(
            m_VulkanDevice.get(), 1, 1, whitePixel);

        // ---- Materials ----
        m_GroundMat = std::make_shared<Leir::Material>(m_VulkanDevice.get(), m_Shader);
        m_GroundMat->SetTexture("texSampler", m_WhiteTexture);
        m_GroundMat->SetColor({0.3f, 0.3f, 0.35f, 1.0f});
        m_GroundMat->RecreatePipeline(m_VulkanDevice->GetRenderPass());

        m_BoxMat = std::make_shared<Leir::Material>(m_VulkanDevice.get(), m_Shader);
        m_BoxMat->SetTexture("texSampler", m_WhiteTexture);
        m_BoxMat->SetColor({0.85f, 0.25f, 0.15f, 1.0f});
        m_BoxMat->RecreatePipeline(m_VulkanDevice->GetRenderPass());

        // ---- Meshes ----
        auto [boxVerts, boxIdxs] = Leir::Primitives::CreateCube();
        m_BoxMesh = std::make_shared<Leir::Mesh>(m_VulkanDevice.get(), boxVerts, boxIdxs);

        // ---- Render Pipeline ----
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_VulkanDevice.get());

        // ---- Physics ----
        Leir::PhysicsWorld::GetInstance().Init();

        // ---- Scene ----
        auto& sceneManager = Leir::SceneManager::GetInstance();
        auto& scene = sceneManager.CreateScene("Main Scene");
        sceneManager.SetActiveScene(&scene);

        // Camera
        m_CameraObj = scene.CreateObject3D("Camera");
        m_CameraObj->GetTransform().SetLocalPosition(glm::vec3(0.0f, 5.0f, 12.0f));
        auto& camera = m_CameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)GetWidth() / (float)GetHeight(), 0.1f, 100.0f);
        camera.SetPrimary(true);

        // Light
        auto* lightObj = scene.CreateObject3D("Light");
        lightObj->GetTransform().SetLocalPosition(glm::vec3(5.0f, 10.0f, -5.0f));
        lightObj->GetTransform().SetLocalRotation(
            glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f)));
        auto& light = lightObj->AddComponent<Leir::Light>();
        light.SetType(Leir::LightType::Directional);
        light.SetColor({1.0f, 0.95f, 0.9f});
        light.SetIntensity(1.5f);

        // Ground
        auto* ground = scene.CreateObject3D("Ground");
        ground->GetTransform().SetLocalPosition(glm::vec3(0.0f, -5.0f, 0.0f));
        ground->GetTransform().SetLocalScale(glm::vec3(10.0f, 0.5f, 10.0f));
        auto& groundRenderer = ground->AddComponent<Leir::MeshRenderer>();
        groundRenderer.SetMesh(m_BoxMesh);
        groundRenderer.SetMaterial(m_GroundMat);
        ground->AddComponent<Leir::Collider>().SetBox(glm::vec3(10.0f, 0.5f, 10.0f));
        ground->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Static);

        // Dynamic boxes in a grid
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                float x = (col - 1) * 1.8f;
                float z = (row - 1) * 1.8f;
                float y = 5.0f + row * 1.2f + col * 1.2f;

                std::string name = "Box" + std::to_string(row * 3 + col);
                auto* box = scene.CreateObject3D(name.c_str());
                box->GetTransform().SetLocalPosition(glm::vec3(x, y, z));
                auto& renderer = box->AddComponent<Leir::MeshRenderer>();
                renderer.SetMesh(m_BoxMesh);
                renderer.SetMaterial(m_BoxMat);
                box->AddComponent<Leir::Collider>().SetBox(glm::vec3(0.5f, 0.5f, 0.5f));
                box->AddComponent<Leir::RigidBody>().SetType(Leir::RigidBodyType::Dynamic);
            }
        }

        spdlog::info("Physics Demo initialized — 9 boxes + ground");
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        // ---- Orbit Camera ----
        auto& input = Leir::InputManager::GetInstance();

        if (input.IsMouseButtonDown(Leir::MouseButton::Left)) {
            glm::vec2 mouseDelta = input.GetMouseDelta();
            m_OrbitYaw += mouseDelta.x * 0.005f;
            m_OrbitPitch = glm::clamp(
                m_OrbitPitch + mouseDelta.y * 0.005f,
                -1.3f, 1.3f
            );
        }

        m_OrbitDistance = glm::clamp(
            m_OrbitDistance - input.GetScrollDelta() * 0.5f,
            3.0f, 30.0f
        );

        // Spherical to cartesian
        glm::vec3 camPos;
        camPos.x = m_OrbitDistance * std::cos(m_OrbitPitch) * std::sin(m_OrbitYaw);
        camPos.y = m_OrbitDistance * std::sin(m_OrbitPitch);
        camPos.z = m_OrbitDistance * std::cos(m_OrbitPitch) * std::cos(m_OrbitYaw);

        // Camera always looks at origin
        glm::vec3 f = glm::normalize(camPos);
        glm::vec3 r = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), f));
        glm::vec3 u = glm::cross(f, r);
        glm::mat3 rotMat(r, u, f);
        glm::quat camRot(rotMat);

        m_CameraObj->GetTransform().SetLocalPosition(camPos);
        m_CameraObj->GetTransform().SetLocalRotation(camRot);

        // Debug: count objects with renderers
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            int renderable = 0;
            for (auto& obj : scene->GetObjects()) {
                if (obj->IsActive() && obj->GetComponent<Leir::MeshRenderer>())
                    ++renderable;
            }
            auto& t = m_CameraObj->GetTransform();
            spdlog::info("Frame {}: cam=({:.1f},{:.1f},{:.1f}) objs={}", 
                frameCount, t.GetWorldPosition().x, t.GetWorldPosition().y, t.GetWorldPosition().z, renderable);
        }
    }

    void OnRender() override
    {
        if (m_VulkanDevice && m_VulkanDevice->BeginFrame()) {
            VkCommandBuffer cmd = m_VulkanDevice->GetCurrentCommandBuffer();
            auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
            m_RenderPipeline->Render(cmd, scene);
            m_VulkanDevice->EndFrame();
        }
    }

    void OnShutdown() override
    {
        spdlog::info("Shutting down Physics Demo");

        auto& sm = Leir::SceneManager::GetInstance();
        sm.DestroyScene("Main Scene");
        sm.SetActiveScene(nullptr);

        Leir::PhysicsWorld::GetInstance().Shutdown();
    }

private:
    std::unique_ptr<Leir::VulkanDevice> m_VulkanDevice;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_BoxMesh;
    std::shared_ptr<Leir::Material> m_GroundMat;
    std::shared_ptr<Leir::Material> m_BoxMat;
    std::shared_ptr<Leir::Texture2D> m_WhiteTexture;
    Leir::Object3D* m_CameraObj = nullptr;

    float m_OrbitYaw = 0.0f;
    float m_OrbitPitch = 0.3f;
    float m_OrbitDistance = 14.0f;
};

int main()
{
    spdlog::set_level(spdlog::level::info);

    PhysicsDemo app;
    app.Run();

    return 0;
}
