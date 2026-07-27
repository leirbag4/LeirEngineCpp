#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/Export.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Leir {

enum class RigidBodyType {
    Static,
    Dynamic,
    Kinematic
};

class LEIR_API RigidBody : public Component {
public:
    RigidBody() = default;
    ~RigidBody();

    void OnStart() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;

    void SetType(RigidBodyType type);
    RigidBodyType GetType() const { return m_Type; }

    void SetMass(float mass);
    float GetMass() const { return m_Mass; }

    void SetGravityScale(float scale) { m_GravityScale = scale; }
    float GetGravityScale() const { return m_GravityScale; }

    void SetLinearDamping(float damping) { m_LinearDamping = damping; }
    float GetLinearDamping() const { return m_LinearDamping; }

    void SetAngularDamping(float damping) { m_AngularDamping = damping; }
    float GetAngularDamping() const { return m_AngularDamping; }

    void SetLinearVelocity(const glm::vec3& velocity);
    glm::vec3 GetLinearVelocity() const;

    void SetAngularVelocity(const glm::vec3& velocity);
    glm::vec3 GetAngularVelocity() const;

    void AddForce(const glm::vec3& force);
    void AddTorque(const glm::vec3& torque);

    uint32_t GetBodyID() const { return m_BodyID; }
    bool HasBody() const { return m_BodyID != 0; }

private:
    void CreateBody();
    void DestroyBody();
    void SyncTransformToBody();
    void SyncBodyToTransform();

    uint32_t m_BodyID = 0;
    RigidBodyType m_Type = RigidBodyType::Dynamic;
    float m_Mass = 1.0f;
    float m_GravityScale = 1.0f;
    float m_LinearDamping = 0.05f;
    float m_AngularDamping = 0.05f;
};

} // namespace Leir
