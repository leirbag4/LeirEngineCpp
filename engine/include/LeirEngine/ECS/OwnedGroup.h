#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"

#include <algorithm>
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
//   group.ForEach(fn);            // O(members), no per-entity membership checks
//
// Iteration reads LIVE pool data (no copies), so it is always consistent with
// writes through World::Get/Add/Remove. Row order is NOT stable across removals
// (swap-and-pop). SoA column alignment for SIMD arrives in Fase 2.
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

    size_t Count() const { return m_Members.size(); }

    // Calls fn(components&..., Entity) for every member, in cached order.
    template<typename Fn>
    void ForEach(Fn&& fn)
    {
        auto pools = std::tuple<TypedPool<Ts>*...>(m_World->GetTypedPool<Ts>()...);
        for (uint32_t ei : m_Members) {
            Entity e{ei, m_World->GenerationOf(ei)};
            std::tuple<Ts*...> comps = std::apply(
                [ei](auto*... ps) { return std::make_tuple(ps->Get(ei)...); }, pools);
            std::apply([&](Ts*... ps) { fn(*ps..., e); }, comps);
        }
    }

private:
    bool HasAll(uint32_t ei) const
    {
        return (m_World->GetTypedPool<Ts>()->Has(ei) && ...);
    }

    void Reconcile(uint32_t ei)
    {
        auto it = std::find(m_Members.begin(), m_Members.end(), ei);
        const bool member = it != m_Members.end();
        const bool hasAll = HasAll(ei);
        if (hasAll && !member) {
            m_Members.push_back(ei);
        } else if (!hasAll && member) {
            *it = m_Members.back();
            m_Members.pop_back();
        }
    }

    World* m_World;
    std::vector<uint32_t> m_Members;
};

} // namespace ECS
} // namespace Leir