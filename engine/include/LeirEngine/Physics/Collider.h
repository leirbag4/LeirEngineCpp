#pragma once

#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Core/Export.h"

#include <glm/glm.hpp>

namespace Leir {

enum class ColliderType {
    Box,
    Sphere,
    Capsule
};

class LEIR_API Collider : public Component {
public:
    Collider() = default;

    void SetBox(const glm::vec3& halfExtents);
    void SetSphere(float radius);
    void SetCapsule(float radius, float halfHeight);

    ColliderType GetShapeType() const { return m_ShapeType; }
    const glm::vec3& GetHalfExtents() const { return m_HalfExtents; }
    float GetRadius() const { return m_Radius; }
    float GetHalfHeight() const { return m_HalfHeight; }

private:
    ColliderType m_ShapeType = ColliderType::Box;
    glm::vec3 m_HalfExtents{0.5f};
    float m_Radius = 0.5f;
    float m_HalfHeight = 0.5f;
};

} // namespace Leir
