#include <LeirEngine/Core/CoreApplication.h>
#include <LeirEngine/Core/CoreObject.h>
#include <LeirEngine/Core/Settings.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Scene/ISceneStorage.h>
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
#include <LeirEngine/ECS/World.h>
#include <LeirEngine/ECS/HierarchyTree.h>
#include <LeirEngine/ECS/TransformSystem.h>

#include "LeirEngine/Core/Log.h"

#include <memory>
#include <vector>
#include <cmath>
#include <string>

// Etapa A proof (TODO_HYBRID_ECS.md §7): CoreObjects whose TRANSFORM is
// ECS-backed (facade over LocalTransform/WorldTransform), whose COMPONENTS are
// HybridComponents in the ECS, and whose HIERARCHY is the ECS tree. No OOP
// transform storage is used for rendering: the RenderPipeline reads the ECS
// worlds through the facade. Single source of truth = the ECS.
struct BackedScene : public Leir::ISceneStorage {
    Leir::ECS::World world;
    Leir::ECS::HierarchyTree tree;
    Leir::ECS::TransformSystem transforms{&world, &tree};
    std::vector<std::unique_ptr<Leir::CoreObject>> objects;
    std::vector<Leir::CoreObject*> renderables;
    std::vector<Leir::CoreObject*> cameras;
    std::vector<Leir::CoreObject*> lights;

    Leir::Object3D* CreateObject3D(const std::string& name) override
    {
        auto obj = std::make_unique<Leir::Object3D>(name);
        Leir::Object3D* ptr = obj.get();
        Leir::ECS::Entity e = world.Create();
        tree.EnsureIndex(e.index);
        transforms.SetLocal(e, {}); // LocalTransform + marks dirty
        ptr->GetTransform().SetEcsBacked(&world, &transforms, &tree, e);
        objects.push_back(std::move(obj));
        return ptr;
    }
    Leir::Object2D* CreateObject2D(const std::string&) override { return nullptr; }
    void DestroyObject(Leir::CoreObject*) override {}
    void MoveObject(Leir::CoreObject*, size_t) override {}
    Leir::CoreObject* FindObjectByUUID(uint64_t) const override { return nullptr; }
    Leir::CoreObject* FindObjectByName(const std::string&) const override { return nullptr; }
    const std::vector<std::unique_ptr<Leir::CoreObject>>& GetObjects() const override { return objects; }
    void MarkCachesDirty() override {}
    const std::vector<Leir::CoreObject*>& GetRenderables() override { Rebuild(); return renderables; }
    const std::vector<Leir::CoreObject*>& GetCameras() override { Rebuild(); return cameras; }
    const std::vector<Leir::CoreObject*>& GetLights() override { Rebuild(); return lights; }
    void OnUpdate() { transforms.Update(); }

private:
    void Rebuild()
    {
        renderables.clear();
        cameras.clear();
        lights.clear();
        for (auto& o : objects) {
            if (o->GetComponent<Leir::MeshRenderer>())
                renderables.push_back(o.get());
            if (o->GetComponent<Leir::Camera>())
                cameras.push_back(o.get());
            if (o->GetComponent<Leir::Light>())
                lights.push_back(o.get());
        }
    }
};

class ECSBackedDemo : public Leir::CoreApplication {
public:
    ECSBackedDemo()
        : CoreApplication("LeirEngine ECS-Backed Demo", 1280, 720)
    {
    }

    ~ECSBackedDemo()
    {
        if (m_Backend)
            m_Backend->WaitIdle();
    }

protected:
    void OnInit() override
    {
        Leir::XConsole::Println("ECS-Backed Demo initializing (Etapa A proof — CoreObject over ECS)");

        m_Backend.reset(Leir::RHI::BackendFactory::Create(
            Leir::LeirSettings::Get().graphics.backend,
            GetWindow(), GetWidth(), GetHeight(), false, "LeirEngine ECS-Backed Demo"));
        if (!m_Backend) {
            Leir::XConsole::PrintError("Failed to create render backend");
            return;
        }

        std::string shaderDir = LEIR_SHADER_DIR;
        m_Shader = std::make_shared<Leir::Shader>(
            m_Backend.get(),
            shaderDir + "/Basic.vert" + m_Backend->GetShaderFileExtension(),
            shaderDir + "/Basic.frag" + m_Backend->GetShaderFileExtension());
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        m_WhiteTexture = std::make_shared<Leir::Texture2D>(m_Backend.get(), 1, 1, whitePixel);

        m_BlueMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_BlueMat->SetTexture("texSampler", m_WhiteTexture);
        m_BlueMat->SetColor({0.25f, 0.55f, 0.95f, 1.0f});
        m_BlueMat->RecreatePipeline(m_Backend->GetRenderPass());
        m_RedMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_RedMat->SetTexture("texSampler", m_WhiteTexture);
        m_RedMat->SetColor({0.85f, 0.3f, 0.25f, 1.0f});
        m_RedMat->RecreatePipeline(m_Backend->GetRenderPass());
        m_GreenMat = std::make_shared<Leir::Material>(m_Backend.get(), m_Shader);
        m_GreenMat->SetTexture("texSampler", m_WhiteTexture);
        m_GreenMat->SetColor({0.6f, 0.8f, 0.3f, 1.0f});
        m_GreenMat->RecreatePipeline(m_Backend->GetRenderPass());

        auto [boxVerts, boxIdxs] = Leir::Primitives::CreateCube();
        m_BoxMesh = std::make_shared<Leir::Mesh>(m_Backend.get(), boxVerts, boxIdxs);
        m_RenderPipeline = std::make_unique<Leir::RenderPipeline>(m_Backend.get());

        // Camera (backed object).
        m_CameraObj = m_Scene.CreateObject3D("Camera");
        m_CameraObj->GetTransform().SetLocalPosition(Leir::Vector3(0.0f, 4.0f, 12.0f));
        auto& camera = m_CameraObj->AddComponent<Leir::Camera>();
        camera.SetPerspective(60.0f, (float)GetWidth() / (float)GetHeight(), 0.1f, 100.0f);
        camera.SetPrimary(true);

        // Directional light.
        auto* lightObj = m_Scene.CreateObject3D("Light");
        lightObj->GetTransform().SetLocalPosition(Leir::Vector3(5.0f, 10.0f, -5.0f));
        auto& light = lightObj->AddComponent<Leir::Light>();
        light.SetType(Leir::LightType::Directional);
        light.SetColor({1.0f, 0.95f, 0.9f});
        light.SetIntensity(1.5f);

        // Parent + child (hierarchy through the ECS tree; AddChild preserves world).
        m_Parent = m_Scene.CreateObject3D("Parent");
        m_Parent->GetTransform().SetLocalPosition(Leir::Vector3(-1.5f, 1.0f, 0.0f));
        auto& pr = m_Parent->AddComponent<Leir::MeshRenderer>();
        pr.SetMesh(m_BoxMesh);
        pr.SetMaterial(m_BlueMat);

        auto* child = m_Scene.CreateObject3D("Child");
        child->GetTransform().SetLocalPosition(Leir::Vector3(0.5f, 1.0f, 0.0f));
        auto& cr = child->AddComponent<Leir::MeshRenderer>();
        cr.SetMesh(m_BoxMesh);
        cr.SetMaterial(m_RedMat);
        m_Parent->AddChild(child);

        // Rotated + uniform-scaled parent with a kid reparented worldPositionStays
        // (lossy-preserve exact through the ECS), + a stretched cube prop.
        auto* rotated = m_Scene.CreateObject3D("Rotated");
        rotated->GetTransform().SetLocalPosition(Leir::Vector3(2.5f, 1.0f, 0.0f));
        rotated->GetTransform().SetLocalRotation(Leir::Quaternion::AngleAxis(45.0f, Leir::Vector3::Forward()));
        rotated->GetTransform().SetLocalScale(Leir::Vector3(2.0f, 2.0f, 2.0f));
        auto& rr = rotated->AddComponent<Leir::MeshRenderer>();
        rr.SetMesh(m_BoxMesh);
        rr.SetMaterial(m_BlueMat);

        m_Kid = m_Scene.CreateObject3D("Kid");
        m_Kid->GetTransform().SetLocalPosition(Leir::Vector3(0.0f, 0.0f, 0.0f));
        auto& kr = m_Kid->AddComponent<Leir::MeshRenderer>();
        kr.SetMesh(m_BoxMesh);
        kr.SetMaterial(m_GreenMat);
        m_Kid->SetParent(rotated, true);

        auto* stretched = m_Scene.CreateObject3D("Stretched");
        stretched->GetTransform().SetLocalPosition(Leir::Vector3(1.2f, 0.5f, -1.8f));
        stretched->GetTransform().SetLocalRotation(Leir::Quaternion::AngleAxis(30.0f, Leir::Vector3::Up()));
        stretched->GetTransform().SetLocalScale(Leir::Vector3(1.5f, 0.75f, 0.75f));
        auto& sr = stretched->AddComponent<Leir::MeshRenderer>();
        sr.SetMesh(m_BoxMesh);
        sr.SetMaterial(m_BlueMat);

        m_Scene.OnUpdate();

        Leir::XConsole::Println("ECS-Backed Demo initialized — CoreObject over pure ECS, rendered by the real RenderPipeline");
        Leir::XConsole::Println("Controls: Left-drag = orbit, wheel = zoom");
    }

    void OnUpdate(float deltaTime) override
    {
        // Orbit camera (backed object: writing its local goes straight to the ECS).
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
        Leir::Vector3 f = (Leir::Vector3::Zero() - camPos).Normalized();
        Leir::Vector3 right = Leir::Vector3::Cross(Leir::Vector3::Up(), f).Normalized();
        Leir::Vector3 u = Leir::Vector3::Cross(f, right);
        m_CameraObj->GetTransform().SetLocalPosition(camPos);
        m_CameraObj->GetTransform().SetLocalRotation(Leir::Quaternion::LookRotation(f, u));

        m_Scene.OnUpdate(); // ECS TransformSystem computes the worlds

        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            Leir::XConsole::Println("Frame {}: renderables={} kidWorldScale=({:.2f},{:.2f},{:.2f})",
                frameCount, (int)m_Scene.GetRenderables().size(),
                m_Kid->GetTransform().GetWorldScale().x,
                m_Kid->GetTransform().GetWorldScale().y,
                m_Kid->GetTransform().GetWorldScale().z);
        }
    }

    void OnRender() override
    {
        if (m_Backend && m_Backend->BeginFrame(false)) {
            auto cmd = m_Backend->GetCurrentCommandBuffer();
            m_SceneGraph.Clear();
            m_RenderPipeline->Render(m_SceneGraph, &m_Scene); // ISceneStorage* -> BackedScene
            m_Backend->CmdExecuteGraph(cmd, m_SceneGraph);
            m_Backend->EndFrame();
        }
    }

    void OnShutdown() override
    {
        Leir::XConsole::Println("Shutting down ECS-Backed Demo");
    }

private:
    std::unique_ptr<Leir::RHI::RenderBackend> m_Backend;
    std::unique_ptr<Leir::RenderPipeline> m_RenderPipeline;
    Leir::RHI::GCommandGraph m_SceneGraph;
    std::shared_ptr<Leir::Shader> m_Shader;
    std::shared_ptr<Leir::Mesh> m_BoxMesh;
    std::shared_ptr<Leir::Material> m_BlueMat;
    std::shared_ptr<Leir::Material> m_RedMat;
    std::shared_ptr<Leir::Material> m_GreenMat;
    std::shared_ptr<Leir::Texture2D> m_WhiteTexture;

    BackedScene m_Scene;
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

    ECSBackedDemo app;
    app.Run();

    return 0;
}