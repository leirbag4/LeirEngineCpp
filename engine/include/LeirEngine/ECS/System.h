#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Leir {

class JobSystem;

namespace ECS {

class CommandBuffer;
class World;

enum class SystemPhase : uint8_t {
    FixedUpdate, // deterministic, fixed timestep (physics)
    Update,      // gameplay
    Render,      // build render commands
};

// A system's declared access to a component type (Fase 2 scheduler): typeId is
// the ECS component type (World::ComponentType<T>()), write = true means the
// system mutates it. Two systems conflict if they share a type and at least one
// WRITES it — only then must they run sequentially (registration order).
struct SystemAccess {
    uint32_t typeId = 0;
    bool write = false;
};

// Base for an ECS system: a free-running update that iterates pools/groups.
// Systems declare their data access (GetAccess) so the SystemPipeline can run
// independent systems in parallel via the JobSystem (Fase 2).
class LEIR_API ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(float dt) = 0;
    virtual std::vector<SystemAccess> GetAccess() const { return {}; }
    const std::string& Name() const { return m_Name; }

protected:
    explicit ISystem(std::string name)
        : m_Name(std::move(name))
    {
    }

    std::string m_Name;
};

// Ordered pipeline of systems per phase. Run() executes FixedUpdate -> Update ->
// Render in registration order within each phase, but systems whose declared
// access does NOT conflict are scheduled on the JobSystem in parallel (level
// based topological scheduling; conflicting systems keep the registration
// order). With no JobSystem (or a single thread) it runs sequentially. A
// CommandBuffer + World may be passed: the deferred structural changes are
// replayed at the sync points BETWEEN phases (parallel-safe).
class LEIR_API SystemPipeline {
public:
    void Add(ISystem* system, SystemPhase phase);
    void Run(float fixedDt, float dt, JobSystem* jobs = nullptr,
             CommandBuffer* cb = nullptr, World* world = nullptr);
    size_t Count() const { return m_Fixed.size() + m_Update.size() + m_Render.size(); }

private:
    void RunPhase(std::vector<ISystem*>& systems, float dt, JobSystem* jobs);

    std::vector<ISystem*> m_Fixed;
    std::vector<ISystem*> m_Update;
    std::vector<ISystem*> m_Render;
};

} // namespace ECS
} // namespace Leir