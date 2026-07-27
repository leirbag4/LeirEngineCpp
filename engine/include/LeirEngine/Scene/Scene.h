#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Types.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Leir {

class CoreObject;
class Object3D;
class Object2D;

class LEIR_API Scene {
public:
    Scene(const std::string& name = "Untitled Scene");
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    // Object creation / destruction
    Object3D* CreateObject3D(const std::string& name = "Object3D");
    Object2D* CreateObject2D(const std::string& name = "Object2D");
    void DestroyObject(CoreObject* object);

    // Query
    CoreObject* FindObjectByUUID(uint64_t uuid) const;
    CoreObject* FindObjectByName(const std::string& name) const;
    const std::vector<std::unique_ptr<CoreObject>>& GetObjects() const { return m_Objects; }

    // Lifecycle
    void OnUpdate(float deltaTime);
    void OnRender();

private:
    std::string m_Name;
    std::vector<std::unique_ptr<CoreObject>> m_Objects;
    std::unordered_map<uint64_t, CoreObject*> m_ObjectIndex;
};

} // namespace Leir
