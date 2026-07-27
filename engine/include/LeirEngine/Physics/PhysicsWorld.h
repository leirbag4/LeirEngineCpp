#pragma once

#include "LeirEngine/Core/Export.h"

#include <cstdint>

namespace JPH {
class PhysicsSystem;
class BodyInterface;
class TempAllocator;
class JobSystem;
}

namespace PhysicsLayers {
    constexpr uint8_t NON_MOVING = 0;
    constexpr uint8_t MOVING = 1;
}

namespace Leir {

class LEIR_API PhysicsWorld {
public:
    static PhysicsWorld& GetInstance();

    void Init();
    void Shutdown();
    void StepPhysics(float deltaTime);

    JPH::BodyInterface& GetBodyInterface();
    const JPH::BodyInterface& GetBodyInterface() const;

    JPH::PhysicsSystem& GetPhysicsSystem();

private:
    PhysicsWorld() = default;
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    JPH::PhysicsSystem* m_PhysicsSystem = nullptr;
    JPH::TempAllocator* m_TempAllocator = nullptr;
    JPH::JobSystem* m_JobSystem = nullptr;

    bool m_Initialized = false;
};

} // namespace Leir
