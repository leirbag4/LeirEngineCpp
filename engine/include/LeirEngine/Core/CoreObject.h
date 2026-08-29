#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Types.h"
#include "LeirEngine/Core/UUID.h"
#include "LeirEngine/Core/Transform.h"
#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/ComponentTraits.h"
#include "LeirEngine/ECS/World.h"

#include <cassert>
#include <string>
#include <vector>
#include <memory>

namespace Leir {

class Scene;

class LEIR_API CoreObject {
public:
    CoreObject(const std::string& name = "Object");
    virtual ~CoreObject();
    CoreObject(const CoreObject&) = delete;
    CoreObject& operator=(const CoreObject&) = delete;

    // Identity
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }
    const UUID& GetUUID() const { return m_UUID; }
    bool IsActive() const { return m_Active; }
    void SetActive(bool active);

    // Transform (direct access — always exists)
    Transform& GetTransform() { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }

    // Parent / Child hierarchy
    void SetParent(CoreObject* parent, bool worldPositionStays = true);
    CoreObject* GetParent() const;
    size_t GetChildCount() const;
    CoreObject* GetChild(size_t index) const;
    void AddChild(CoreObject* child);
    // Insert a child at a specific index in m_Children (removes it from its
    // current parent first). Used by the hierarchy drag&drop to reorder siblings
    // (Below). Clamps to [0, m_Children.size()].
    void InsertChildAt(CoreObject* child, size_t index);
    void RemoveChild(CoreObject* child);
    const std::vector<CoreObject*>& GetChildren() const { return m_Children; }

    // Scene
    Scene* GetScene() const { return m_Scene; }

    // Components. Data components (IsDataComponent<T>) live DIRECTLY in the ECS
    // pools (contiguous, no box); lifecycle components stay boxed as
    // HybridComponent<T>. Either way, one per type per object.
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        assert(m_Transform.IsEcsBacked() && "CoreObject must be ECS-backed to hold components");
        if (T* existing = GetComponent<T>())
            return *existing;
        T& ref = IsDataComponent<T>::value
            ? m_Transform.GetEcsWorld()->Add<T>(m_Transform.GetEcsEntity())
            : m_Transform.GetEcsWorld()->AddHybrid<T>(m_Transform.GetEcsEntity(), std::forward<Args>(args)...);
        ref.m_Owner = this;
        ref.OnAwake();
        NotifyStructuralChange();
        return ref;
    }

    template<typename T>
    T* GetComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        if (!m_Transform.IsEcsBacked())
            return nullptr;
        return IsDataComponent<T>::value
            ? m_Transform.GetEcsWorld()->Get<T>(m_Transform.GetEcsEntity())
            : m_Transform.GetEcsWorld()->GetHybrid<T>(m_Transform.GetEcsEntity());
    }

    template<typename T>
    const T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        if (!m_Transform.IsEcsBacked())
            return nullptr;
        return IsDataComponent<T>::value
            ? m_Transform.GetEcsWorld()->Get<T>(m_Transform.GetEcsEntity())
            : m_Transform.GetEcsWorld()->GetHybrid<T>(m_Transform.GetEcsEntity());
    }

    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        if (!m_Transform.IsEcsBacked())
            return;
        if (IsDataComponent<T>::value)
            m_Transform.GetEcsWorld()->Remove<T>(m_Transform.GetEcsEntity());
        else
            m_Transform.GetEcsWorld()->Remove<ECS::HybridComponent<T>>(m_Transform.GetEcsEntity());
        NotifyStructuralChange();
    }

    // Lifecycle (called by Scene)
    virtual void OnUpdate(float deltaTime);

protected:
    friend class Scene;

    // Invalidates the owning Scene's query caches (component added/removed or
    // reparent). Defined in CoreObject.cpp (needs the full Scene type).
    void NotifyStructuralChange();

    std::string m_Name;
    UUID m_UUID;
    bool m_Active = true;

    Transform m_Transform;

    CoreObject* m_Parent = nullptr;
    std::vector<CoreObject*> m_Children;

    Scene* m_Scene = nullptr;
};

} // namespace Leir
