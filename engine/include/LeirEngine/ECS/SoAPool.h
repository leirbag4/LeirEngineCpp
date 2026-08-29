#pragma once

#include "LeirEngine/Core/Export.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Leir {
namespace ECS {

// SoA (structure-of-arrays) component pool — "storage SoA por campo"
// (TODO_HYBRID_ECS.md §5/§10, Incremento 5). Stores a pure-float POD component
// T as ONE contiguous float column per field (kFloats = sizeof(T) / 4) instead
// of struct rows (AoS). A column is a flat float array, so hot systems process
// a single field with Mathf::Simd*Floats / Frustum::CullBatch directly — no
// gathers, cache-friendly, SIMD-ready.
//
//   SoAPool<P> pool;
//   pool.Set(ei, { ... });                 // add-or-update (O(1), sparse)
//   pool.Remove(ei);                       // swap-and-pop (O(1))
//   bool ok = pool.Get(ei, value);         // materialize a row (column gather)
//   const float* px = pool.Col(0);         // contiguous column for SIMD
//
// Complement (not replacement) of TypedPool<T>: keep TypedPool for struct-wise
// random access (World::Get returns T&); use SoAPool where a system iterates ONE
// field in bulk (cull positions, particle attributes, crowd columns).
template<typename T>
class SoAPool {
    static_assert(sizeof(T) % sizeof(float) == 0, "SoAPool<T> requires a pure-float POD (sizeof multiple of 4)");
    static_assert(std::is_trivially_copyable<T>::value, "SoAPool<T> requires a trivially copyable type");

public:
    static constexpr size_t kFloats = sizeof(T) / sizeof(float);
    static constexpr uint32_t kNone = 0xFFFFFFFFu;

    void Set(uint32_t entityIndex, const T& value)
    {
        const uint32_t row = RowOf(entityIndex);
        if (row == kNone) {
            Add(entityIndex, value);
            return;
        }
        const float* src = reinterpret_cast<const float*>(&value);
        for (size_t k = 0; k < kFloats; ++k)
            m_Cols[k][row] = src[k];
    }

    void Add(uint32_t entityIndex, const T& value)
    {
        if (m_Sparse.size() <= entityIndex)
            m_Sparse.resize((size_t)entityIndex + 1, kNone);
        const uint32_t row = (uint32_t)m_Entities.size();
        m_Sparse[entityIndex] = row;
        m_Entities.push_back(entityIndex);
        const float* src = reinterpret_cast<const float*>(&value);
        for (size_t k = 0; k < kFloats; ++k)
            m_Cols[k].push_back(src[k]);
    }

    void Remove(uint32_t entityIndex)
    {
        const uint32_t row = RowOf(entityIndex);
        if (row == kNone)
            return;
        const uint32_t last = (uint32_t)m_Entities.size() - 1;
        if (row != last) {
            const uint32_t moved = m_Entities[last];
            m_Entities[row] = moved;
            for (size_t k = 0; k < kFloats; ++k)
                m_Cols[k][row] = m_Cols[k][last];
            m_Sparse[moved] = row;
        }
        for (size_t k = 0; k < kFloats; ++k)
            m_Cols[k].pop_back();
        m_Entities.pop_back();
        m_Sparse[entityIndex] = kNone;
    }

    bool Get(uint32_t entityIndex, T& out) const
    {
        const uint32_t row = RowOf(entityIndex);
        if (row == kNone)
            return false;
        float* dst = reinterpret_cast<float*>(&out);
        for (size_t k = 0; k < kFloats; ++k)
            dst[k] = m_Cols[k][row];
        return true;
    }

    bool Has(uint32_t entityIndex) const { return RowOf(entityIndex) != kNone; }
    size_t Count() const { return m_Entities.size(); }

    // Field columns (contiguous float arrays of Count() entries) for SIMD.
    const float* Col(size_t field) const { return m_Cols[field].data(); }
    float* Col(size_t field) { return m_Cols[field].data(); }
    const std::vector<uint32_t>& Entities() const { return m_Entities; }

    void Clear()
    {
        for (auto& c : m_Cols)
            c.clear();
        m_Entities.clear();
        m_Sparse.clear();
    }

private:
    uint32_t RowOf(uint32_t entityIndex) const
    {
        if (entityIndex >= m_Sparse.size())
            return kNone;
        return m_Sparse[entityIndex];
    }

    std::array<std::vector<float>, kFloats> m_Cols; // one column per float field
    std::vector<uint32_t> m_Entities;               // row -> entityIndex
    std::vector<uint32_t> m_Sparse;                 // entityIndex -> row (kNone = absent)
};

} // namespace ECS
} // namespace Leir