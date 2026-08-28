#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Scene/Scene.h"

#include <algorithm>

namespace Leir {

void CoreObject::NotifyStructuralChange()
{
    if (m_Scene)
        m_Scene->MarkCachesDirty();
}

CoreObject::CoreObject(const std::string& name)
    : m_Name(name)
{
    m_Transform.SetOwner(this);
}

CoreObject::~CoreObject()
{
    if (m_Parent)
        m_Parent->RemoveChild(this);
    for (auto child : m_Children)
        child->m_Parent = nullptr;
    m_Children.clear();
}

void CoreObject::SetActive(bool active)
{
    m_Active = active;
}

void CoreObject::SetParent(CoreObject* parent, bool worldPositionStays)
{
    if (m_Parent == parent)
        return;

    // AddChild links the transform preserving the world (stays=true); for the
    // stays=false request we restore the ORIGINAL local transform afterwards.
    const Vector3 localPos = m_Transform.GetLocalPosition();
    const Quaternion localRot = m_Transform.GetLocalRotation();
    const Vector3 localScale = m_Transform.GetLocalScale();

    if (m_Parent)
        m_Parent->RemoveChild(this);

    m_Parent = parent;

    if (m_Parent) {
        m_Parent->AddChild(this); // tree link + transform parent (world preserved)
        if (!worldPositionStays) {
            m_Transform.SetLocalPosition(localPos);
            m_Transform.SetLocalRotation(localRot);
            m_Transform.SetLocalScale(localScale); // keep the child's local as-is
        }
    } else {
        m_Transform.SetParent(nullptr, worldPositionStays);
    }

    // Reparenting changes the DFS render order -> invalidate scene query caches.
    NotifyStructuralChange();
}

CoreObject* CoreObject::GetParent() const
{
    return m_Parent;
}

size_t CoreObject::GetChildCount() const
{
    return m_Children.size();
}

CoreObject* CoreObject::GetChild(size_t index) const
{
    if (index < m_Children.size())
        return m_Children[index];
    return nullptr;
}

void CoreObject::AddChild(CoreObject* child)
{
    if (!child || child == this)
        return;
    if (std::find(m_Children.begin(), m_Children.end(), child) == m_Children.end()) {
        m_Children.push_back(child);
        child->m_Parent = this;
        // Link the TRANSFORM hierarchy too and preserve the child's WORLD
        // (Unity AddChild semantics: the child stays in place, its locals are
        // re-derived from the new parent). Without this, a reparented child
        // keeps a stale local and its world drifts.
        child->m_Transform.SetParent(&m_Transform, true);
        NotifyStructuralChange();
    }
}

void CoreObject::InsertChildAt(CoreObject* child, size_t index)
{
    if (!child || child == this)
        return;
    // Remove from its current parent (or leave it if already a child of ours).
    if (child->m_Parent != this) {
        if (child->m_Parent)
            child->m_Parent->RemoveChild(child);
    } else {
        auto it = std::find(m_Children.begin(), m_Children.end(), child);
        if (it != m_Children.end())
            m_Children.erase(it);
    }
    index = std::min(index, m_Children.size());
    m_Children.insert(m_Children.begin() + (ptrdiff_t)index, child);
    child->m_Parent = this;
    // Keep the TRANSFORM hierarchy in sync with the CoreObject hierarchy AND
    // preserve the child's WORLD transform (Unity worldPositionStays): after
    // reparenting, recompute the child's local from its world so it never jumps.
    child->m_Transform.SetParent(&m_Transform, true);
    NotifyStructuralChange();
}

void CoreObject::RemoveChild(CoreObject* child)
{
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        m_Children.erase(it);
        child->m_Parent = nullptr;
    }
}

void CoreObject::OnUpdate(float deltaTime)
{
    if (!m_Active)
        return;

    // Components live as ECS hybrids; their lifecycle (OnStart/OnUpdate) is
    // driven by Scene::OnUpdate via the world's hybrid registry. Here we just
    // recurse the (OOP-mirrored) hierarchy.
    for (auto child : m_Children)
        child->OnUpdate(deltaTime);
}

} // namespace Leir
