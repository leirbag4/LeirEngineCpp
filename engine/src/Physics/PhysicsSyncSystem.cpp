#include "LeirEngine/Physics/PhysicsSyncSystem.h"
#include "LeirEngine/ECS/World.h"
#include "LeirEngine/ECS/TransformSystem.h"
#include "LeirEngine/Physics/RigidBody.h"
#include "LeirEngine/Physics/Collider.h"
#include "LeirEngine/Physics/PhysicsWorld.h"
#include "PhysicsConversions.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

JPH_SUPPRESS_WARNINGS

namespace Leir {

void PhysicsSyncSystem::CreateBody(ECS::World& world, ECS::TransformSystem& transforms, RigidBody& rb, ECS::Entity e)
{
    if (rb.HasBody())
        return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    auto* wt = transforms.GetWorld(e);
    if (!wt)
        return;

    JPH::ShapeRefC shape;
    auto* collider = world.Get<Collider>(e);
    if (collider) {
        switch (collider->GetShapeType()) {
            case ColliderType::Box:
                shape = new JPH::BoxShape(PhysicsConv::ToJolt(collider->GetHalfExtents()));
                break;
            case ColliderType::Sphere:
                shape = new JPH::SphereShape(collider->GetRadius());
                break;
            case ColliderType::Capsule:
                shape = new JPH::CapsuleShape(collider->GetHalfHeight(), collider->GetRadius());
                break;
        }
    } else {
        shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
    }

    JPH::EMotionType motionType;
    JPH::ObjectLayer layer;
    switch (rb.GetType()) {
        case RigidBodyType::Static:
            motionType = JPH::EMotionType::Static;
            layer = PhysicsLayers::NON_MOVING;
            break;
        case RigidBodyType::Dynamic:
            motionType = JPH::EMotionType::Dynamic;
            layer = PhysicsLayers::MOVING;
            break;
        case RigidBodyType::Kinematic:
            motionType = JPH::EMotionType::Kinematic;
            layer = PhysicsLayers::MOVING;
            break;
    }

    JPH::BodyCreationSettings settings(
        shape,
        PhysicsConv::ToJolt(wt->worldPosition),
        PhysicsConv::ToJolt(wt->worldRotation),
        motionType,
        layer
    );
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
    settings.mLinearDamping = rb.GetLinearDamping();
    settings.mAngularDamping = rb.GetAngularDamping();
    settings.mGravityFactor = rb.GetGravityScale();

    JPH::Body* body = bodyInterface.CreateBody(settings);
    if (body) {
        rb.m_BodyID = body->GetID().GetIndexAndSequenceNumber();
        bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
    }
}

void PhysicsSyncSystem::SyncTransformToBody(RigidBody& rb, ECS::TransformSystem& transforms, ECS::Entity e)
{
    if (!rb.HasBody())
        return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    auto* wt = transforms.GetWorld(e);
    if (!wt)
        return;
    bodyInterface.SetPositionAndRotation(
        JPH::BodyID(rb.m_BodyID),
        PhysicsConv::ToJolt(wt->worldPosition),
        PhysicsConv::ToJolt(wt->worldRotation),
        JPH::EActivation::Activate
    );
}

void PhysicsSyncSystem::SyncBodyToTransform(ECS::TransformSystem& transforms, RigidBody& rb, ECS::Entity e)
{
    if (!rb.HasBody())
        return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    JPH::BodyID id(rb.m_BodyID);
    transforms.SetWorldPosition(e, PhysicsConv::ToGLM(bodyInterface.GetPosition(id)));
    transforms.SetWorldRotation(e, PhysicsConv::ToGLM(bodyInterface.GetRotation(id)));
}

// (the create/sync helpers are static members of PhysicsSyncSystem)

void PhysicsSyncSystem::Update(ECS::World& world, ECS::TransformSystem& transforms)
{
    world.Each<RigidBody>([&](RigidBody& rb, ECS::Entity e) {
        CreateBody(world, transforms, rb, e);
        switch (rb.GetType()) {
            case RigidBodyType::Dynamic:
                SyncBodyToTransform(transforms, rb, e);
                break;
            case RigidBodyType::Kinematic:
                SyncTransformToBody(rb, transforms, e);
                break;
            case RigidBodyType::Static:
                break;
        }
    });
}

} // namespace Leir