#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Quaternion.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include "LeirEngine/ECS/Entity.h"
#include <vector>

namespace Leir {

class CoreObject;
namespace ECS { class World; class HierarchyTree; class TransformSystem; }

class LEIR_API Transform {
public:
    Transform();
    ~Transform();

    // --- Local space ---
    void SetLocalPosition(const Vector3& position);
    void SetLocalRotation(const Quaternion& rotation);
    void SetLocalScale(const Vector3& scale);

    const Vector3& GetLocalPosition() const { return m_LocalPosition; }
    const Quaternion& GetLocalRotation() const { return m_LocalRotation; }
    const Vector3& GetLocalScale() const { return m_LocalScale; }

    // --- World space ---
    Vector3 GetWorldPosition() const;
    Quaternion GetWorldRotation() const;
    Vector3 GetWorldScale() const;

    void SetWorldPosition(const Vector3& position);
    void SetWorldRotation(const Quaternion& rotation);
    void SetWorldScale(const Vector3& scale);

    // --- Convenience ---
    void Translate(const Vector3& delta);
    void Rotate(float angle, const Vector3& axis);
    void Scale(const Vector3& factor);

    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;

    // --- Matrices ---
    Matrix4x4 GetLocalToWorldMatrix() const;
    Matrix4x4 GetWorldToLocalMatrix() const;
    Matrix4x4 GetLocalMatrix() const;

    // --- Parent / Child ---
    void SetParent(Transform* parent, bool worldPositionStays = true);
    Transform* GetParent() const { return m_Parent; }
    size_t GetChildCount() const { return m_Children.size(); }
    Transform* GetChild(size_t index) const;
    void AddChild(Transform* child);
    void RemoveChild(Transform* child);
    const std::vector<Transform*>& GetChildren() const { return m_Children; }

    // --- Owner ---
    CoreObject* GetOwner() const { return m_Owner; }
    void SetOwner(CoreObject* owner) { m_Owner = owner; }

    // --- ECS backing (Etapa A, TODO_HYBRID_ECS.md §7) ---
    // When backed, the transform mirrors its LOCALS into the entity's ECS
    // LocalTransform (TransformSystem::SetLocal) and reads its WORLD from the
    // entity's ECS WorldTransform (computed by TransformSystem). Reparenting and
    // world setters go through the ECS tree/system with the exact lossy-preserve
    // semantics. Unbacked = classic standalone behavior. Additive: nothing sets
    // backing yet in the engine, so existing code paths are untouched.
    void SetEcsBacked(ECS::World* world, ECS::TransformSystem* transforms,
                      ECS::HierarchyTree* tree, ECS::Entity entity);
    bool IsEcsBacked() const { return m_Ecs.world != nullptr; }
    ECS::Entity GetEcsEntity() const { return m_Ecs.entity; }

private:
    void MarkDirty();
    void UpdateWorldMatrix() const;
    void UpdateWorldFromLocal() const;
    void SyncEcsLocal();
    void SyncFromEcsLocal();

    Vector3 m_LocalPosition{0.0f, 0.0f, 0.0f};
    Quaternion m_LocalRotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 m_LocalScale{1.0f, 1.0f, 1.0f};

    mutable Vector3 m_WorldPosition{0.0f, 0.0f, 0.0f};
    mutable Quaternion m_WorldRotation{0.0f, 0.0f, 0.0f, 1.0f};
    mutable Vector3 m_WorldScale{1.0f, 1.0f, 1.0f};
    mutable Matrix4x4 m_WorldMatrix;
    mutable bool m_Dirty = true;

    Transform* m_Parent = nullptr;
    std::vector<Transform*> m_Children;
    CoreObject* m_Owner = nullptr;

    struct EcsBacking {
        ECS::World* world = nullptr;
        ECS::TransformSystem* transforms = nullptr;
        ECS::HierarchyTree* tree = nullptr;
        ECS::Entity entity = ECS::kNullEntity;
    } m_Ecs;
};

} // namespace Leir
