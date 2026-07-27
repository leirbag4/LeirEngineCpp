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

private:
    friend class CoreObject;
    CoreObject* m_Owner = nullptr;
    bool m_Active = true;
    bool m_Started = false;
};

} // namespace Leir
