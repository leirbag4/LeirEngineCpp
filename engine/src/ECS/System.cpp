#include "LeirEngine/ECS/System.h"
#include "LeirEngine/ECS/CommandBuffer.h"
#include "LeirEngine/Core/JobSystem.h"

#include <algorithm>

namespace Leir {
namespace ECS {

void SystemPipeline::Add(ISystem* system, SystemPhase phase)
{
    if (!system)
        return;
    switch (phase) {
    case SystemPhase::FixedUpdate: m_Fixed.push_back(system); break;
    case SystemPhase::Update: m_Update.push_back(system); break;
    case SystemPhase::Render: m_Render.push_back(system); break;
    }
}

void SystemPipeline::Run(float fixedDt, float dt, JobSystem* jobs)
{
    RunPhase(m_Fixed, fixedDt, jobs);
    RunPhase(m_Update, dt, jobs);
    RunPhase(m_Render, dt, jobs);
}

namespace {
// Two systems conflict when they share a component type and at least one WRITES
// it — only then must they run sequentially.
bool Conflicts(ISystem* a, ISystem* b)
{
    auto aa = a->GetAccess();
    auto bb = b->GetAccess();
    for (const auto& x : aa)
        for (const auto& y : bb)
            if (x.typeId == y.typeId && (x.write || y.write))
                return true;
    return false;
}
} // namespace

void SystemPipeline::RunPhase(std::vector<ISystem*>& systems, float dt, JobSystem* jobs)
{
    const size_t n = systems.size();
    if (n == 0)
        return;
    if (n == 1 || !jobs || jobs->ThreadCount() <= 1) {
        for (ISystem* s : systems)
            s->Update(dt);
        return;
    }

    // Level-based topological schedule: for every conflicting pair (i, j) with
    // i registered before j, j must run after i -> level[j] >= level[i]+1.
    // Systems at the same level are pairwise independent -> parallel-safe.
    std::vector<int> level(n, 0);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (Conflicts(systems[i], systems[j]))
                level[j] = std::max(level[j], level[i] + 1);

    int maxLevel = 0;
    for (int l : level)
        maxLevel = std::max(maxLevel, l);

    for (int l = 0; l <= maxLevel; ++l) {
        std::vector<ISystem*> bucket;
        for (size_t i = 0; i < n; ++i)
            if (level[i] == l)
                bucket.push_back(systems[i]);
        if (bucket.size() == 1) {
            bucket[0]->Update(dt);
            continue;
        }
        // Independent systems at this level run in parallel.
        jobs->ParallelFor(bucket.size(), [&](size_t k) { bucket[k]->Update(dt); });
    }
}

void CommandBuffer::Replay(World& world)
{
    for (auto& op : m_Ops)
        op(world);
    m_Ops.clear();
}

} // namespace ECS
} // namespace Leir