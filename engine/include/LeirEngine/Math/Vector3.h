#pragma once
#include "Mathf.h"
#include <glm/glm.hpp>
#include <cmath>

namespace Leir {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vector3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

    operator glm::vec3() const { return {x, y, z}; }

    static Vector3 FromGLM(const glm::vec3& v) { return Vector3(v); }

    // Constants
    static Vector3 Zero() { return {0.0f, 0.0f, 0.0f}; }
    static Vector3 One() { return {1.0f, 1.0f, 1.0f}; }
    static Vector3 Forward() { return {0.0f, 0.0f, -1.0f}; }
    static Vector3 Back() { return {0.0f, 0.0f, 1.0f}; }
    static Vector3 Up() { return {0.0f, 1.0f, 0.0f}; }
    static Vector3 Down() { return {0.0f, -1.0f, 0.0f}; }
    static Vector3 Right() { return {1.0f, 0.0f, 0.0f}; }
    static Vector3 Left() { return {-1.0f, 0.0f, 0.0f}; }

    // Arithmetic
    Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vector3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vector3 operator-() const { return {-x, -y, -z}; }
    Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    Vector3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

    bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vector3& v) const { return x != v.x || y != v.y || z != v.z; }

    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float SqrLength() const { return x * x + y * y + z * z; }

    Vector3 Normalized() const {
        float len = Length();
        return len > 0.0f ? Vector3(x / len, y / len, z / len) : Zero();
    }
    void Normalize() {
        float len = Length();
        if (len > 0.0f) { x /= len; y /= len; z /= len; }
    }

    // Static methods
    static float Dot(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static Vector3 Cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    static float Distance(const Vector3& a, const Vector3& b) { return (a - b).Length(); }
    static float Angle(const Vector3& from, const Vector3& to);
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
    static Vector3 Slerp(const Vector3& a, const Vector3& b, float t);
    static Vector3 Project(const Vector3& v, const Vector3& onNormal);
    static Vector3 Reflect(const Vector3& inDir, const Vector3& inNormal);
    static Vector3 ClampMagnitude(const Vector3& v, float maxLen);
    static void OrthoNormalize(Vector3& normal, Vector3& tangent);
};

inline Vector3 operator*(float s, const Vector3& v) { return v * s; }

inline float Vector3::Angle(const Vector3& from, const Vector3& to) {
    float dot = Mathf::Clamp(Dot(from.Normalized(), to.Normalized()), -1.0f, 1.0f);
    return std::acos(dot) * 57.29577951308232f;
}

inline Vector3 Vector3::Lerp(const Vector3& a, const Vector3& b, float t) {
    t = Mathf::Clamp01(t);
    return a + (b - a) * t;
}

inline Vector3 Vector3::Slerp(const Vector3& a, const Vector3& b, float t) {
    float dot = Mathf::Clamp(Dot(a.Normalized(), b.Normalized()), -1.0f, 1.0f);
    float angle = std::acos(dot) * t;
    Vector3 dir = (b - a * dot).Normalized();
    return a * std::cos(angle) + dir * std::sin(angle);
}

inline Vector3 Vector3::Project(const Vector3& v, const Vector3& onNormal) {
    float sqrMag = onNormal.SqrLength();
    return sqrMag > 0.0f ? onNormal * (Dot(v, onNormal) / sqrMag) : Zero();
}

inline Vector3 Vector3::Reflect(const Vector3& inDir, const Vector3& inNormal) {
    return inDir - 2.0f * Dot(inDir, inNormal) * inNormal;
}

inline Vector3 Vector3::ClampMagnitude(const Vector3& v, float maxLen) {
    float sqr = v.SqrLength();
    if (sqr > maxLen * maxLen) {
        float len = std::sqrt(sqr);
        return v * (maxLen / len);
    }
    return v;
}

inline void Vector3::OrthoNormalize(Vector3& normal, Vector3& tangent) {
    normal.Normalize();
    tangent = tangent - Project(tangent, normal);
    tangent.Normalize();
}

} // namespace Leir