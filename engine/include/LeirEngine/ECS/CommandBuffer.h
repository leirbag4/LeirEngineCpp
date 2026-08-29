#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"

#include <functional>
#include <mutex>
#include <vector>

namespace Leir {
namespace ECS {

// Deferred structural changes (Unity EntityCommandBuffer / Bevy Commands
// pattern): systems enqueue create/destroy/add/remove while iterating, and the
// buffer replays them at a sync point — so iteration iterators are never
// invalidated mid-loop. Thread-safe (mutex): systems running in parallel on the
// JobSystem enqueue safely; Replay swaps the ops out atomically and applies
// them on the caller (sync) thread.
class LEIR_API CommandBuffer {
public:
    void Destroy(Entity e)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ops.emplace_back([e](World& w) { w.Destroy(e); });
    }

    template<typename T>
    void Add(Entity e, const T& value)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ops.emplace_back([e, value](World& w) { w.Add<T>(e) = value; });
    }

    template<typename T>
    void Remove(Entity e)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ops.emplace_back([e](World& w) { w.Remove<T>(e); });
    }

    void Replay(World& world);
    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Ops.empty();
    }
    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ops.clear();
    }

private:
    mutable std::mutex m_Mutex;
    std::vector<std::function<void(World&)>> m_Ops;
};

} // namespace ECS
} // namespace Leir