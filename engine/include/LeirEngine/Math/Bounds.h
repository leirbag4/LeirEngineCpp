#pragma once
#include "Vector3.h"
#include "Plane.h"

namespace Leir {

struct Bounds {
    Vector3 center;
    Vector3 size;

    Bounds() = default;
    Bounds(const Vector3& center, const Vector3& size) : center(center), size(size) {}

    Vector3 Extents() const { return {size.x * 0.5f, size.y * 0.5f, size.z * 0.5f}; }
    Vector3 Min() const { return center - Extents(); }
    Vector3 Max() const { return center + Extents(); }

    bool Contains(const Vector3& point) const {
        Vector3 min = Min();
        Vector3 max = Max();
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool Intersects(const Bounds& other) const {
        Vector3 aMin = Min(), aMax = Max();
        Vector3 bMin = other.Min(), bMax = other.Max();
        return aMin.x <= bMax.x && aMax.x >= bMin.x &&
               aMin.y <= bMax.y && aMax.y >= bMin.y &&
               aMin.z <= bMax.z && aMax.z >= bMin.z;
    }

    void Encapsulate(const Vector3& point) {
        Vector3 min = Min(), max = Max();
        min.x = Mathf::Min(min.x, point.x);
        min.y = Mathf::Min(min.y, point.y);
        min.z = Mathf::Min(min.z, point.z);
        max.x = Mathf::Max(max.x, point.x);
        max.y = Mathf::Max(max.y, point.y);
        max.z = Mathf::Max(max.z, point.z);
        center = (min + max) * 0.5f;
        size = max - min;
    }

    void Encapsulate(const Bounds& other) {
        Encapsulate(other.Min());
        Encapsulate(other.Max());
    }

    void Expand(float amount) {
        size.x += amount;
        size.y += amount;
        size.z += amount;
    }

    void Expand(const Vector3& amount) {
        size += amount;
    }

    // Frustum intersection (simple AABB vs plane test)
    bool IntersectPlane(const Plane& plane) const {
        Vector3 extents = Extents();
        Vector3 normal = plane.normal;
        float r = extents.x * std::abs(normal.x) + extents.y * std::abs(normal.y) + extents.z * std::abs(normal.z);
        float s = Vector3::Dot(plane.normal, center) - plane.distance;
        return std::abs(s) <= r;
    }
};

} // namespace Leir