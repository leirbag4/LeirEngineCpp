#pragma once
#include "Mathf.h"
#include <glm/glm.hpp>
#include <cmath>

namespace Leir {

struct Vector3i {
    int x = 0;
    int y = 0;
    int z = 0;

    Vector3i() = default;
    constexpr Vector3i(int x, int y, int z) : x(x), y(y), z(z) {}
    explicit Vector3i(const glm::ivec3& v) : x(v.x), y(v.y), z(v.z) {}

    operator glm::ivec3() const { return {x, y, z}; }

    static Vector3i FromGLM(const glm::ivec3& v) { return Vector3i(v); }

    static Vector3i Zero() { return {0, 0, 0}; }
    static Vector3i One() { return {1, 1, 1}; }

    Vector3i operator+(const Vector3i& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vector3i operator-(const Vector3i& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vector3i operator*(int s) const { return {x * s, y * s, z * s}; }
    Vector3i operator/(int s) const { return {x / s, y / s, z / s}; }
    Vector3i operator-() const { return {-x, -y, -z}; }
    Vector3i& operator+=(const Vector3i& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vector3i& operator-=(const Vector3i& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vector3i& operator*=(int s) { x *= s; y *= s; z *= s; return *this; }
    Vector3i& operator/=(int s) { x /= s; y /= s; z /= s; return *this; }

    bool operator==(const Vector3i& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vector3i& v) const { return x != v.x || y != v.y || z != v.z; }

    int& operator[](int i) { return (&x)[i]; }
    const int& operator[](int i) const { return (&x)[i]; }

    float Length() const { return std::sqrt((float)(x * x + y * y + z * z)); }
    int SqrLength() const { return x * x + y * y + z * z; }

    static Vector3i Min(const Vector3i& a, const Vector3i& b) {
        return {Mathf::Min(a.x, b.x), Mathf::Min(a.y, b.y), Mathf::Min(a.z, b.z)};
    }
    static Vector3i Max(const Vector3i& a, const Vector3i& b) {
        return {Mathf::Max(a.x, b.x), Mathf::Max(a.y, b.y), Mathf::Max(a.z, b.z)};
    }
    static Vector3i Clamp(const Vector3i& v, const Vector3i& min, const Vector3i& max) {
        return {Mathf::Clamp(v.x, min.x, max.x), Mathf::Clamp(v.y, min.y, max.y), Mathf::Clamp(v.z, min.z, max.z)};
    }
};

inline Vector3i operator*(int s, const Vector3i& v) { return v * s; }

} // namespace Leir