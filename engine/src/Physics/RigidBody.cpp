#include "LeirEngine/Physics/RigidBody.h"
#include "LeirEngine/Physics/Collider.h"
#include "LeirEngine/Physics/PhysicsWorld.h"
#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Core/Transform.h"
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

RigidBody::~RigidBody() {
    DestroyBody();
}

// ---- Lifecycle ----

void RigidBody::OnStart() {
    CreateBody();
}

void RigidBody::OnUpdate(float /*deltaTime*/) {
    if (!HasBody()) return;

    switch (m_Type) {
        case RigidBodyType::Dynamic:
            SyncBodyToTransform();
            break;
        case RigidBodyType::Kinematic:
            SyncTransformToBody();
            break;
        case RigidBodyType::Static:
            break;
    }
}

void RigidBody::OnDestroy() {
    DestroyBody();
}

// ---- Type ----

void RigidBody::SetType(RigidBodyType type) {
    if (m_Type == type) return;
    m_Type = type;
    if (HasBody()) {
        DestroyBody();
        CreateBody();
    }
}

// ---- Velocity ----

void RigidBody::SetLinearVelocity(const Vector3& velocity) {
    if (!HasBody()) return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    bodyInterface.SetLinearVelocity(JPH::BodyID(m_BodyID), PhysicsConv::ToJolt(velocity));
}

Vector3 RigidBody::GetLinearVelocity() const {
    if (!HasBody()) return Vector3::Zero();
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    return PhysicsConv::ToGLM(bodyInterface.GetLinearVelocity(JPH::BodyID(m_BodyID)));
}

void RigidBody::SetAngularVelocity(const Vector3& velocity) {
    if (!HasBody()) return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    bodyInterface.SetAngularVelocity(JPH::BodyID(m_BodyID), PhysicsConv::ToJolt(velocity));
}

Vector3 RigidBody::GetAngularVelocity() const {
    if (!HasBody()) return Vector3::Zero();
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    return PhysicsConv::ToGLM(bodyInterface.GetAngularVelocity(JPH::BodyID(m_BodyID)));
}

// ---- Forces ----

void RigidBody::AddForce(const Vector3& force) {
    if (!HasBody()) return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    bodyInterface.AddForce(JPH::BodyID(m_BodyID), PhysicsConv::ToJolt(force));
}

void RigidBody::AddTorque(const Vector3& torque) {
    if (!HasBody()) return;
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    bodyInterface.AddTorque(JPH::BodyID(m_BodyID), PhysicsConv::ToJolt(torque));
}

// ---- Internal ----

void RigidBody::CreateBody() {
    if (HasBody()) return;

    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    auto* transform = &GetOwner()->GetTransform();

    JPH::ShapeRefC shape;
    auto* collider = GetOwner()->GetComponent<Collider>();

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
    switch (m_Type) {
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
        PhysicsConv::ToJolt(transform->GetWorldPosition()),
        PhysicsConv::ToJolt(transform->GetWorldRotation()),
        motionType,
        layer
    );

    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
    settings.mLinearDamping = m_LinearDamping;
    settings.mAngularDamping = m_AngularDamping;
    settings.mGravityFactor = m_GravityScale;

    JPH::Body* body = bodyInterface.CreateBody(settings);
    if (body) {
        m_BodyID = body->GetID().GetIndexAndSequenceNumber();
        bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
    }
}

void RigidBody::DestroyBody() {
    if (!HasBody()) return;

    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    JPH::BodyID id(m_BodyID);

    bodyInterface.RemoveBody(id);
    bodyInterface.DestroyBody(id);

    m_BodyID = 0;
}

void RigidBody::SyncTransformToBody() {
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    auto* transform = &GetOwner()->GetTransform();
    JPH::BodyID id(m_BodyID);

    bodyInterface.SetPositionAndRotation(
        id,
        PhysicsConv::ToJolt(transform->GetWorldPosition()),
        PhysicsConv::ToJolt(transform->GetWorldRotation()),
        JPH::EActivation::Activate
    );
}

void RigidBody::SyncBodyToTransform() {
    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    auto* transform = &GetOwner()->GetTransform();
    JPH::BodyID id(m_BodyID);

    JPH::Vec3 pos = bodyInterface.GetPosition(id);
    JPH::Quat rot = bodyInterface.GetRotation(id);

    transform->SetWorldPosition(PhysicsConv::ToGLM(pos));
    transform->SetWorldRotation(PhysicsConv::ToGLM(rot));
}

} // namespace Leir
