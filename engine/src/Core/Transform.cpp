#include "LeirEngine/Core/Transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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

// --- Local space setters ---

void Transform::SetLocalPosition(const glm::vec3& position)
{
    m_LocalPosition = position;
    MarkDirty();
}

void Transform::SetLocalRotation(const glm::quat& rotation)
{
    m_LocalRotation = rotation;
    MarkDirty();
}

void Transform::SetLocalScale(const glm::vec3& scale)
{
    m_LocalScale = scale;
    MarkDirty();
}

// --- World space getters (computed lazily) ---

glm::vec3 Transform::GetWorldPosition() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldPosition;
}

glm::quat Transform::GetWorldRotation() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldRotation;
}

glm::vec3 Transform::GetWorldScale() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldScale;
}

// --- World space setters ---

void Transform::SetWorldPosition(const glm::vec3& position)
{
    if (!m_Parent) {
        SetLocalPosition(position);
        return;
    }
    // Convert world position to local
    glm::mat4 parentWorld = m_Parent->GetLocalToWorldMatrix();
    glm::mat4 parentInverse = glm::inverse(parentWorld);
    glm::vec3 localPos = glm::vec3(parentInverse * glm::vec4(position, 1.0f));
    SetLocalPosition(localPos);
}

void Transform::SetWorldRotation(const glm::quat& rotation)
{
    if (!m_Parent) {
        SetLocalRotation(rotation);
        return;
    }
    glm::quat parentRot = m_Parent->GetWorldRotation();
    glm::quat localRot = glm::inverse(parentRot) * rotation;
    SetLocalRotation(localRot);
}

void Transform::SetWorldScale(const glm::vec3& scale)
{
    if (!m_Parent) {
        SetLocalScale(scale);
        return;
    }
    glm::vec3 parentScale = m_Parent->GetWorldScale();
    SetLocalScale(scale / parentScale);
}

// --- Convenience ---

void Transform::Translate(const glm::vec3& delta)
{
    SetLocalPosition(m_LocalPosition + delta);
}

void Transform::Rotate(float angle, const glm::vec3& axis)
{
    SetLocalRotation(glm::rotate(m_LocalRotation, angle, axis));
}

void Transform::Scale(const glm::vec3& factor)
{
    SetLocalScale(m_LocalScale * factor);
}

glm::vec3 Transform::GetForward() const
{
    return GetWorldRotation() * glm::vec3(0.0f, 0.0f, -1.0f);
}

glm::vec3 Transform::GetRight() const
{
    return GetWorldRotation() * glm::vec3(1.0f, 0.0f, 0.0f);
}

glm::vec3 Transform::GetUp() const
{
    return GetWorldRotation() * glm::vec3(0.0f, 1.0f, 0.0f);
}

// --- Matrices ---

glm::mat4 Transform::GetLocalMatrix() const
{
    glm::mat4 mat = glm::translate(glm::mat4(1.0f), m_LocalPosition);
    mat *= glm::mat4_cast(m_LocalRotation);
    mat = glm::scale(mat, m_LocalScale);
    return mat;
}

glm::mat4 Transform::GetLocalToWorldMatrix() const
{
    if (m_Dirty) UpdateWorldMatrix();
    return m_WorldMatrix;
}

glm::mat4 Transform::GetWorldToLocalMatrix() const
{
    return glm::inverse(GetLocalToWorldMatrix());
}

// --- Parent / Child ---

void Transform::SetParent(Transform* parent, bool worldPositionStays)
{
    if (m_Parent == parent)
        return;

    glm::vec3 worldPos, worldScale;
    glm::quat worldRot;

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

        // Chain transforms without decomposing the matrix
        m_WorldRotation = m_Parent->GetWorldRotation() * m_LocalRotation;
        m_WorldPosition = glm::vec3(m_Parent->GetLocalToWorldMatrix() * glm::vec4(m_LocalPosition, 1.0f));
        m_WorldScale = m_Parent->GetWorldScale() * m_LocalScale;
    }

    m_Dirty = false;
}

void Transform::UpdateWorldFromLocal() const
{
    UpdateWorldMatrix();
}

} // namespace Leir
