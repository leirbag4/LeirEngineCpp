#include "LeirEngine/Scene/Scene.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Objects/Object3D.h"
#include "LeirEngine/Objects/Object2D.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/SpriteRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
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
    m_CachesDirty = true;
    return ptr;
}

Object2D* Scene::CreateObject2D(const std::string& name)
{
    auto obj = std::make_unique<Object2D>(name);
    Object2D* ptr = obj.get();
    ptr->m_Scene = this;
    m_ObjectIndex[ptr->GetUUID()] = ptr;
    m_Objects.push_back(std::move(obj));
    m_CachesDirty = true;
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
    m_CachesDirty = true;
}

void Scene::MoveObject(CoreObject* object, size_t index)
{
    if (!object) return;
    for (size_t i = 0; i < m_Objects.size(); ++i) {
        if (m_Objects[i].get() != object) continue;
        auto ptr = std::move(m_Objects[i]);
        m_Objects.erase(m_Objects.begin() + (ptrdiff_t)i);
        if (i < index) --index; // removal shifted earlier entries
        index = std::min(index, m_Objects.size());
        m_Objects.insert(m_Objects.begin() + (ptrdiff_t)index, std::move(ptr));
        m_CachesDirty = true;
        return;
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

void Scene::RebuildCaches()
{
    m_Renderables.clear();
    m_Cameras.clear();
    m_Lights.clear();

    // Full DFS over the hierarchy. m_Objects holds EVERY object (roots and
    // children alike — parenting doesn't remove it), so we must start the walk
    // only from parentless roots (same rule as the hierarchy panel) and recurse
    // children; otherwise children would be visited twice. Inactive objects are
    // included too — callers filter via IsActive, so toggling active doesn't
    // need a rebuild. GetComponent<T> is O(1) via the type_index registry.
    auto visit = [this](auto&& visit, CoreObject* obj) -> void {
        if (!obj) return;
        if (obj->GetComponent<MeshRenderer>() || obj->GetComponent<SpriteRenderer>())
            m_Renderables.push_back(obj);
        if (obj->GetComponent<Camera>())
            m_Cameras.push_back(obj);
        if (obj->GetComponent<Light>())
            m_Lights.push_back(obj);
        for (auto child : obj->GetChildren())
            visit(visit, child);
    };

    for (auto& root : m_Objects)
        if (!root->GetParent())
            visit(visit, root.get());

    m_CachesDirty = false;
}

const std::vector<CoreObject*>& Scene::GetRenderables()
{
    if (m_CachesDirty) RebuildCaches();
    return m_Renderables;
}

const std::vector<CoreObject*>& Scene::GetCameras()
{
    if (m_CachesDirty) RebuildCaches();
    return m_Cameras;
}

const std::vector<CoreObject*>& Scene::GetLights()
{
    if (m_CachesDirty) RebuildCaches();
    return m_Lights;
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
