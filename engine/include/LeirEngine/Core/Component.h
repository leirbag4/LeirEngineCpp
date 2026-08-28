#pragma once

#include "LeirEngine/Core/Export.h"

namespace Leir {

class CoreObject;
class Scene;

class LEIR_API Component {
public:
    virtual ~Component() = default;

    virtual void OnAwake() {}
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnDestroy() {}

    CoreObject* GetOwner() const { return m_Owner; }
    Scene* GetScene() const;

    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }

    // Drives the lifecycle: lazily calls OnStart() once, then OnUpdate() while
    // active. Used by Scene/CoreObject for both the OOP and ECS-hybrid paths.
    void Tick(float deltaTime)
    {
        if (!m_Active)
            return;
        if (!m_Started) {
            m_Started = true;
            OnStart();
        }
        OnUpdate(deltaTime);
    }

private:
    friend class CoreObject;
    CoreObject* m_Owner = nullptr;
    bool m_Active = true;
    bool m_Started = false;
};

} // namespace Leir
