#pragma once

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/ECS/Entity.h"

namespace Leir {
namespace ECS { class World; class TransformSystem; }
class RigidBody;

// Drives the RigidBody data components (Incremento 2, TODO_HYBRID_ECS.md §10):
// lazily creates the Jolt body from the entity's Collider + WorldTransform and
// syncs per motion type (Dynamic: Jolt -> world; Kinematic: world -> Jolt;
// Static: nothing). Replaces the old RigidBody OnStart/OnUpdate lifecycle.
// Runs in Scene::OnUpdate right after PhysicsWorld::StepPhysics.
class LEIR_API PhysicsSyncSystem {
public:
    void Update(ECS::World& world, ECS::TransformSystem& transforms);

private:
    static void CreateBody(ECS::World& world, ECS::TransformSystem& transforms, RigidBody& rb, ECS::Entity e);
    static void SyncTransformToBody(RigidBody& rb, ECS::TransformSystem& transforms, ECS::Entity e);
    static void SyncBodyToTransform(ECS::TransformSystem& transforms, RigidBody& rb, ECS::Entity e);
};

} // namespace Leir