#include "LeirEngine/Scene/ECSScene.h"

#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Objects/Object3D.h"
#include "LeirEngine/Objects/Object2D.h"
#include "LeirEngine/Components/MeshRenderer.h"
#include "LeirEngine/Components/SpriteRenderer.h"
#include "LeirEngine/Components/Camera.h"
#include "LeirEngine/Components/Light.h"
#include "LeirEngine/ECS/Tags.h"

#include <algorithm>
#include <functional>

namespace Leir {

using namespace ECS;

ECSScene::ECSScene()
    : m_Transforms(&m_World, &m_Tree)
{
}

ECSScene::~ECSScene() = default;

Object3D* ECSScene::CreateObject3D(const std::string& name)
{
    auto obj = std::make_unique<Object3D>(name);
    Object3D* ptr = obj.get();
    m_ObjectIndex[ptr->GetUUID()] = ptr;
    m_Objects.push_back(std::move(obj));
    CreateEntity(ptr, true);
    m_CachesDirty = true;
    return ptr;
}

Object2D* ECSScene::CreateObject2D(const std::string& name)
{
    auto obj = std::make_unique<Object2D>(name);
    Object2D* ptr = obj.get();
    m_ObjectIndex[ptr->GetUUID()] = ptr;
    m_Objects.push_back(std::move(obj));
    CreateEntity(ptr, false);
    m_CachesDirty = true;
    return ptr;
}

void ECSScene::DestroyObject(CoreObject* object)
{
    if (!object)
        return;

    for (auto child : object->GetChildren())
        DestroyObject(child);

    auto it = m_EntityOf.find(object);
    if (it != m_EntityOf.end()) {
        m_World.Destroy(it->second);
        m_Tree.ClearEntity(it->second.index);
        m_EntityOf.erase(it);
    }
    m_ObjectIndex.erase(object->GetUUID());

    auto oit = std::find_if(m_Objects.begin(), m_Objects.end(),
        [object](const std::unique_ptr<CoreObject>& ptr) { return ptr.get() == object; });
    if (oit != m_Objects.end())
        m_Objects.erase(oit);
    m_CachesDirty = true;
}

void ECSScene::MoveObject(CoreObject* object, size_t index)
{
    if (!object)
        return;
    for (size_t i = 0; i < m_Objects.size(); ++i) {
        if (m_Objects[i].get() != object)
            continue;
        auto ptr = std::move(m_Objects[i]);
        m_Objects.erase(m_Objects.begin() + (ptrdiff_t)i);
        if (i < index)
            --index;
        index = std::min(index, m_Objects.size());
        m_Objects.insert(m_Objects.begin() + (ptrdiff_t)index, std::move(ptr));
        m_CachesDirty = true;
        return;
    }
}

CoreObject* ECSScene::FindObjectByUUID(uint64_t uuid) const
{
    auto it = m_ObjectIndex.find(uuid);
    return it != m_ObjectIndex.end() ? it->second : nullptr;
}

CoreObject* ECSScene::FindObjectByName(const std::string& name) const
{
    for (const auto& obj : m_Objects)
        if (obj->GetName() == name)
            return obj.get();
    return nullptr;
}

Entity ECSScene::CreateEntity(CoreObject* object, bool is3D)
{
    Entity e = m_World.Create();
    m_Transforms.SetLocal(e, LocalTransform{}); // adds LocalTransform + marks dirty
    if (is3D)
        m_World.Add<Tag3D>(e);
    else
        m_World.Add<Tag2D>(e);
    m_Tree.EnsureIndex(e.index);
    m_EntityOf[object] = e;
    return e;
}

void ECSScene::OnUpdate(float dt)
{
    (void)dt;
    SyncStructure();
    m_Transforms.Update();

    // ECS is authoritative for WORLD transforms: write them back into the
    // handles so GetLocalToWorldMatrix() returns the ECS result.
    for (auto& objPtr : m_Objects) {
        CoreObject* obj = objPtr.get();
        auto it = m_EntityOf.find(obj);
        if (it == m_EntityOf.end())
            continue;
        WorldTransform* wt = m_Transforms.GetWorld(it->second);
        if (!wt)
            continue;
        auto& t = obj->GetTransform();
        t.SetWorldPosition(wt->worldPosition);
        t.SetWorldRotation(wt->worldRotation);
        t.SetWorldScale(wt->worldScale);
    }
}

void ECSScene::SyncStructure()
{
    // Safety: any object without an entity gets one.
    for (auto& obj : m_Objects)
        if (!m_EntityOf.count(obj.get()))
            CreateEntity(obj.get(), dynamic_cast<Object3D*>(obj.get()) != nullptr);

    // Reconcile the ECS tree + LocalTransform with the OOP hierarchy, DFS from
    // parentless roots (m_Objects holds every object, roots + children).
    std::function<void(CoreObject*)> visit;
    visit = [this, &visit](CoreObject* obj) {
        Entity e = m_EntityOf[obj];
        CoreObject* parent = obj->GetParent();
        uint32_t parentIdx = parent ? m_EntityOf[parent].index : kNullIndex;
        if (m_Tree.GetParent(e.index) != parentIdx)
            m_Tree.SetParent(e.index, parentIdx);

        auto& t = obj->GetTransform();
        LocalTransform* lt = m_World.Get<LocalTransform>(e);
        if (lt && (lt->position != t.GetLocalPosition()
                   || lt->rotation != t.GetLocalRotation()
                   || lt->scale != t.GetLocalScale())) {
            m_Transforms.SetLocal(e, {t.GetLocalPosition(), t.GetLocalRotation(), t.GetLocalScale()});
        }
        for (auto* c : obj->GetChildren())
            visit(c);
    };
    for (auto& obj : m_Objects)
        if (!obj->GetParent())
            visit(obj.get());
}

void ECSScene::RebuildCaches()
{
    m_Renderables.clear();
    m_Cameras.clear();
    m_Lights.clear();
    for (auto& obj : m_Objects) {
        if (obj->GetComponent<MeshRenderer>() || obj->GetComponent<SpriteRenderer>())
            m_Renderables.push_back(obj.get());
        if (obj->GetComponent<Camera>())
            m_Cameras.push_back(obj.get());
        if (obj->GetComponent<Light>())
            m_Lights.push_back(obj.get());
    }
}

const std::vector<CoreObject*>& ECSScene::GetRenderables()
{
    RebuildCaches();
    return m_Renderables;
}

const std::vector<CoreObject*>& ECSScene::GetCameras()
{
    RebuildCaches();
    return m_Cameras;
}

const std::vector<CoreObject*>& ECSScene::GetLights()
{
    RebuildCaches();
    return m_Lights;
}

Entity ECSScene::EntityOf(const CoreObject* object) const
{
    auto it = m_EntityOf.find(object);
    return it != m_EntityOf.end() ? it->second : kNullEntity;
}

} // namespace Leir