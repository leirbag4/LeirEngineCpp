#include "LeirEngine/Physics/Collider.h"

namespace Leir {

void Collider::SetBox(const Vector3& halfExtents) {
    m_ShapeType = ColliderType::Box;
    m_HalfExtents = halfExtents;
}

void Collider::SetSphere(float radius) {
    m_ShapeType = ColliderType::Sphere;
    m_Radius = radius;
}

void Collider::SetCapsule(float radius, float halfHeight) {
    m_ShapeType = ColliderType::Capsule;
    m_Radius = radius;
    m_HalfHeight = halfHeight;
}

} // namespace Leir
