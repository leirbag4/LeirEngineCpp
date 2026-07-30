#pragma once
#include "Vector3.h"

namespace Leir {

struct Ray {
    Vector3 origin;
    Vector3 direction;

    Ray() = default;
    Ray(const Vector3& origin, const Vector3& direction)
        : origin(origin), direction(direction.Normalized()) {}

    Vector3 GetPoint(float distance) const {
        return origin + direction * distance;
    }
};

} // namespace Leir