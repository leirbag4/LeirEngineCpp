#pragma once
#include "Vector3.h"
#include "Ray.h"
#include <cmath>

namespace Leir {

struct Plane {
    Vector3 normal;
    float distance = 0.0f;

    Plane() = default;
    Plane(const Vector3& normal, float distance)
        : normal(normal.Normalized()), distance(distance) {}
    Plane(const Vector3& a, const Vector3& b, const Vector3& c) {
        normal = Vector3::Cross(b - a, c - a).Normalized();
        distance = Vector3::Dot(normal, a);
    }

    void SetNormalAndPosition(const Vector3& n, const Vector3& p) {
        normal = n.Normalized();
        distance = Vector3::Dot(normal, p);
    }

    void Set3Points(const Vector3& a, const Vector3& b, const Vector3& c) {
        normal = Vector3::Cross(b - a, c - a).Normalized();
        distance = Vector3::Dot(normal, a);
    }

    float GetDistanceToPoint(const Vector3& point) const {
        return Vector3::Dot(normal, point) - distance;
    }

    Vector3 ClosestPointOnPlane(const Vector3& point) const {
        float d = GetDistanceToPoint(point);
        return point - normal * d;
    }

    bool Raycast(const Ray& ray, float& enter) const {
        float denom = Vector3::Dot(normal, ray.direction);
        if (std::abs(denom) < 1e-6f) return false;
        enter = (distance - Vector3::Dot(normal, ray.origin)) / denom;
        return enter >= 0.0f;
    }

    bool GetSide(const Vector3& point) const {
        return GetDistanceToPoint(point) >= 0.0f;
    }

    bool SameSide(const Vector3& p1, const Vector3& p2) const {
        return GetSide(p1) == GetSide(p2);
    }
};

} // namespace Leir