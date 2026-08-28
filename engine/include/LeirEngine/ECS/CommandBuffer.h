#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"
#include "LeirEngine/ECS/World.h"

#include <functional>
#include <vector>

namespace Leir {
namespace ECS {

// Deferred structural changes (Unity EntityCommandBuffer / Bevy Commands
// pattern): systems enqueue create/destroy/add/remove while iterating, and the
// buffer replays them at a sync point — so iteration iterators are never
// invalidated mid-loop. Data-carrying adds store the value in the op.
class LEIR_API CommandBuffer {
public:
    void Destroy(Entity e)
    {
        m_Ops.emplace_back([e](World& w) { w.Destroy(e); });
    }

    template<typename T>
    void Add(Entity e, const T& value)
    {
        m_Ops.emplace_back([e, value](World& w) { w.Add<T>(e) = value; });
    }

    template<typename T>
    void Remove(Entity e)
    {
        m_Ops.emplace_back([e](World& w) { w.Remove<T>(e); });
    }

    void Replay(World& world);
    bool IsEmpty() const { return m_Ops.empty(); }
    void Clear() { m_Ops.clear(); }

private:
    std::vector<std::function<void(World&)>> m_Ops;
};

} // namespace ECS
} // namespace Leir