#pragma once

#include "LeirEngine/Core/CoreObject.h"
#include "LeirEngine/Math/Vector3.h"

namespace Leir {

class LEIR_API Object3D : public CoreObject {
public:
    Object3D(const std::string& name = "Object3D");
    ~Object3D() override = default;

    // Bounding box (world-space AABB — updated from transform)
    void SetBounds(const Vector3& min, const Vector3& max);
    Vector3 GetBoundsMin() const { return m_BoundsMin; }
    Vector3 GetBoundsMax() const { return m_BoundsMax; }

private:
    Vector3 m_BoundsMin{-0.5f, -0.5f, -0.5f};
    Vector3 m_BoundsMax{0.5f, 0.5f, 0.5f};
};

} // namespace Leir
