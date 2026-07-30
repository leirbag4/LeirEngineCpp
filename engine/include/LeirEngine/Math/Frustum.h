#pragma once
#include "Plane.h"
#include "Bounds.h"
#include "Matrix4x4.h"

namespace Leir {

struct Frustum {
    enum Side { Near = 0, Far = 1, Left = 2, Right = 3, Top = 4, Bottom = 5 };
    Plane planes[6];

    Frustum() = default;

    void Update(const Matrix4x4& vp) {
        // Left
        planes[Left] = Plane(
            Vector3(vp(0, 3) + vp(0, 0), vp(1, 3) + vp(1, 0), vp(2, 3) + vp(2, 0)),
            vp(3, 3) + vp(3, 0));
        // Right
        planes[Right] = Plane(
            Vector3(vp(0, 3) - vp(0, 0), vp(1, 3) - vp(1, 0), vp(2, 3) - vp(2, 0)),
            vp(3, 3) - vp(3, 0));
        // Bottom
        planes[Bottom] = Plane(
            Vector3(vp(0, 3) + vp(0, 1), vp(1, 3) + vp(1, 1), vp(2, 3) + vp(2, 1)),
            vp(3, 3) + vp(3, 1));
        // Top
        planes[Top] = Plane(
            Vector3(vp(0, 3) - vp(0, 1), vp(1, 3) - vp(1, 1), vp(2, 3) - vp(2, 1)),
            vp(3, 3) - vp(3, 1));
        // Near
        planes[Near] = Plane(
            Vector3(vp(0, 3) + vp(0, 2), vp(1, 3) + vp(1, 2), vp(2, 3) + vp(2, 2)),
            vp(3, 3) + vp(3, 2));
        // Far
        planes[Far] = Plane(
            Vector3(vp(0, 3) - vp(0, 2), vp(1, 3) - vp(1, 2), vp(2, 3) - vp(2, 2)),
            vp(3, 3) - vp(3, 2));
    }

    bool Intersects(const Bounds& bounds) const {
        for (int i = 0; i < 6; ++i) {
            if (!bounds.IntersectPlane(planes[i]))
                return false;
        }
        return true;
    }
};

} // namespace Leir