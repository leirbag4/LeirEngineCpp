#include "LeirEngine/Physics/RigidBody.h"
#include "LeirEngine/Physics/PhysicsWorld.h"
#include "PhysicsConversions.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

JPH_SUPPRESS_WARNINGS

namespace Leir {

RigidBody::~RigidBody() {
    DestroyBody();
}

void RigidBody::SetType(RigidBodyType type) {
    if (m_Type == type) return;
    m_Type = type;
    // The next PhysicsSyncSystem update rebuilds the body with the new type.
    if (HasBody()) DestroyBody();
}

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

void RigidBody::DestroyBody() {
    if (!HasBody()) return;

    auto& bodyInterface = PhysicsWorld::GetInstance().GetBodyInterface();
    JPH::BodyID id(m_BodyID);

    bodyInterface.RemoveBody(id);
    bodyInterface.DestroyBody(id);

    m_BodyID = 0;
}

} // namespace Leir