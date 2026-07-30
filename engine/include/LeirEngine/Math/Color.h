#pragma once
#include "Mathf.h"
#include "Vector4.h"
#include "Vector3.h"
#include <glm/glm.hpp>
#include <cmath>

namespace Leir {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    Color() = default;
    constexpr Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    explicit Color(const glm::vec4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}
    explicit Color(const Vector4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}

    operator glm::vec4() const { return {r, g, b, a}; }
    explicit operator Vector4() const { return {r, g, b, a}; }

    static Color FromGLM(const glm::vec4& v) { return Color(v); }

    // Common colors
    static Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color Blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static Color Yellow() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
    static Color Cyan() { return {0.0f, 1.0f, 1.0f, 1.0f}; }
    static Color Magenta() { return {1.0f, 0.0f, 1.0f, 1.0f}; }
    static Color Gray() { return {0.5f, 0.5f, 0.5f, 1.0f}; }
    static Color Clear() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    Color operator+(const Color& c) const { return {r + c.r, g + c.g, b + c.b, a + c.a}; }
    Color operator-(const Color& c) const { return {r - c.r, g - c.g, b - c.b, a - c.a}; }
    Color operator*(float s) const { return {r * s, g * s, b * s, a * s}; }
    Color operator*(const Color& c) const { return {r * c.r, g * c.g, b * c.b, a * c.a}; }

    Color& operator+=(const Color& c) { r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
    Color& operator*=(float s) { r *= s; g *= s; b *= s; a *= s; return *this; }

    static Color Lerp(const Color& a, const Color& b, float t) {
        t = Mathf::Clamp01(t);
        return a + (b - a) * t;
    }

    static void RGBToHSV(const Color& color, float& h, float& s, float& v);
    static Color HSVToRGB(float h, float s, float v, bool hdr = false);
};

inline Color operator*(float s, const Color& c) { return c * s; }

inline void Color::RGBToHSV(const Color& color, float& h, float& s, float& v) {
    float max = Mathf::Max(Mathf::Max(color.r, color.g), color.b);
    float min = Mathf::Min(Mathf::Min(color.r, color.g), color.b);
    v = max;
    float delta = max - min;
    if (delta < 1e-6f) { h = 0.0f; s = 0.0f; return; }
    s = delta / max;
    if (color.r >= max) h = (color.g - color.b) / delta;
    else if (color.g >= max) h = 2.0f + (color.b - color.r) / delta;
    else h = 4.0f + (color.r - color.g) / delta;
    h *= 60.0f;
    if (h < 0.0f) h += 360.0f;
}

inline Color Color::HSVToRGB(float h, float s, float v, bool hdr) {
    if (!hdr) { h = Mathf::Clamp(h, 0.0f, 360.0f); s = Mathf::Clamp01(s); v = Mathf::Clamp01(v); }
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - Mathf::Abs(Mathf::Repeat(hp, 2.0f) - 1.0f));
    float m = v - c;
    Color result;
    int sector = (int)hp % 6;
    switch (sector) {
        case 0: result = {c, x, 0.0f, 1.0f}; break;
        case 1: result = {x, c, 0.0f, 1.0f}; break;
        case 2: result = {0.0f, c, x, 1.0f}; break;
        case 3: result = {0.0f, x, c, 1.0f}; break;
        case 4: result = {x, 0.0f, c, 1.0f}; break;
        case 5: result = {c, 0.0f, x, 1.0f}; break;
    }
    result.r += m; result.g += m; result.b += m;
    return result;
}

} // namespace Leir