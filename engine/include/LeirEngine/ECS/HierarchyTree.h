#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"

#include <cstdint>
#include <vector>

namespace Leir {
namespace ECS {

// Compact scene-graph tree over entity indices (the unified hierarchy,
// TODO_HYBRID_ECS.md §4.8): parent / firstChild / lastChild / nextSibling /
// prevSibling adjacency arrays. The nodes ARE entity indices; the tree holds
// STRUCTURE ONLY (no component data). GetParent/GetChildren/SetParent are the
// primitives the future CoreObject bridge and the TransformSystem consume.
// index 0 (the null entity) doubles as the "no parent / no child" sentinel.
class LEIR_API HierarchyTree {
public:
    uint32_t GetParent(uint32_t entityIndex) const;
    uint32_t GetFirstChild(uint32_t entityIndex) const;
    uint32_t GetLastChild(uint32_t entityIndex) const;
    uint32_t GetNextSibling(uint32_t entityIndex) const;
    uint32_t GetPreviousSibling(uint32_t entityIndex) const;
    uint32_t GetDepth(uint32_t entityIndex) const;
    bool IsRoot(uint32_t entityIndex) const;
    bool IsDescendantOf(uint32_t entityIndex, uint32_t ancestor) const;

    // Detaches the child from its current parent (if any) and appends it as the
    // last child of `parent` (kNullIndex = root). Rejects cycles.
    void SetParent(uint32_t child, uint32_t parent);
    void Detach(uint32_t entityIndex);

    // Sizes the adjacency arrays up to entityIndex (idempotent; called by the
    // world on entity allocation).
    void EnsureIndex(uint32_t entityIndex);
    // On entity destroy: unlink from the tree and reset its slots.
    void ClearEntity(uint32_t entityIndex);

private:
    void EnsureDepthFrom(uint32_t entityIndex);

    std::vector<uint32_t> m_Parent;      // entity -> parent (kNullIndex = root)
    std::vector<uint32_t> m_FirstChild;  // entity -> first child (kNullIndex)
    std::vector<uint32_t> m_LastChild;   // entity -> last child (kNullIndex)
    std::vector<uint32_t> m_NextSibling; // entity -> next sibling
    std::vector<uint32_t> m_PrevSibling; // entity -> previous sibling
    std::vector<uint32_t> m_Depth;       // depth in the tree (root = 0)
};

} // namespace ECS
} // namespace Leir