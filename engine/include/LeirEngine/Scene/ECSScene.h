#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Scene/ISceneStorage.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/HierarchyTree.h"
#include "LeirEngine/ECS/TransformSystem.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leir {

class CoreObject;
class Object3D;
class Object2D;

// Etapa B (TODO_HYBRID_ECS.md §7): an ISceneStorage backed by the custom ECS.
// Each object = an ECS entity (LocalTransform + family tag + tree node) + an
// OOP CoreObject handle (friendly API / component storage). The ECS computes
// the WORLD transforms (TransformSystem); ECSScene::OnUpdate syncs the handles'
// locals into the ECS and writes the ECS worlds back into the handles, so
// GetLocalToWorldMatrix() returns the ECS result. Structure (AddChild/SetParent/
// MoveObject) is mirrored to the ECS tree. Renderables/cameras/lights come from
// the OOP components for now (Etapa A moves them to ECS groups + HybridComponent).
//
// This class is the Etapa-B proof of the seam; it is REPLACED by Scene when
// CoreObject becomes an ECS handle (Etapa A) — see the cleanup list in §7.
class LEIR_API ECSScene : public ISceneStorage {
public:
    ECSScene();
    ~ECSScene() override;
    ECSScene(const ECSScene&) = delete;
    ECSScene& operator=(const ECSScene&) = delete;

    // ISceneStorage structural ops.
    Object3D* CreateObject3D(const std::string& name = "Object3D") override;
    Object2D* CreateObject2D(const std::string& name = "Object2D") override;
    void DestroyObject(CoreObject* object) override;
    void MoveObject(CoreObject* object, size_t index) override;

    // ISceneStorage queries.
    CoreObject* FindObjectByUUID(uint64_t uuid) const override;
    CoreObject* FindObjectByName(const std::string& name) const override;
    const std::vector<std::unique_ptr<CoreObject>>& GetObjects() const override { return m_Objects; }

    const std::vector<CoreObject*>& GetRenderables() override;
    const std::vector<CoreObject*>& GetCameras() override;
    const std::vector<CoreObject*>& GetLights() override;
    void MarkCachesDirty() override { m_CachesDirty = true; }

    // Per-frame: sync structure+locals into the ECS, run the transform system,
    // and write the computed ECS worlds back into the handles.
    void OnUpdate(float dt);

    // ECS access (used by tests now, by Scene in Etapa A).
    ECS::World& GetWorld() { return m_World; }
    ECS::HierarchyTree& GetTree() { return m_Tree; }
    ECS::TransformSystem& GetTransforms() { return m_Transforms; }
    ECS::Entity EntityOf(const CoreObject* object) const;

private:
    ECS::Entity CreateEntity(CoreObject* object, bool is3D);
    void SyncStructure();
    void RebuildCaches();

    ECS::World m_World;
    ECS::HierarchyTree m_Tree;
    ECS::TransformSystem m_Transforms;
    std::vector<std::unique_ptr<CoreObject>> m_Objects;      // handles (all objects, roots + children)
    std::unordered_map<const CoreObject*, ECS::Entity> m_EntityOf;
    std::unordered_map<uint64_t, CoreObject*> m_ObjectIndex; // uuid -> handle
    bool m_CachesDirty = true;
    std::vector<CoreObject*> m_Renderables;
    std::vector<CoreObject*> m_Cameras;
    std::vector<CoreObject*> m_Lights;
};

} // namespace Leir