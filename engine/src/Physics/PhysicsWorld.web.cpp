#include "LeirEngine/Physics/PhysicsWorld.h"

namespace Leir {

PhysicsWorld& PhysicsWorld::GetInstance() {
    static PhysicsWorld instance;
    return instance;
}

void PhysicsWorld::Init() {}

void PhysicsWorld::Shutdown() {}

void PhysicsWorld::StepPhysics(float) {}

JPH::BodyInterface& PhysicsWorld::GetBodyInterface() {
    static JPH::BodyInterface* dummy = nullptr;
    return *dummy;
}

const JPH::BodyInterface& PhysicsWorld::GetBodyInterface() const {
    static JPH::BodyInterface* dummy = nullptr;
    return *dummy;
}

JPH::PhysicsSystem& PhysicsWorld::GetPhysicsSystem() {
    static JPH::PhysicsSystem* dummy = nullptr;
    return *dummy;
}

PhysicsWorld::~PhysicsWorld() {}

} // namespace Leir