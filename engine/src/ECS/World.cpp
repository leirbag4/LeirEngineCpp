#include "LeirEngine/ECS/World.h"

#include <algorithm>

namespace Leir {
namespace ECS {

World::World()
{
    // Reserve entity index 0 as the null entity (never handed out).
    m_Generations.push_back(0);
}

Entity World::Create()
{
    uint32_t idx = AllocateIndex();
    uint32_t gen = m_Generations[idx];
    m_LiveIndices.push_back(idx);
    if (m_IndexToLive.size() <= idx)
        m_IndexToLive.resize((size_t)idx + 1, 0xFFFFFFFFu);
    m_IndexToLive[idx] = (uint32_t)m_LiveIndices.size() - 1;

    m_Journal.push_back({ChangeRecord::EntityCreated, idx, 0});
    ++m_ChangeVersion;
    return Entity{idx, gen};
}

void World::Destroy(Entity e)
{
    if (!IsAlive(e))
        return;

    // Clear every component owned by this entity (its index may be recycled).
    for (auto& pool : m_Pools)
        if (pool)
            pool->RemoveFromEntity(e.index);

    uint32_t slot = m_IndexToLive[e.index];
    uint32_t last = (uint32_t)m_LiveIndices.size() - 1;
    if (slot != last) {
        m_LiveIndices[slot] = m_LiveIndices[last];
        m_IndexToLive[m_LiveIndices[slot]] = slot;
    }
    m_LiveIndices.pop_back();
    m_IndexToLive[e.index] = 0xFFFFFFFFu;

    ++m_Generations[e.index]; // invalidate stale handles
    m_FreeList.push_back(e.index);

    m_Journal.push_back({ChangeRecord::EntityDestroyed, e.index, 0});
    ++m_ChangeVersion;
}

bool World::IsAlive(Entity e) const
{
    if (!e)
        return false;
    if (e.index >= m_Generations.size())
        return false;
    if (m_Generations[e.index] != e.generation)
        return false;
    return e.index < m_IndexToLive.size() && m_IndexToLive[e.index] != 0xFFFFFFFFu;
}

uint32_t World::AllocateIndex()
{
    if (!m_FreeList.empty()) {
        uint32_t idx = m_FreeList.back();
        m_FreeList.pop_back();
        return idx;
    }
    uint32_t idx = (uint32_t)m_Generations.size();
    m_Generations.push_back(1);
    return idx;
}

} // namespace ECS
} // namespace Leir