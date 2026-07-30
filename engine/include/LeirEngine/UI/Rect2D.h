#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

struct LEIR_API AnchorSet {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    static AnchorSet Stretch() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static AnchorSet TopLeft() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static AnchorSet Center() { return {0.5f, 0.5f, 0.5f, 0.5f}; }
    static AnchorSet BottomRight() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
};

struct LEIR_API OffsetSet {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    static OffsetSet All(float v) { return {v, v, v, v}; }
    static OffsetSet Symmetric(float h, float v) { return {h, v, h, v}; }
};

struct LEIR_API Rect2D {
    AnchorSet anchor;
    OffsetSet offset;
    Vector2 pivot = {0.5f, 0.5f};

    Vector4 GetRect(const Vector2& parentSize) const {
        float x = anchor.left * parentSize.x + offset.left;
        float y = anchor.top * parentSize.y + offset.top;
        float w = (anchor.right - anchor.left) * parentSize.x + (offset.right - offset.left);
        float h = (anchor.bottom - anchor.top) * parentSize.y + (offset.bottom - offset.top);
        return {x, y, w, h};
    }

    static Rect2D Absolute(float x, float y, float w, float h, const Vector2& pivot = {0.5f, 0.5f}) {
        Rect2D r;
        r.anchor = AnchorSet::TopLeft();
        r.offset = {x, y, x + w, y + h};
        r.pivot = pivot;
        return r;
    }

    static Rect2D Stretch(const OffsetSet& margins = {}, const Vector2& pivot = {0.5f, 0.5f}) {
        Rect2D r;
        r.anchor = AnchorSet::Stretch();
        r.offset = margins;
        r.pivot = pivot;
        return r;
    }
};

} // namespace Leir
