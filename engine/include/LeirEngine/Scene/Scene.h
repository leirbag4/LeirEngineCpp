#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Types.h"
#include "LeirEngine/Scene/ISceneStorage.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/HierarchyTree.h"
#include "LeirEngine/ECS/TransformSystem.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Leir {

class CoreObject;
class Object3D;
class Object2D;

// Scene is the ECS-backed implementation of ISceneStorage (Etapa A, fuses what
// ECSScene proved): every CreateObject3D/2D creates an ECS entity (family tag +
// LocalTransform + tree node) and backs the object's Transform (facade over the
// ECS), so components live as HybridComponents and the hierarchy is the ECS
// tree. OnUpdate drives the hybrid component lifecycle (Component::Tick) and the
// TransformSystem. Render/picking consume the ISceneStorage caches.
class LEIR_API Scene : public ISceneStorage {
public:
    Scene(const std::string& name = "Untitled Scene");
    ~Scene() override;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    // Object creation / destruction (ISceneStorage).
    Object3D* CreateObject3D(const std::string& name = "Object3D") override;
    Object2D* CreateObject2D(const std::string& name = "Object2D") override;
    void DestroyObject(CoreObject* object) override;

    // Query (ISceneStorage).
    CoreObject* FindObjectByUUID(uint64_t uuid) const override;
    CoreObject* FindObjectByName(const std::string& name) const override;
    const std::vector<std::unique_ptr<CoreObject>>& GetObjects() const override { return m_Objects; }

    // Move an object to a given index in m_Objects (the authoritative scene
    // order: render + the hierarchy's root/sibling DFS). Used by the hierarchy
    // drag&drop to reorder roots. Clamps; no-op if obj isn't in the scene.
    void MoveObject(CoreObject* object, size_t index) override;

    // Data-oriented query caches (ISceneStorage) — see ISceneStorage docs.
    const std::vector<CoreObject*>& GetRenderables() override;
    const std::vector<CoreObject*>& GetCameras() override;
    const std::vector<CoreObject*>& GetLights() override;
    void MarkCachesDirty() override { m_CachesDirty = true; }

    // ECS access (Etapa A: the single source of truth).
    ECS::World& GetWorld() { return m_World; }
    ECS::HierarchyTree& GetTree() { return m_Tree; }
    ECS::TransformSystem& GetTransforms() { return m_Transforms; }
    ECS::Entity EntityOf(const CoreObject* object) const;

    // Lifecycle
    void OnUpdate(float deltaTime);
    void OnRender();

private:
    ECS::Entity CreateEntity(CoreObject* object, bool is3D);
    void RebuildCaches();

    std::string m_Name;
    std::vector<std::unique_ptr<CoreObject>> m_Objects;
    std::unordered_map<uint64_t, CoreObject*> m_ObjectIndex;

    ECS::World m_World;
    ECS::HierarchyTree m_Tree;
    ECS::TransformSystem m_Transforms;
    std::unordered_map<const CoreObject*, ECS::Entity> m_EntityOf;

    // Query caches (rebuilt lazily via RebuildCaches when m_CachesDirty).
    bool m_CachesDirty = true;
    std::vector<CoreObject*> m_Renderables;
    std::vector<CoreObject*> m_Cameras;
    std::vector<CoreObject*> m_Lights;
};

} // namespace Leir
