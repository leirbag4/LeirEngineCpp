#pragma once

#include "LeirEngine/Core/Export.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace Leir {

class CoreObject;

class LEIR_API Transform {
public:
    Transform();
    ~Transform();

    // --- Local space ---
    void SetLocalPosition(const glm::vec3& position);
    void SetLocalRotation(const glm::quat& rotation);
    void SetLocalScale(const glm::vec3& scale);

    const glm::vec3& GetLocalPosition() const { return m_LocalPosition; }
    const glm::quat& GetLocalRotation() const { return m_LocalRotation; }
    const glm::vec3& GetLocalScale() const { return m_LocalScale; }

    // --- World space ---
    glm::vec3 GetWorldPosition() const;
    glm::quat GetWorldRotation() const;
    glm::vec3 GetWorldScale() const;

    void SetWorldPosition(const glm::vec3& position);
    void SetWorldRotation(const glm::quat& rotation);
    void SetWorldScale(const glm::vec3& scale);

    // --- Convenience ---
    void Translate(const glm::vec3& delta);
    void Rotate(float angle, const glm::vec3& axis);
    void Scale(const glm::vec3& factor);

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

    // --- Matrices ---
    glm::mat4 GetLocalToWorldMatrix() const;
    glm::mat4 GetWorldToLocalMatrix() const;
    glm::mat4 GetLocalMatrix() const;

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

private:
    void MarkDirty();
    void UpdateWorldMatrix() const;
    void UpdateWorldFromLocal() const;

    glm::vec3 m_LocalPosition{0.0f};
    glm::quat m_LocalRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_LocalScale{1.0f};

    mutable glm::vec3 m_WorldPosition{0.0f};
    mutable glm::quat m_WorldRotation{1.0f, 0.0f, 0.0f, 0.0f};
    mutable glm::vec3 m_WorldScale{1.0f};
    mutable glm::mat4 m_WorldMatrix{1.0f};
    mutable bool m_Dirty = true;

    Transform* m_Parent = nullptr;
    std::vector<Transform*> m_Children;
    CoreObject* m_Owner = nullptr;
};

} // namespace Leir
