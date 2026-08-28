#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Leir {
namespace ECS {

enum class SystemPhase : uint8_t {
    FixedUpdate, // deterministic, fixed timestep (physics)
    Update,      // gameplay
    Render,      // build render commands
};

// Base for an ECS system: a free-running update that iterates pools/groups.
// Systems are registered in a SystemPipeline; the registration order IS the
// declared dependency order for v1 (automatic parallel scheduling by declared
// read/write access arrives in Fase 2).
class LEIR_API ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(float dt) = 0;
    const std::string& Name() const { return m_Name; }

protected:
    explicit ISystem(std::string name)
        : m_Name(std::move(name))
    {
    }

    std::string m_Name;
};

// Ordered pipeline of systems per phase. Run() executes FixedUpdate -> Update ->
// Render in registration order within each phase. Structural changes made by a
// system go through a CommandBuffer replayed at the sync points between phases
// (the caller owns the buffer).
class LEIR_API SystemPipeline {
public:
    void Add(ISystem* system, SystemPhase phase);
    void Run(float fixedDt, float dt);
    size_t Count() const { return m_Fixed.size() + m_Update.size() + m_Render.size(); }

private:
    std::vector<ISystem*> m_Fixed;
    std::vector<ISystem*> m_Update;
    std::vector<ISystem*> m_Render;
};

} // namespace ECS
} // namespace Leir