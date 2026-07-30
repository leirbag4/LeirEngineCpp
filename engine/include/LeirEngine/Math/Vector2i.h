#pragma once
#include "Mathf.h"
#include <glm/glm.hpp>
#include <cmath>

namespace Leir {

struct Vector2i {
    int x = 0;
    int y = 0;

    Vector2i() = default;
    constexpr Vector2i(int x, int y) : x(x), y(y) {}
    explicit Vector2i(const glm::ivec2& v) : x(v.x), y(v.y) {}

    operator glm::ivec2() const { return {x, y}; }

    static Vector2i FromGLM(const glm::ivec2& v) { return Vector2i(v); }

    static Vector2i Zero() { return {0, 0}; }
    static Vector2i One() { return {1, 1}; }

    Vector2i operator+(const Vector2i& v) const { return {x + v.x, y + v.y}; }
    Vector2i operator-(const Vector2i& v) const { return {x - v.x, y - v.y}; }
    Vector2i operator*(int s) const { return {x * s, y * s}; }
    Vector2i operator/(int s) const { return {x / s, y / s}; }
    Vector2i operator-() const { return {-x, -y}; }
    Vector2i& operator+=(const Vector2i& v) { x += v.x; y += v.y; return *this; }
    Vector2i& operator-=(const Vector2i& v) { x -= v.x; y -= v.y; return *this; }
    Vector2i& operator*=(int s) { x *= s; y *= s; return *this; }
    Vector2i& operator/=(int s) { x /= s; y /= s; return *this; }

    bool operator==(const Vector2i& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vector2i& v) const { return x != v.x || y != v.y; }

    int& operator[](int i) { return (&x)[i]; }
    const int& operator[](int i) const { return (&x)[i]; }

    float Length() const { return std::sqrt((float)(x * x + y * y)); }
    int SqrLength() const { return x * x + y * y; }

    static Vector2i Min(const Vector2i& a, const Vector2i& b) {
        return {Mathf::Min(a.x, b.x), Mathf::Min(a.y, b.y)};
    }
    static Vector2i Max(const Vector2i& a, const Vector2i& b) {
        return {Mathf::Max(a.x, b.x), Mathf::Max(a.y, b.y)};
    }
    static Vector2i Clamp(const Vector2i& v, const Vector2i& min, const Vector2i& max) {
        return {Mathf::Clamp(v.x, min.x, max.x), Mathf::Clamp(v.y, min.y, max.y)};
    }
};

inline Vector2i operator*(int s, const Vector2i& v) { return v * s; }

} // namespace Leir