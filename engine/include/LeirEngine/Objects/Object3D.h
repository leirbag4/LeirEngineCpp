#pragma once

#include "LeirEngine/Core/CoreObject.h"

namespace Leir {

class LEIR_API Object3D : public CoreObject {
public:
    Object3D(const std::string& name = "Object3D");
    ~Object3D() override = default;

    // Bounding box (world-space AABB — updated from transform)
    void SetBounds(const glm::vec3& min, const glm::vec3& max);
    glm::vec3 GetBoundsMin() const { return m_BoundsMin; }
    glm::vec3 GetBoundsMax() const { return m_BoundsMax; }

private:
    glm::vec3 m_BoundsMin{-0.5f};
    glm::vec3 m_BoundsMax{0.5f};
};

} // namespace Leir
