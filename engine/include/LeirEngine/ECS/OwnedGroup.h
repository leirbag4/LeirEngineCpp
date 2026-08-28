#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"

#include <cstdint>
#include <tuple>
#include <vector>

namespace Leir {
namespace ECS {

// Journal-driven cached group (the hybrid "owned group", TODO_HYBRID_ECS.md
// §4.4): the ordered set of entities that own ALL of Ts, maintained
// incrementally from the world's structural-change journal at each sync point.
//
//   group.Sync(world);            // consume the journal (before ClearJournal)
//   world.ClearJournal();         // the caller owns the sync point
//   group.ForEach(fn);            // O(rows), no per-entity membership checks
//
// STABLE ORDER (professional, not swap-and-pop): removing a member only turns
// its row into a tombstone — the relative order of the remaining members never
// changes, which matters for render z-order, deterministic serialization and
// deterministic simulation. Freed rows are reused on the next add. Membership
// lookup is O(1) via m_RowOf. Iteration reads LIVE pool data (no copies), so it
// is always consistent with writes through World::Get/Add/Remove.
template<typename... Ts>
class OwnedGroup {
public:
    explicit OwnedGroup(World* world)
        : m_World(world)
    {
        static_assert(sizeof...(Ts) > 0, "OwnedGroup requires at least one component type");
    }

    // Applies the world's journal (must run before World::ClearJournal).
    void Sync(const World& world)
    {
        for (const auto& rec : world.GetJournal())
            Reconcile(rec.entityIndex);
    }

    size_t Count() const { return m_AliveCount; }

    // Calls fn(components&..., Entity) for every member, in stable order.
    template<typename Fn>
    void ForEach(Fn&& fn)
    {
        auto pools = std::tuple<TypedPool<Ts>*...>(m_World->GetTypedPool<Ts>()...);
        for (size_t row = 0; row < m_Members.size(); ++row) {
            if (!m_Alive[row])
                continue; // tombstone
            const uint32_t ei = m_Members[row];
            Entity e{ei, m_World->GenerationOf(ei)};
            std::tuple<Ts*...> comps = std::apply(
                [ei](auto*... ps) { return std::make_tuple(ps->Get(ei)...); }, pools);
            std::apply([&](Ts*... ps) { fn(*ps..., e); }, comps);
        }
    }

private:
    static constexpr uint32_t kNoRow = 0xFFFFFFFFu;

    bool HasAll(uint32_t ei) const
    {
        return (m_World->GetTypedPool<Ts>()->Has(ei) && ...);
    }

    bool IsMember(uint32_t ei) const
    {
        return ei < m_RowOf.size() && m_RowOf[ei] != kNoRow;
    }

    void Reconcile(uint32_t ei)
    {
        const bool member = IsMember(ei);
        const bool hasAll = HasAll(ei);
        if (hasAll && !member)
            Add(ei);
        else if (!hasAll && member)
            Remove(ei);
    }

    void Add(uint32_t ei)
    {
        size_t row;
        if (!m_FreeRows.empty()) {
            row = m_FreeRows.back();
            m_FreeRows.pop_back();
            m_Members[row] = ei;
            m_Alive[row] = 1;
        } else {
            row = m_Members.size();
            m_Members.push_back(ei);
            m_Alive.push_back(1);
        }
        if (m_RowOf.size() <= ei)
            m_RowOf.resize((size_t)ei + 1, kNoRow);
        m_RowOf[ei] = (uint32_t)row;
        ++m_AliveCount;
    }

    void Remove(uint32_t ei)
    {
        if (ei >= m_RowOf.size())
            return;
        const size_t row = m_RowOf[ei];
        if (row == kNoRow)
            return;
        m_Alive[row] = 0; // tombstone: order of the remaining members is preserved
        m_FreeRows.push_back((uint32_t)row);
        m_RowOf[ei] = kNoRow;
        --m_AliveCount;
    }

    World* m_World;
    std::vector<uint32_t> m_Members;  // entity index per row (tombstoned rows skipped)
    std::vector<uint8_t> m_Alive;     // row is a live member?
    std::vector<uint32_t> m_FreeRows; // reusable tombstoned rows
    std::vector<uint32_t> m_RowOf;    // entity index -> row (kNoRow = not a member)
    size_t m_AliveCount = 0;
};

} // namespace ECS
} // namespace Leir