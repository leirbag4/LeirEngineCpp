#include "LeirEngine/ECS/HierarchyTree.h"

#include <algorithm>

namespace Leir {
namespace ECS {

uint32_t HierarchyTree::GetParent(uint32_t entityIndex) const
{
    if (entityIndex >= m_Parent.size())
        return kNullIndex;
    return m_Parent[entityIndex];
}

uint32_t HierarchyTree::GetFirstChild(uint32_t entityIndex) const
{
    if (entityIndex >= m_FirstChild.size())
        return kNullIndex;
    return m_FirstChild[entityIndex];
}

uint32_t HierarchyTree::GetLastChild(uint32_t entityIndex) const
{
    if (entityIndex >= m_LastChild.size())
        return kNullIndex;
    return m_LastChild[entityIndex];
}

uint32_t HierarchyTree::GetNextSibling(uint32_t entityIndex) const
{
    if (entityIndex >= m_NextSibling.size())
        return kNullIndex;
    return m_NextSibling[entityIndex];
}

uint32_t HierarchyTree::GetPreviousSibling(uint32_t entityIndex) const
{
    if (entityIndex >= m_PrevSibling.size())
        return kNullIndex;
    return m_PrevSibling[entityIndex];
}

uint32_t HierarchyTree::GetDepth(uint32_t entityIndex) const
{
    if (entityIndex >= m_Depth.size())
        return 0;
    return m_Depth[entityIndex];
}

bool HierarchyTree::IsRoot(uint32_t entityIndex) const
{
    return GetParent(entityIndex) == kNullIndex;
}

bool HierarchyTree::IsDescendantOf(uint32_t entityIndex, uint32_t ancestor) const
{
    for (uint32_t cur = GetParent(entityIndex); cur != kNullIndex; cur = GetParent(cur))
        if (cur == ancestor)
            return true;
    return false;
}

void HierarchyTree::EnsureIndex(uint32_t entityIndex)
{
    if (entityIndex < m_Parent.size())
        return;
    size_t newSize = (size_t)entityIndex + 1;
    m_Parent.resize(newSize, kNullIndex);
    m_FirstChild.resize(newSize, kNullIndex);
    m_LastChild.resize(newSize, kNullIndex);
    m_NextSibling.resize(newSize, kNullIndex);
    m_PrevSibling.resize(newSize, kNullIndex);
    m_Depth.resize(newSize, 0);
}

void HierarchyTree::SetParent(uint32_t child, uint32_t parent)
{
    EnsureIndex(child);
    if (parent != kNullIndex)
        EnsureIndex(parent);

    // Cycle guard: never reparent an entity under its own descendant.
    if (child == parent || IsDescendantOf(parent, child))
        return;

    Detach(child);

    m_Parent[child] = parent;
    if (parent != kNullIndex) {
        uint32_t last = m_LastChild[parent];
        if (last != kNullIndex) {
            m_NextSibling[last] = child;
            m_PrevSibling[child] = last;
        } else {
            m_FirstChild[parent] = child;
        }
        m_LastChild[parent] = child;
        m_Depth[child] = m_Depth[parent] + 1;
    } else {
        m_Depth[child] = 0;
    }
    EnsureDepthFrom(child);
}

void HierarchyTree::Detach(uint32_t entityIndex)
{
    if (entityIndex >= m_Parent.size())
        return;
    uint32_t parent = m_Parent[entityIndex];
    if (parent == kNullIndex)
        return;

    uint32_t prev = m_PrevSibling[entityIndex];
    uint32_t next = m_NextSibling[entityIndex];
    if (prev != kNullIndex)
        m_NextSibling[prev] = next;
    else
        m_FirstChild[parent] = next;
    if (next != kNullIndex)
        m_PrevSibling[next] = prev;
    else
        m_LastChild[parent] = prev;

    m_Parent[entityIndex] = kNullIndex;
    m_NextSibling[entityIndex] = kNullIndex;
    m_PrevSibling[entityIndex] = kNullIndex;
}

void HierarchyTree::ClearEntity(uint32_t entityIndex)
{
    if (entityIndex >= m_Parent.size())
        return;
    // Detach children from this entity (they become roots).
    uint32_t child = m_FirstChild[entityIndex];
    while (child != kNullIndex) {
        uint32_t next = m_NextSibling[child];
        m_Parent[child] = kNullIndex;
        m_PrevSibling[child] = kNullIndex;
        m_Depth[child] = 0;
        EnsureDepthFrom(child);
        child = next;
    }
    Detach(entityIndex);
    m_FirstChild[entityIndex] = kNullIndex;
    m_LastChild[entityIndex] = kNullIndex;
}

void HierarchyTree::EnsureDepthFrom(uint32_t entityIndex)
{
    uint32_t child = GetFirstChild(entityIndex);
    while (child != kNullIndex) {
        m_Depth[child] = m_Depth[entityIndex] + 1;
        EnsureDepthFrom(child);
        child = GetNextSibling(child);
    }
}

} // namespace ECS
} // namespace Leir