#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>
#include <vector>

namespace Leir {
namespace ECS {

// Erased base for a component pool (sparse set). Structural-change paths only:
// destroying an entity calls RemoveFromEntity on every pool. The hot iteration
// paths go through TypedPool<T> directly (no virtuals per entity).
class LEIR_API ComponentPoolBase {
public:
    virtual ~ComponentPoolBase() = default;

    // Removes the component owned by the entity (if present) via swap-and-pop.
    virtual void RemoveFromEntity(uint32_t entityIndex) = 0;
    virtual bool Has(uint32_t entityIndex) const = 0;
    virtual size_t Count() const = 0;
};

// Sparse-set pool for one component type: dense contiguous storage (cache- and
// SIMD-friendly) + a sparse index entityIndex -> denseIndex. Add/remove are
// O(1) (swap-and-pop, no migration — the LeirEngine hybrid choice, see
// TODO_HYBRID_ECS.md §3).
template<typename T>
class TypedPool final : public ComponentPoolBase {
public:
    static constexpr uint32_t kNone = 0xFFFFFFFFu;

    T* Get(uint32_t entityIndex) {
        if (entityIndex >= m_Sparse.size())
            return nullptr;
        uint32_t di = m_Sparse[entityIndex];
        if (di == kNone)
            return nullptr;
        return &m_Dense[di];
    }

    const T* Get(uint32_t entityIndex) const {
        if (entityIndex >= m_Sparse.size())
            return nullptr;
        uint32_t di = m_Sparse[entityIndex];
        if (di == kNone)
            return nullptr;
        return &m_Dense[di];
    }

    // One component per type per entity: returns the existing instance if the
    // entity already owns this type (Unity/Godot semantics).
    T& Add(uint32_t entityIndex) {
        if (entityIndex >= m_Sparse.size())
            m_Sparse.resize((size_t)entityIndex + 1, kNone);
        uint32_t di = m_Sparse[entityIndex];
        if (di != kNone)
            return m_Dense[di];
        m_Sparse[entityIndex] = (uint32_t)m_Dense.size();
        m_DenseEntities.push_back(entityIndex);
        m_Dense.emplace_back();
        return m_Dense.back();
    }

    bool Has(uint32_t entityIndex) const override {
        return entityIndex < m_Sparse.size() && m_Sparse[entityIndex] != kNone;
    }

    void RemoveFromEntity(uint32_t entityIndex) override {
        if (entityIndex >= m_Sparse.size())
            return;
        uint32_t di = m_Sparse[entityIndex];
        if (di == kNone)
            return;
        // Swap-and-pop: move the last dense slot into the hole.
        uint32_t last = (uint32_t)m_Dense.size() - 1;
        if (di != last) {
            m_Dense[di] = std::move(m_Dense[last]);
            m_DenseEntities[di] = m_DenseEntities[last];
            m_Sparse[m_DenseEntities[di]] = di;
        }
        m_Dense.pop_back();
        m_DenseEntities.pop_back();
        m_Sparse[entityIndex] = kNone;
    }

    size_t Count() const override { return m_Dense.size(); }

    T* Data() { return m_Dense.data(); }
    const T* Data() const { return m_Dense.data(); }
    const std::vector<uint32_t>& Entities() const { return m_DenseEntities; }

private:
    std::vector<T> m_Dense;           // dense component data (SoA-ready)
    std::vector<uint32_t> m_DenseEntities; // entity index at each dense slot
    std::vector<uint32_t> m_Sparse;   // entityIndex -> denseIndex (kNone = absent)
};

} // namespace ECS
} // namespace Leir