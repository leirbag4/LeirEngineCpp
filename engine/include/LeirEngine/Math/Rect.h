#pragma once
#include "Vector2.h"

namespace Leir {

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Rect() = default;
    constexpr Rect(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}

    float XMin() const { return x; }
    float XMax() const { return x + width; }
    float YMin() const { return y; }
    float YMax() const { return y + height; }

    Vector2 Center() const { return {x + width * 0.5f, y + height * 0.5f}; }
    Vector2 Position() const { return {x, y}; }
    Vector2 Size() const { return {width, height}; }

    bool Contains(const Vector2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }
    bool ContainsStrict(const Vector2& point) const {
        return point.x > x && point.x < x + width &&
               point.y > y && point.y < y + height;
    }

    bool Overlaps(const Rect& other) const {
        return XMin() < other.XMax() && XMax() > other.XMin() &&
               YMin() < other.YMax() && YMax() > other.YMin();
    }

    static Rect MinMaxRect(float xMin, float yMin, float xMax, float yMax) {
        return {xMin, yMin, xMax - xMin, yMax - yMin};
    }

    static Rect Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
};

} // namespace Leir