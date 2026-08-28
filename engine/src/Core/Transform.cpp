#include "LeirEngine/Core/Transform.h"

#include <algorithm>
#include <cmath>

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

// --- Local space setters ---

void Transform::SetLocalPosition(const Vector3& position)
{
    m_LocalPosition = position;
    MarkDirty();
}

void Transform::SetLocalRotation(const Quaternion& rotation)
{
    m_LocalRotation = rotation;
    MarkDirty();
}

void Transform::SetLocalScale(const Vector3& scale)
{
    m_LocalScale = scale;
    MarkDirty();
}

// --- World space getters (computed lazily) ---

Vector3 Transform::GetWorldPosition() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldPosition;
}

Quaternion Transform::GetWorldRotation() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldRotation;
}

Vector3 Transform::GetWorldScale() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldScale;
}

// --- World space setters ---

void Transform::SetWorldPosition(const Vector3& position)
{
    if (!m_Parent) {
        SetLocalPosition(position);
        return;
    }
    Matrix4x4 parentWorld = m_Parent->GetLocalToWorldMatrix();
    Matrix4x4 parentInverse = parentWorld.Inverse();
    Vector3 localPos = parentInverse.MultiplyPoint3x4(position);
    SetLocalPosition(localPos);
}

void Transform::SetWorldRotation(const Quaternion& rotation)
{
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
    if (!m_Parent) {
        SetLocalScale(scale);
        return;
    }
    Vector3 parentScale = m_Parent->GetWorldScale();
    SetLocalScale(Vector3(scale.x / parentScale.x,
                          scale.y / parentScale.y,
                          scale.z / parentScale.z));
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
            std::sqrt(m_WorldMatrix(0,0) * m_WorldMatrix(0,0) +
                      m_WorldMatrix(1,0) * m_WorldMatrix(1,0) +
                      m_WorldMatrix(2,0) * m_WorldMatrix(2,0)),
            std::sqrt(m_WorldMatrix(0,1) * m_WorldMatrix(0,1) +
                      m_WorldMatrix(1,1) * m_WorldMatrix(1,1) +
                      m_WorldMatrix(2,1) * m_WorldMatrix(2,1)),
            std::sqrt(m_WorldMatrix(0,2) * m_WorldMatrix(0,2) +
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
