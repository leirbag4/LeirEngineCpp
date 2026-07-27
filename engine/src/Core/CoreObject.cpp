#include "LeirEngine/Core/CoreObject.h"

#include <algorithm>

namespace Leir {

CoreObject::CoreObject(const std::string& name)
    : m_Name(name)
{
    m_Transform.SetOwner(this);
}

CoreObject::~CoreObject()
{
    m_Components.clear();
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

    if (m_Parent)
        m_Parent->RemoveChild(this);

    m_Parent = parent;

    if (m_Parent)
        m_Parent->AddChild(this);

    m_Transform.SetParent(parent ? &parent->m_Transform : nullptr, worldPositionStays);
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
    }
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

    for (auto& comp : m_Components) {
        if (comp->IsActive()) {
            if (!comp->m_Started) {
                comp->m_Started = true;
                comp->OnStart();
            }
            comp->OnUpdate(deltaTime);
        }
    }

    for (auto child : m_Children)
        child->OnUpdate(deltaTime);
}

} // namespace Leir
