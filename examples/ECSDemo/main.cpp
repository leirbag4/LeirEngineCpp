#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
#include <LeirEngine/Core/Settings.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Scene/ECSScene.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/Rendering/RenderPipeline.h>
#include <LeirEngine/Rendering/Shader.h>
#include <LeirEngine/Rendering/Mesh.h>
#include <LeirEngine/Rendering/Material.h>
#include <LeirEngine/Rendering/Texture2D.h>
#include <LeirEngine/Components/MeshRenderer.h>
#include <LeirEngine/Components/Camera.h>
#include <LeirEngine/Components/Light.h>
#include <LeirEngine/Input/Mouse.h>

#include "LeirEngine/Core/Log.h"

#include <memory>
#include <vector>
#include <cmath>
#include <string>

// Prueba de B (TODO_HYBRID_ECS.md §7, Etapa B): the scene is authored through an
// ECSScene (ISceneStorage backed by the custom ECS) and rendered with the REAL
// RenderPipeline. The ECS computes the world transforms (TransformSystem) and
// ECSScene::OnUpdate writes them back into the CoreObject handles, so the
// renderer draws exactly what the ECS computed — hierarchy + lossy-preserve
// reparent included.
class ECSDemo : public Leir::CoreApplication {
public:
    ECSDemo()
        : CoreApplication("LeirEngine ECS Demo", 1280, 720)
    {
    }

    ~ECSDemo()
    {
        if (m_Backend)
            m_Backend->WaitIdle();
    }

protected:
    void OnInit() override
    {
        Leir::XConsole::Println("ECS Demo initializing (Etapa B — ECSScene + RenderPipeline)");

        // ---- RHI backend ----
        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            Leir::LeirSettings::Get().graphics.backend,
            GetWindow(), GetWidth(), GetHeight(), false, "LeirEngine ECS Demo"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("Failed to create render backend");
            return;
        }

        // ---- Shaders / assets ----
        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_Backend.get(),
            shaderDir + "/Basic.vert" + m_Backend->GetShaderFileExtension(),
            shaderDir + "/Basic.frag" + m_Backend->GetShaderFileExtension());
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(m_Backend.get(), 1, 1, whitePixel);

        m_ParentMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_ParentMat->SetTexture("texSampler", m_WhiteTexture);
        m_ParentMat->SetColor({0.25f, 0.55f, 0.95f, 1.0f});
        m_ParentMat->RecreatePipeline(m_Backend->GetRenderPass());

        m_ChildMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_ChildMat->SetTexture("texSampler", m_WhiteTexture);
        m_ChildMat->SetColor({0.85f, 0.3f, 0.25f, 1.0f});
        m_ChildMat->RecreatePipeline(m_Backend->GetRenderPass());

        m_KidMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_KidMat->SetTexture("texSampler", m_WhiteTexture);
        m_KidMat->SetColor({0.6f, 0.8f, 0.3f, 1.0f});
        m_KidMat->RecreatePipeline(m_Backend->GetRenderPass());

        auto [boxVerts, boxIdxs] = Leir::Primitives::CreateCube();
        m_BoxMesh = std::make_shared<Leir::Mesh>(m_Backend.get(), boxVerts, boxIdxs);

        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_Backend.get());

        // ---- Scene over the ECS (Etapa B) ----
        m_Scene = std::make_unique<Leir::ECSScene>();

        // Camera.
        m_CameraObj = m_Scene->CreateObject3D("Camera");
        m_CameraObj->GetTransform().SetLocalPosition(Leir::Vector3(0.0f, 4.0f, 12.0f));
        auto& camera = m_CameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)GetWidth() / (float)GetHeight(), 0.1f, 100.0f);
        camera.SetPrimary(true);

        // Directional light.
        auto* lightObj = m_Scene->CreateObject3D("Light");
        lightObj->GetTransform().SetLocalPosition(Leir::Vector3(5.0f, 10.0f, -5.0f));
        auto& light = lightObj->AddComponent<Leir::Light>();
        light.SetType(Leir::LightType::Directional);
        light.SetColor({1.0f, 0.95f, 0.9f});
        light.SetIntensity(1.5f);

        // Parent cube with a child cube (hierarchy through the ECS tree).
        m_Parent = m_Scene->CreateObject3D("Parent");
        m_Parent->GetTransform().SetLocalPosition(Leir::Vector3(-1.5f, 1.0f, 0.0f));
        auto& pr = m_Parent->AddComponent<Leir::MeshRenderer>();
        pr.SetMesh(m_BoxMesh);
        pr.SetMaterial(m_ParentMat);

        auto* child = m_Scene->CreateObject3D("Child");
        child->GetTransform().SetLocalPosition(Leir::Vector3(2.0f, 0.0f, 0.0f));
        auto& cr = child->AddComponent<Leir::MeshRenderer>();
        cr.SetMesh(m_BoxMesh);
        cr.SetMaterial(m_ChildMat);
        m_Parent->AddChild(child); // mirrored to the ECS tree

        // Rotated + non-uniform-scaled parent with a kid reparented with
        // worldPositionStays: the ECS TransformSystem preserves the kid's world
        // exactly (lossy scale 1,1,1 — no deformation).
        auto* rotated = m_Scene->CreateObject3D("Rotated");
        rotated->GetTransform().SetLocalPosition(Leir::Vector3(2.5f, 1.0f, 0.0f));
        rotated->GetTransform().SetLocalRotation(
            Leir::Quaternion::AngleAxis(45.0f, Leir::Vector3::Forward()));
        rotated->GetTransform().SetLocalScale(Leir::Vector3(2.0f, 1.0f, 1.0f));
        auto& rr = rotated->AddComponent<Leir::MeshRenderer>();
        rr.SetMesh(m_BoxMesh);
        rr.SetMaterial(m_ParentMat);

        m_Kid = m_Scene->CreateObject3D("Kid");
        m_Kid->GetTransform().SetLocalPosition(Leir::Vector3(0.0f, 0.0f, 0.0f));
        auto& kr = m_Kid->AddComponent<Leir::MeshRenderer>();
        kr.SetMesh(m_BoxMesh);
        kr.SetMaterial(m_KidMat);
        m_Kid->SetParent(rotated, true); // worldPositionStays via the ECS

        m_Scene->OnUpdate(0.0f); // initial ECS sync so renderables have worlds

        Leir::XConsole::Println("ECS Demo initialized — ECS scene rendered by the real RenderPipeline");
        Leir::XConsole::Println("Controls: Left-drag = orbit, wheel = zoom");
    }

    void OnUpdate(float deltaTime) override
    {
        if (!m_Scene)
            return;

        // Orbit camera (authoring writes the handle's local; ECS computes world).
        if (Leir::Mouse::IsDown(Leir::PointerButton::Left)) {
            Leir::Vector2 d = Leir::Mouse::GetDelta();
            m_OrbitYaw += d.x * 0.005f;
            m_OrbitPitch += d.y * 0.005f;
            m_OrbitPitch = Leir::Mathf::Clamp(m_OrbitPitch, -1.3f, 1.3f);
        }
        m_OrbitDistance = Leir::Mathf::Clamp(
            m_OrbitDistance - Leir::Mouse::GetScrollDelta() * 0.5f, 3.0f, 30.0f);
        Leir::Vector3 camPos(
            m_OrbitDistance * std::cos(m_OrbitPitch) * std::sin(m_OrbitYaw),
            m_OrbitDistance * std::sin(m_OrbitPitch),
            m_OrbitDistance * std::cos(m_OrbitPitch) * std::cos(m_OrbitYaw));
        Leir::Vector3 f = camPos.Normalized();
        Leir::Vector3 right = Leir::Vector3::Cross(Leir::Vector3::Up(), f).Normalized();
        Leir::Vector3 u = Leir::Vector3::Cross(f, right);
        Leir::Quaternion camRot = Leir::Quaternion::LookRotation(f, u);
        m_CameraObj->GetTransform().SetLocalPosition(camPos);
        m_CameraObj->GetTransform().SetLocalRotation(camRot);

        // Etapa B: sync structure + locals into the ECS, run the transform
        // system, and write the ECS worlds back into the handles.
        m_Scene->OnUpdate(deltaTime);

        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            Leir::XConsole::Println("Frame {}: renderables={} kidWorldScale=({:.2f},{:.2f},{:.2f})",
                frameCount, (int)m_Scene->GetRenderables().size(),
                m_Kid->GetTransform().GetWorldScale().x,
                m_Kid->GetTransform().GetWorldScale().y,
                m_Kid->GetTransform().GetWorldScale().z);
        }
    }

    void OnRender() override
    {
        if (m_Backend && m_Scene && m_Backend->BeginFrame(false)) {
            auto cmd = m_Backend->GetCurrentCommandBuffer();
            m_SceneGraph.Clear();
            m_RenderPipeline->Render(m_SceneGraph, m_Scene.get()); // ISceneStorage* -> ECSScene
            m_Backend->CmdExecuteGraph(cmd, m_SceneGraph);
            m_Backend->EndFrame();
        }
    }

    void OnShutdown() override
    {
        Leir::XConsole::Println("Shutting down ECS Demo");
        m_Scene.reset();
    }

private:
    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;
    Leir::RHI::GCommandGraph m_SceneGraph;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_BoxMesh;
    std::shared_ptr<Leir::Material> m_ParentMat;
    std::shared_ptr<Leir::Material> m_ChildMat;
    std::shared_ptr<Leir::Material> m_KidMat;
    std::shared_ptr<Leir::Texture2D> m_WhiteTexture;

    std::unique_ptr<Leir::ECSScene> m_Scene;
    Leir::Object3D* m_CameraObj = nullptr;
    Leir::Object3D* m_Parent = nullptr;
    Leir::Object3D* m_Kid = nullptr;

    float m_OrbitYaw = 0.0f;
    float m_OrbitPitch = 0.3f;
    float m_OrbitDistance = 14.0f;
};

int main()
{
    Leir::XConsole::SetLevel(Leir::LogLevel::Info);
    Leir::LeirSettings::Get().Load();

    ECSDemo app;
    app.Run();

    return 0;
}