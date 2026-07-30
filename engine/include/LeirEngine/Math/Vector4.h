#pragma once
#include "Mathf.h"
#include "Vector3.h"
#include <glm/glm.hpp>

namespace Leir {

struct Vector4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vector4() = default;
    constexpr Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    constexpr Vector4(const Vector3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}
    Vector4(const glm::vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

    operator glm::vec4() const { return {x, y, z, w}; }
    explicit operator Vector3() const { return {x, y, z}; }

    static Vector4 FromGLM(const glm::vec4& v) { return Vector4(v); }

    static Vector4 Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static Vector4 One() { return {1.0f, 1.0f, 1.0f, 1.0f}; }

    Vector4 operator+(const Vector4& v) const { return {x + v.x, y + v.y, z + v.z, w + v.w}; }
    Vector4 operator-(const Vector4& v) const { return {x - v.x, y - v.y, z - v.z, w - v.w}; }
    Vector4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    Vector4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    Vector4 operator-() const { return {-x, -y, -z, -w}; }
    Vector4& operator+=(const Vector4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    Vector4& operator-=(const Vector4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    Vector4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vector4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }

    bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }

    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
    float SqrLength() const { return x * x + y * y + z * z + w * w; }

    static float Dot(const Vector4& a, const Vector4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
    static float Distance(const Vector4& a, const Vector4& b) { return (a - b).Length(); }
    static Vector4 Lerp(const Vector4& a, const Vector4& b, float t) {
        t = Mathf::Clamp01(t);
        return a + (b - a) * t;
    }
};

inline Vector4 operator*(float s, const Vector4& v) { return v * s; }

} // namespace Leir