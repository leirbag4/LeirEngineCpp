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
        auto comp = std::make_unique<T>(std::forward<Args>(args)...);
        comp->m_Owner = this;
        T* ptr = comp.get();
        m_Components.push_back(std::move(comp));
        ptr->OnAwake();
        return *ptr;
    }

    template<typename T>
    T* GetComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        for (auto& comp : m_Components) {
            T* result = dynamic_cast<T*>(comp.get());
            if (result) return result;
        }
        return nullptr;
    }

    template<typename T>
    const T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        for (const auto& comp : m_Components) {
            const T* result = dynamic_cast<const T*>(comp.get());
            if (result) return result;
        }
        return nullptr;
    }

    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
        for (auto it = m_Components.begin(); it != m_Components.end(); ++it) {
            if (dynamic_cast<T*>(it->get())) {
                (*it)->OnDestroy();
                m_Components.erase(it);
                return;
            }
        }
    }

    // Lifecycle (called by Scene)
    virtual void OnUpdate(float deltaTime);

protected:
    friend class Scene;

    std::string m_Name;
    UUID m_UUID;
    bool m_Active = true;

    Transform m_Transform;

    CoreObject* m_Parent = nullptr;
    std::vector<CoreObject*> m_Children;

    std::vector<std::unique_ptr<Component>> m_Components;

    Scene* m_Scene = nullptr;
};

} // namespace Leir
