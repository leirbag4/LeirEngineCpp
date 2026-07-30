#pragma once
#include "Vector2i.h"

namespace Leir {

struct RectInt {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    RectInt() = default;
    constexpr RectInt(int x, int y, int width, int height)
        : x(x), y(y), width(width), height(height) {}

    int XMin() const { return x; }
    int XMax() const { return x + width; }
    int YMin() const { return y; }
    int YMax() const { return y + height; }

    Vector2i Position() const { return {x, y}; }
    Vector2i Size() const { return {width, height}; }

    bool Contains(const Vector2i& point) const {
        return point.x >= x && point.x < x + width &&
               point.y >= y && point.y < y + height;
    }

    bool Overlaps(const RectInt& other) const {
        return XMin() < other.XMax() && XMax() > other.XMin() &&
               YMin() < other.YMax() && YMax() > other.YMin();
    }

    static RectInt Zero() { return {0, 0, 0, 0}; }
};

} // namespace Leir