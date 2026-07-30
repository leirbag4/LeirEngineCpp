#pragma once
#include "Color.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Leir {

struct Color32 {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color32() = default;
    constexpr Color32(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    explicit Color32(const glm::u8vec4& v) : r(v.x), g(v.y), b(v.z), a(v.w) {}
    explicit Color32(const Color& c)
        : r((uint8_t)(Mathf::Clamp01(c.r) * 255.0f + 0.5f))
        , g((uint8_t)(Mathf::Clamp01(c.g) * 255.0f + 0.5f))
        , b((uint8_t)(Mathf::Clamp01(c.b) * 255.0f + 0.5f))
        , a((uint8_t)(Mathf::Clamp01(c.a) * 255.0f + 0.5f))
    {}

    operator glm::u8vec4() const { return {r, g, b, a}; }
    explicit operator Color() const {
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    static Color32 FromGLM(const glm::u8vec4& v) { return Color32(v); }

    static Color32 Red() { return {255, 0, 0, 255}; }
    static Color32 Green() { return {0, 255, 0, 255}; }
    static Color32 Blue() { return {0, 0, 255, 255}; }
    static Color32 White() { return {255, 255, 255, 255}; }
    static Color32 Black() { return {0, 0, 0, 255}; }
    static Color32 Clear() { return {0, 0, 0, 0}; }
};

} // namespace Leir