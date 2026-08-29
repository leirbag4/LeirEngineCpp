#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/ComponentTraits.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector3.h"

namespace Leir {

enum class RigidBodyType {
    Static,
    Dynamic,
    Kinematic
};

// Data component (Incremento 2, TODO_HYBRID_ECS.md §10): the Jolt body is
// created and synced by PhysicsSyncSystem (the old OnStart/OnUpdate lifecycle
// moved there). The dtor destroys the body when the component/entity is
// removed. Methods keep the friendly API and talk to PhysicsWorld directly.
class LEIR_API RigidBody : public Component {
public:
    RigidBody() = default;
    ~RigidBody();

    void SetType(RigidBodyType type);
    RigidBodyType GetType() const { return m_Type; }

    void SetMass(float mass) { m_Mass = mass; }
    float GetMass() const { return m_Mass; }

    void SetGravityScale(float scale) { m_GravityScale = scale; }
    float GetGravityScale() const { return m_GravityScale; }

    void SetLinearDamping(float damping) { m_LinearDamping = damping; }
    float GetLinearDamping() const { return m_LinearDamping; }

    void SetAngularDamping(float damping) { m_AngularDamping = damping; }
    float GetAngularDamping() const { return m_AngularDamping; }

    void SetLinearVelocity(const Vector3& velocity);
    Vector3 GetLinearVelocity() const;

    void SetAngularVelocity(const Vector3& velocity);
    Vector3 GetAngularVelocity() const;

    void AddForce(const Vector3& force);
    void AddTorque(const Vector3& torque);

    uint32_t GetBodyID() const { return m_BodyID; }
    bool HasBody() const { return m_BodyID != 0; }

private:
    friend class PhysicsSyncSystem;
    void DestroyBody();

    uint32_t m_BodyID = 0;
    RigidBodyType m_Type = RigidBodyType::Dynamic;
    float m_Mass = 1.0f;
    float m_GravityScale = 1.0f;
    float m_LinearDamping = 0.05f;
    float m_AngularDamping = 0.05f;
};

} // namespace Leir

template<>
struct Leir::IsDataComponent<Leir::RigidBody> : std::true_type {
};