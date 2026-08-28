#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
#include <LeirEngine/Core/Settings.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
#include <LeirEngine/Rendering/Shader.h>
#include <LeirEngine/Rendering/Mesh.h>
#include <LeirEngine/Rendering/Material.h>
#include <LeirEngine/Rendering/Texture2D.h>
#include <LeirEngine/Components/MeshRenderer.h>
#include <LeirEngine/Components/Camera.h>
#include <LeirEngine/Components/Light.h>
#include <LeirEngine/Components/AudioListener.h>
#include <LeirEngine/Physics/PhysicsWorld.h>
#include <LeirEngine/Physics/RigidBody.h>
#include <LeirEngine/Physics/Collider.h>
#include <LeirEngine/Input/Mouse.h>
#include <LeirEngine/Input/Keyboard.h>
#include <LeirEngine/Audio/AudioEngine.h>
#include <LeirEngine/Audio/SoundPlayer.h>

#include "LeirEngine/Core/Log.h"

#include <memory>
#include <vector>
#include <cmath>
#include <string>

class PhysicsDemo : public Leir::CoreApplication {
public:
    PhysicsDemo()
        : CoreApplication("LeirEngine Physics Demo", 1280, 720)
    {
    }

    ~PhysicsDemo()
    {
        if (m_Backend)
            m_Backend->WaitIdle();
    }

protected:
    void OnInit() override
    {
        Leir::XConsole::Println("Physics Demo initializing");

        // ---- RHI backend ----
        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            Leir::LeirSettings::Get().graphics.backend,
            GetWindow(), GetWidth(), GetHeight(), false, "LeirEngine Physics Demo"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("Failed to create render backend");
            return;
        }

        // ---- Shaders ----
        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_Backend.get(),
            shaderDir + "/Basic.vert" + m_Backend->GetShaderFileExtension(),
            shaderDir + "/Basic.frag" + m_Backend->GetShaderFileExtension()
        );

        // ---- Default white texture ----
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(
            m_Backend.get(), 1, 1, whitePixel);

        // ---- Materials ----
        m_GroundMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_GroundMat->SetTexture("texSampler", m_WhiteTexture);
        m_GroundMat->SetColor({0.3f, 0.3f, 0.35f, 1.0f});
        m_GroundMat->RecreatePipeline(m_Backend->GetRenderPass());

        m_BoxMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_BoxMat->SetTexture("texSampler", m_WhiteTexture);
        m_BoxMat->SetColor({0.85f, 0.25f, 0.15f, 1.0f});
        m_BoxMat->RecreatePipeline(m_Backend->GetRenderPass());

        // ---- Meshes ----
        auto [boxVerts, boxIdxs] = Leir::Primitives::CreateCube();
        m_BoxMesh = std::make_shared<Leir::Mesh>(m_Backend.get(), boxVerts, boxIdxs);

        // ---- Render Pipeline ----
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_Backend.get());

        // ---- Physics ----
        Leir::PhysicsWorld::GetInstance().Init();

        // ---- Audio (Fase 6 / M4) ----
        // Desktop starts the WASAPI device immediately (WakeUp() is a no-op
        // outside the browser); the WebGPU/Vulkan/D3D12 render backends are
        // orthogonal to this.
        Leir::AudioEngine::GetInstance().Init();

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
        m_CameraObj->AddComponent<Leir::AudioListener>();

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
                if (row == 1 && col == 1)
                    m_PopBox = box;
            }
        }

        // Music loop + 3D "pop" anchored at the center box. Paths resolve to
        // the repo's assets/audio via LEIR_AUDIO_DIR (compile definition).
        Leir::SoundPlayer::PlayMusic(1, AudioPath("music_loop.ogg"));
        Leir::SoundPlayer::SetMusicVolume(0.7f);

        Leir::XConsole::Println("Physics Demo initialized — 9 boxes + ground");
        Leir::XConsole::Println("Controls: Left-drag = orbit, wheel = zoom, "
            "click = beep (2D), Space = pop at center box (3D), music looping");
    }

    void OnUpdate(float deltaTime) override
    {
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return;

        Leir::AudioEngine::GetInstance().Update(deltaTime);

        // ---- SFX triggers ----
        // Click = 2D beep (one-shot). Space = 3D pop at the center box's
        // current world position (spatialized against the camera listener).
        if (Leir::Mouse::WasPressed(Leir::PointerButton::Primary))
            Leir::SoundPlayer::Play(AudioPath("beep.wav"));
        if (Leir::Keyboard::WasPressed(Leir::Key::Space) && m_PopBox) {
            Leir::SoundPlayer::Play(2, false, 0.8f, AudioPath("pop.wav"),
                m_PopBox->GetTransform().GetWorldPosition());
        }

        // ---- Orbit Camera ----
        if (Leir::Mouse::IsDown(Leir::PointerButton::Left)) {
            glm::vec2 mouseDelta = Leir::Mouse::GetDelta();
            m_OrbitYaw += mouseDelta.x * 0.005f;
            m_OrbitPitch = glm::clamp(
                m_OrbitPitch + mouseDelta.y * 0.005f,
                -1.3f, 1.3f
            );
        }

        m_OrbitDistance = glm::clamp(
            m_OrbitDistance - Leir::Mouse::GetScrollDelta() * 0.5f,
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
            for (auto obj : scene->GetRenderables()) {
                if (obj->IsActive() && obj->GetComponent<Leir::MeshRenderer>())
                    ++renderable;
            }
            auto& t = m_CameraObj->GetTransform();
            Leir::XConsole::Println("Frame {}: cam=({:.1f},{:.1f},{:.1f}) objs={}", 
                frameCount, t.GetWorldPosition().x, t.GetWorldPosition().y, t.GetWorldPosition().z, renderable);
        }
    }

    void OnRender() override
    {
        if (m_Backend && m_Backend->BeginFrame(false)) {
            auto cmd = m_Backend->GetCurrentCommandBuffer();
            auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
            // Pass-less graph: records draws into the swapchain 3D render pass
            // started by BeginFrame(false).
            m_SceneGraph.Clear();
            m_RenderPipeline->Render(m_SceneGraph, scene);
            m_Backend->CmdExecuteGraph(cmd, m_SceneGraph);
            m_Backend->EndFrame();
        }
    }

    void OnShutdown() override
    {
        Leir::XConsole::Println("Shutting down Physics Demo");

        auto& sm = Leir::SceneManager::GetInstance();
        sm.DestroyScene("Main Scene");
        sm.SetActiveScene(nullptr);

        Leir::PhysicsWorld::GetInstance().Shutdown();
        Leir::AudioEngine::GetInstance().Shutdown();
    }

private:
    static std::string AudioPath(const char* fileName)
    {
        return std::string(LEIR_AUDIO_DIR) + "/" + fileName;
    }

    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;

    // Per-frame command graph (see GCommandGraph): records draws into the
    // swapchain render pass; executed by the backend in OnRender.
    Leir::RHI::GCommandGraph m_SceneGraph;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_BoxMesh;
    std::shared_ptr<Leir::Material> m_GroundMat;
    std::shared_ptr<Leir::Material> m_BoxMat;
    std::shared_ptr<Leir::Texture2D> m_WhiteTexture;
    Leir::Object3D* m_CameraObj = nullptr;
    Leir::Object3D* m_PopBox = nullptr;

    float m_OrbitYaw = 0.0f;
    float m_OrbitPitch = 0.3f;
    float m_OrbitDistance = 14.0f;
};

int main()
{
    Leir::XConsole::SetLevel(Leir::LogLevel::Info);
    Leir::LeirSettings::Get().Load();

    PhysicsDemo app;
    app.Run();

    return 0;
}
