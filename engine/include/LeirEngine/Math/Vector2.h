#pragma once
#include "Mathf.h"
#include <glm/glm.hpp>
#include <cmath>

namespace Leir {

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}
    Vector2(const glm::vec2& v) : x(v.x), y(v.y) {}

    operator glm::vec2() const { return {x, y}; }

    static Vector2 FromGLM(const glm::vec2& v) { return Vector2(v); }

    // Constants
    static Vector2 Zero() { return {0.0f, 0.0f}; }
    static Vector2 One() { return {1.0f, 1.0f}; }
    static Vector2 Right() { return {1.0f, 0.0f}; }
    static Vector2 Left() { return {-1.0f, 0.0f}; }
    static Vector2 Up() { return {0.0f, 1.0f}; }
    static Vector2 Down() { return {0.0f, -1.0f}; }

    // Arithmetic
    Vector2 operator+(const Vector2& v) const { return {x + v.x, y + v.y}; }
    Vector2 operator-(const Vector2& v) const { return {x - v.x, y - v.y}; }
    Vector2 operator*(float s) const { return {x * s, y * s}; }
    Vector2 operator/(float s) const { return {x / s, y / s}; }
    Vector2 operator-() const { return {-x, -y}; }
    Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; }
    Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }
    Vector2& operator*=(float s) { x *= s; y *= s; return *this; }
    Vector2& operator/=(float s) { x /= s; y /= s; return *this; }

    bool operator==(const Vector2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vector2& v) const { return x != v.x || y != v.y; }

    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    float Length() const { return std::sqrt(x * x + y * y); }
    float SqrLength() const { return x * x + y * y; }

    Vector2 Normalized() const {
        float len = Length();
        return len > 0.0f ? Vector2(x / len, y / len) : Zero();
    }
    void Normalize() {
        float len = Length();
        if (len > 0.0f) { x /= len; y /= len; }
    }

    // Static methods
    static float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }
    static float Cross(const Vector2& a, const Vector2& b) { return a.x * b.y - a.y * b.x; }
    static float Distance(const Vector2& a, const Vector2& b) { return (a - b).Length(); }
    static float Angle(const Vector2& from, const Vector2& to);
    static float SignedAngle(const Vector2& from, const Vector2& to);
    static Vector2 Lerp(const Vector2& a, const Vector2& b, float t);
    static Vector2 Perpendicular(const Vector2& v) { return {-v.y, v.x}; }
    static Vector2 ClampMagnitude(const Vector2& v, float maxLen);
    static Vector2 Reflect(const Vector2& inDir, const Vector2& inNormal);
};

inline Vector2 operator*(float s, const Vector2& v) { return v * s; }

inline float Vector2::Angle(const Vector2& from, const Vector2& to) {
    float dot = Mathf::Clamp(Dot(from.Normalized(), to.Normalized()), -1.0f, 1.0f);
    return std::acos(dot) * 57.29577951308232f;
}

inline float Vector2::SignedAngle(const Vector2& from, const Vector2& to) {
    float ang = Angle(from, to);
    return Cross(from, to) < 0.0f ? -ang : ang;
}

inline Vector2 Vector2::Lerp(const Vector2& a, const Vector2& b, float t) {
    t = Leir::Mathf::Clamp01(t);
    return a + (b - a) * t;
}

inline Vector2 Vector2::ClampMagnitude(const Vector2& v, float maxLen) {
    float sqr = v.SqrLength();
    if (sqr > maxLen * maxLen) {
        float len = std::sqrt(sqr);
        return v * (maxLen / len);
    }
    return v;
}

inline Vector2 Vector2::Reflect(const Vector2& inDir, const Vector2& inNormal) {
    return inDir - 2.0f * Dot(inDir, inNormal) * inNormal;
}

} // namespace Leir