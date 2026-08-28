#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Types.h"
#include "LeirEngine/Core/UUID.h"
#include "LeirEngine/Core/Transform.h"
#include "LeirEngine/Core/Component.h"

#include <string>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>

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

    // Components
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        // One component per type (Unity/Godot semantics): adding an existing type
        // returns the live instance instead of creating a duplicate.
        if (T* existing = GetComponent<T>())
            return *existing;
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->m_Owner = this;
        T* ptr = comp.get();
        m_ComponentIndex[std::type_index(typeid(T))] = m_Components.size();
        m_Components.push_back(std::move(comp));
        ptr->OnAwake();
        NotifyStructuralChange();
        return *ptr;
    }

    template<typename T>
    T* GetComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        auto it = m_ComponentIndex.find(std::type_index(typeid(T)));
        if (it == m_ComponentIndex.end())
            return nullptr;
        return static_cast<T*>(m_Components[it->second].get());
    }

    template<typename T>
    const T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        auto it = m_ComponentIndex.find(std::type_index(typeid(T)));
        if (it == m_ComponentIndex.end())
            return nullptr;
        return static_cast<const T*>(m_Components[it->second].get());
    }

    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        auto it = m_ComponentIndex.find(std::type_index(typeid(T)));
        if (it == m_ComponentIndex.end())
            return;
        size_t idx = it->second;
        m_Components[idx]->OnDestroy();
        m_Components.erase(m_Components.begin() + (ptrdiff_t)idx);
        m_ComponentIndex.erase(it);
        // Components after the removed slot shifted down: refresh their index.
        for (size_t i = idx; i < m_Components.size(); ++i)
            m_ComponentIndex[std::type_index(typeid(*m_Components[i]))] = i;
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

    std::vector<std::unique_ptr<Component>> m_Components;
    // Component type → index into m_Components (O(1) GetComponent<T>; kills the
    // old linear dynamic_cast scans). Refreshed on add/remove (slot shift).
    std::unordered_map<std::type_index, size_t> m_ComponentIndex;

    Scene* m_Scene = nullptr;
};

} // namespace Leir
