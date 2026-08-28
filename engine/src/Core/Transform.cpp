#include "LeirEngine/Core/Transform.h"

#include "LeirEngine/Math/Mathf.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/HierarchyTree.h"
#include "LeirEngine/ECS/TransformSystem.h"
#include <algorithm>

namespace Leir {

Transform::Transform()
{
    MarkDirty();
}

Transform::~Transform()
{
    if (m_Parent)
        m_Parent->RemoveChild(this);
    for (auto child : m_Children)
        child->m_Parent = nullptr;
}

void Transform::SetEcsBacked(ECS::World* world, ECS::TransformSystem* transforms,
                             ECS::HierarchyTree* tree, ECS::Entity entity)
{
    m_Ecs.world = world;
    m_Ecs.transforms = transforms;
    m_Ecs.tree = tree;
    m_Ecs.entity = entity;
    // Mirror the current locals into the ECS so the entity starts consistent.
    SyncEcsLocal();
}

void Transform::SyncEcsLocal()
{
    if (!m_Ecs.world)
        return;
    m_Ecs.transforms->SetLocal(m_Ecs.entity,
        ECS::LocalTransform{m_LocalPosition, m_LocalRotation, m_LocalScale});
}

void Transform::SyncFromEcsLocal()
{
    if (!m_Ecs.world)
        return;
    auto* lt = m_Ecs.transforms->GetLocal(m_Ecs.entity);
    if (!lt)
        return;
    m_LocalPosition = lt->position;
    m_LocalRotation = lt->rotation;
    m_LocalScale = lt->scale;
}

// --- Local space setters ---

void Transform::SetLocalPosition(const Vector3& position)
{
    m_LocalPosition = position;
    MarkDirty();
    SyncEcsLocal();
}

void Transform::SetLocalRotation(const Quaternion& rotation)
{
    m_LocalRotation = rotation;
    MarkDirty();
    SyncEcsLocal();
}

void Transform::SetLocalScale(const Vector3& scale)
{
    m_LocalScale = scale;
    MarkDirty();
    SyncEcsLocal();
}

// --- World space getters (computed lazily; from the ECS when backed) ---

Vector3 Transform::GetWorldPosition() const
{
    if (m_Ecs.world) {
        auto* wt = m_Ecs.transforms->GetWorld(m_Ecs.entity);
        return wt ? wt->worldPosition : m_WorldPosition;
    }
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldPosition;
}

Quaternion Transform::GetWorldRotation() const
{
    if (m_Ecs.world) {
        auto* wt = m_Ecs.transforms->GetWorld(m_Ecs.entity);
        return wt ? wt->worldRotation : m_WorldRotation;
    }
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldRotation;
}

Vector3 Transform::GetWorldScale() const
{
    if (m_Ecs.world) {
        auto* wt = m_Ecs.transforms->GetWorld(m_Ecs.entity);
        return wt ? wt->worldScale : m_WorldScale;
    }
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldScale;
}

// --- World space setters ---

void Transform::SetWorldPosition(const Vector3& position)
{
    if (m_Ecs.world) {
        m_Ecs.transforms->SetWorldPosition(m_Ecs.entity, position);
        SyncFromEcsLocal();
        return;
    }
    if (!m_Parent) {
        SetLocalPosition(position);
        return;
    }
    Matrix4x4 parentWorld = m_Parent->GetLocalToWorldMatrix();
    Matrix4x4 parentInverse = parentWorld.Inverse();
    // A singular parent matrix (e.g. a zero-scaled axis) yields a non-finite
    // inverse via glm::inverse; the world is degenerate there and any local
    // value is arbitrary. Keep the current local position rather than poisoning
    // the chain with NaN.
    if (!parentInverse.IsFinite())
        return;
    Vector3 localPos = parentInverse.MultiplyPoint3x4(position);
    SetLocalPosition(localPos);
}

void Transform::SetWorldRotation(const Quaternion& rotation)
{
    if (m_Ecs.world) {
        m_Ecs.transforms->SetWorldRotation(m_Ecs.entity, rotation);
        SyncFromEcsLocal();
        return;
    }
    if (!m_Parent) {
        SetLocalRotation(rotation);
        return;
    }
    Quaternion parentRot = m_Parent->GetWorldRotation();
    Quaternion localRot = parentRot.Inverse() * rotation;
    SetLocalRotation(localRot);
}

void Transform::SetWorldScale(const Vector3& scale)
{
    if (m_Ecs.world) {
        m_Ecs.transforms->SetWorldScale(m_Ecs.entity, scale);
        SyncFromEcsLocal();
        return;
    }
    if (!m_Parent) {
        SetLocalScale(scale);
        return;
    }
    // World-scale preserve: the child's world LOSSY scale (column lengths) along
    // axis i is localScale[i] * |(parent world rotation-scale · child LOCAL
    // rotation) column i|. Dividing by those column lengths -- instead of a naive
    // parent-lossy division -- factors the parent's ROTATION, so nesting into a
    // rotated + non-uniformly-scaled parent keeps the world LOSSY scale exactly
    // (Unity's SetParent(worldPositionStays) divides by the parent's lossyScale
    // and deforms the child in that case). SetWorldRotation already ran in
    // SetParent, so m_LocalRotation compensates the parent rotation.
    //
    // NOTE: what is preserved is POSITION, ROTATION and the world LOSSY scale.
    // A rotated + non-uniformly-scaled parent can still leave SHEAR in the
    // child's world matrix (unit-length but non-perpendicular columns): the
    // local model is TRS (pos/rot/scale), which cannot express shear, so it
    // leaks into the world. This is an inherent TRS limitation shared with Unity
    // (which additionally squashes the scale); it is not an approximation of the
    // divisor.
    Matrix4x4 parentWorld = m_Parent->GetLocalToWorldMatrix();
    Matrix4x4 localRot = Matrix4x4::TRS(Vector3(0.0f, 0.0f, 0.0f), m_LocalRotation, Vector3(1.0f, 1.0f, 1.0f));
    Matrix4x4 combined = parentWorld * localRot;
    Vector3 colLen(
        Mathf::Sqrt(combined(0,0) * combined(0,0) +
                  combined(1,0) * combined(1,0) +
                  combined(2,0) * combined(2,0)),
        Mathf::Sqrt(combined(0,1) * combined(0,1) +
                  combined(1,1) * combined(1,1) +
                  combined(2,1) * combined(2,1)),
        Mathf::Sqrt(combined(0,2) * combined(0,2) +
                  combined(1,2) * combined(1,2) +
                  combined(2,2) * combined(2,2))
    );
    // Epsilon guard: if a parent axis is scaled to exactly 0 and the child's
    // local rotation aligns with it, the combined column collapses (colLen = 0)
    // and the captured world scale is also 0 there -> 0/0 = NaN would poison the
    // whole transform chain. Clamp the divisor to 1.0 so the result stays 0 (the
    // world is degenerate in that axis anyway). Below the threshold only matters
    // for sub-1e-8 parent scales (invisible); the division is exact above it.
    constexpr float kEps = 1e-8f;
    SetLocalScale(Vector3(scale.x / (colLen.x > kEps ? colLen.x : 1.0f),
                          scale.y / (colLen.y > kEps ? colLen.y : 1.0f),
                          scale.z / (colLen.z > kEps ? colLen.z : 1.0f)));
}

// --- Convenience ---

void Transform::Translate(const Vector3& delta)
{
    SetLocalPosition(m_LocalPosition + delta);
}

void Transform::Rotate(float angle, const Vector3& axis)
{
    SetLocalRotation(m_LocalRotation * Quaternion::AngleAxis(angle, axis));
}

void Transform::Scale(const Vector3& factor)
{
    SetLocalScale(Vector3(
        m_LocalScale.x * factor.x,
        m_LocalScale.y * factor.y,
        m_LocalScale.z * factor.z
    ));
}

Vector3 Transform::GetForward() const
{
    return GetWorldRotation() * Vector3::Forward();
}

Vector3 Transform::GetRight() const
{
    return GetWorldRotation() * Vector3::Right();
}

Vector3 Transform::GetUp() const
{
    return GetWorldRotation() * Vector3::Up();
}

// --- Matrices ---

Matrix4x4 Transform::GetLocalMatrix() const
{
    return Matrix4x4::TRS(m_LocalPosition, m_LocalRotation, m_LocalScale);
}

Matrix4x4 Transform::GetLocalToWorldMatrix() const
{
    if (m_Ecs.world) {
        auto* wt = m_Ecs.transforms->GetWorld(m_Ecs.entity);
        return wt ? wt->worldMatrix : m_WorldMatrix;
    }
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldMatrix;
}

Matrix4x4 Transform::GetWorldToLocalMatrix() const
{
    return GetLocalToWorldMatrix().Inverse();
}

// --- Parent / Child ---

void Transform::SetParent(Transform* parent, bool worldPositionStays)
{
    if (m_Ecs.world) {
        // ECS-backed: the hierarchy lives in the ECS tree; delegate to the
        // transform system (exact lossy-preserve / worldPositionStays) and pull
        // the resulting local back into the members.
        ECS::Entity parentEntity = parent ? parent->GetEcsEntity() : ECS::kNullEntity;
        m_Ecs.transforms->SetParent(m_Ecs.entity, parentEntity, worldPositionStays);
        SyncFromEcsLocal();
        return;
    }
    if (m_Parent == parent)
        return;

    Vector3 worldPos, worldScale;
    Quaternion worldRot;

    if (worldPositionStays) {
        worldPos = GetWorldPosition();
        worldRot = GetWorldRotation();
        worldScale = GetWorldScale();
    }

    if (m_Parent)
        m_Parent->RemoveChild(this);

    m_Parent = parent;

    if (m_Parent)
        m_Parent->AddChild(this);

    if (worldPositionStays) {
        SetWorldPosition(worldPos);
        SetWorldRotation(worldRot);
        SetWorldScale(worldScale);
    }

    MarkDirty();
}

Transform* Transform::GetChild(size_t index) const
{
    if (index < m_Children.size())
        return m_Children[index];
    return nullptr;
}

void Transform::AddChild(Transform* child)
{
    if (!child || child == this)
        return;
    if (std::find(m_Children.begin(), m_Children.end(), child) == m_Children.end())
        m_Children.push_back(child);
}

void Transform::RemoveChild(Transform* child)
{
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end())
        m_Children.erase(it);
}

// --- Internal ---

void Transform::MarkDirty()
{
    m_Dirty = true;
    for (auto child : m_Children)
        child->MarkDirty();
}

void Transform::UpdateWorldMatrix() const
{
    if (!m_Dirty) return;

    if (!m_Parent) {
        m_WorldMatrix = GetLocalMatrix();
        m_WorldPosition = m_LocalPosition;
        m_WorldRotation = m_LocalRotation;
        m_WorldScale = m_LocalScale;
    } else {
        m_WorldMatrix = m_Parent->GetLocalToWorldMatrix() * GetLocalMatrix();

        m_WorldRotation = m_Parent->GetWorldRotation() * m_LocalRotation;
        m_WorldPosition = m_Parent->GetLocalToWorldMatrix().MultiplyPoint3x4(m_LocalPosition);
        // LOSSY world scale (Unity's Transform.lossyScale): the lengths of the
        // rotation-scale part's columns. Unlike a naive component-wise product of
        // parent*local scales, this factors the parent's ROTATION, so a nested
        // non-uniform scale under a rotated parent is preserved correctly (used by
        // SetWorldScale / SetParent(worldPositionStays) for exact round-trips).
        m_WorldScale = Vector3(
            Mathf::Sqrt(m_WorldMatrix(0,0) * m_WorldMatrix(0,0) +
                      m_WorldMatrix(1,0) * m_WorldMatrix(1,0) +
                      m_WorldMatrix(2,0) * m_WorldMatrix(2,0)),
            Mathf::Sqrt(m_WorldMatrix(0,1) * m_WorldMatrix(0,1) +
                      m_WorldMatrix(1,1) * m_WorldMatrix(1,1) +
                      m_WorldMatrix(2,1) * m_WorldMatrix(2,1)),
            Mathf::Sqrt(m_WorldMatrix(0,2) * m_WorldMatrix(0,2) +
                      m_WorldMatrix(1,2) * m_WorldMatrix(1,2) +
                      m_WorldMatrix(2,2) * m_WorldMatrix(2,2))
        );
    }

    m_Dirty = false;
}

void Transform::UpdateWorldFromLocal() const
{
    UpdateWorldMatrix();
}

} // namespace Leir
