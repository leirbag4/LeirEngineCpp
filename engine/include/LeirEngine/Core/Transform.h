#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"
#include "LeirEngine/Math/Quaternion.h"
#include "LeirEngine/Math/Matrix4x4.h"
#include <vector>

namespace Leir {

class CoreObject;

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

private:
    void MarkDirty();
    void UpdateWorldMatrix() const;
    void UpdateWorldFromLocal() const;

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
};

} // namespace Leir
