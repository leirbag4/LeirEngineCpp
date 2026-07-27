#include "LeirEngine/Scene/Scene.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Objects/Object3D.h"
#include "LeirEngine/Objects/Object2D.h"
#include "LeirEngine/Physics/PhysicsWorld.h"

#include <algorithm>

namespace Leir {

Scene::Scene(const std::string& name)
    : m_Name(name)
{
}

Scene::~Scene()
{
    m_Objects.clear();
    m_ObjectIndex.clear();
}

Object3D* Scene::CreateObject3D(const std::string& name)
{
    auto obj = std::make_unique<Object3D>(name);
    Object3D* ptr = obj.get();
    ptr->m_Scene = this;
    m_ObjectIndex[ptr->GetUUID()] = ptr;
    m_Objects.push_back(std::move(obj));
    return ptr;
}

Object2D* Scene::CreateObject2D(const std::string& name)
{
    auto obj = std::make_unique<Object2D>(name);
    Object2D* ptr = obj.get();
    ptr->m_Scene = this;
    m_ObjectIndex[ptr->GetUUID()] = ptr;
    m_Objects.push_back(std::move(obj));
    return ptr;
}

void Scene::DestroyObject(CoreObject* object)
{
    if (!object) return;

    // Destroy all children first
    for (auto child : object->m_Children)
        DestroyObject(child);

    m_ObjectIndex.erase(object->GetUUID());

    auto it = std::find_if(m_Objects.begin(), m_Objects.end(),
        [object](const std::unique_ptr<CoreObject>& ptr) { return ptr.get() == object; });
    if (it != m_Objects.end()) {
        m_Objects.erase(it);
    }
}

CoreObject* Scene::FindObjectByUUID(uint64_t uuid) const
{
    auto it = m_ObjectIndex.find(uuid);
    return it != m_ObjectIndex.end() ? it->second : nullptr;
}

CoreObject* Scene::FindObjectByName(const std::string& name) const
{
    for (const auto& obj : m_Objects) {
        if (obj->GetName() == name)
            return obj.get();
    }
    return nullptr;
}

void Scene::OnUpdate(float deltaTime)
{
    PhysicsWorld::GetInstance().StepPhysics(deltaTime);

    for (auto& obj : m_Objects)
        obj->OnUpdate(deltaTime);
}

void Scene::OnRender()
{
    // TODO: render queue
}

} // namespace Leir
