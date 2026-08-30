#pragma once

/**
 * @file Rect2D.h
 * @brief Anchor/offset rectangles for UI layout.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

/**
 * @brief Anchor set: normalized anchors [0,1] for the four edges.
 * @ingroup UI
 */
struct LEIR_API AnchorSet {
    float left = 0.0f;   ///< Left anchor.
    float top = 0.0f;    ///< Top anchor.
    float right = 0.0f;  ///< Right anchor.
    float bottom = 0.0f; ///< Bottom anchor.

    /**
     * @brief Stretch anchors (0,0)-(1,1).
     * @return AnchorSet with stretch anchors.
     */
    static AnchorSet Stretch() { return {0.0f, 0.0f, 1.0f, 1.0f}; }

    /**
     * @brief Top-left anchors (0,0)-(0,0).
     * @return AnchorSet.
     */
    static AnchorSet TopLeft() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    /**
     * @brief Center anchors (0.5,0.5)-(0.5,0.5).
     * @return AnchorSet.
     */
    static AnchorSet Center() { return {0.5f, 0.5f, 0.5f, 0.5f}; }

    /**
     * @brief Bottom-right anchors (1,1)-(1,1).
     * @return AnchorSet.
     */
    static AnchorSet BottomRight() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
};

/**
 * @brief Offset set: pixel offsets for the four edges.
 * @ingroup UI
 */
struct LEIR_API OffsetSet {
    float left = 0.0f;   ///< Left offset.
    float top = 0.0f;    ///< Top offset.
    float right = 0.0f;  ///< Right offset.
    float bottom = 0.0f; ///< Bottom offset.

    /**
     * @brief Creates offsets with same value on all sides.
     * @param[in] v Value for all sides.
     * @return OffsetSet.
     */
    static OffsetSet All(float v) { return {v, v, v, v}; }

    /**
     * @brief Creates symmetric offsets.
     * @param[in] h Horizontal offsets (left/right).
     * @param[in] v Vertical offsets (top/bottom).
     * @return OffsetSet.
     */
    static OffsetSet Symmetric(float h, float v) { return {h, v, h, v}; }
};

/**
 * @brief Anchor/offset rectangle for UI layout.
 * @ingroup UI
 * @details Combines normalized anchors and pixel offsets with a pivot to
 *  compute an absolute rectangle via GetRect(parentSize).
 */
struct LEIR_API Rect2D {
    AnchorSet anchor;            ///< Anchors.
    OffsetSet offset;            ///< Offsets.
    Vector2 pivot = {0.5f, 0.5f};///< Pivot in [0,1]×[0,1].

    /**
     * @brief Computes absolute rectangle from parent size.
     * @param[in] parentSize Parent size.
     * @return Absolute rect (x,y,w,h) in logical pixels.
     */
    Vector4 GetRect(const Vector2& parentSize) const {
        float x = anchor.left * parentSize.x + offset.left;
        float y = anchor.top * parentSize.y + offset.top;
        float w = (anchor.right - anchor.left) * parentSize.x + (offset.right - offset.left);
        float h = (anchor.bottom - anchor.top) * parentSize.y + (offset.bottom - offset.top);
        return {x, y, w, h};
    }

    /**
     * @brief Creates an absolute rectangle (TopLeft anchors).
     * @param[in] x X position.
     * @param[in] y Y position.
     * @param[in] w Width.
     * @param[in] h Height.
     * @param[in] pivot Pivot.
     * @return Rect2D.
     */
    static Rect2D Absolute(float x, float y, float w, float h, const Vector2& pivot = {0.5f, 0.5f}) {
        Rect2D r;
        r.anchor = AnchorSet::TopLeft();
        r.offset = {x, y, x + w, y + h};
        r.pivot = pivot;
        return r;
    }

    /**
     * @brief Creates a stretch rectangle with margins.
     * @param[in] margins Margins (offsets).
     * @param[in] pivot Pivot.
     * @return Rect2D.
     */
    static Rect2D Stretch(const OffsetSet& margins = {}, const Vector2& pivot = {0.5f, 0.5f}) {
        Rect2D r;
        r.anchor = AnchorSet::Stretch();
        r.offset = margins;
        r.pivot = pivot;
        return r;
    }
};

} // namespace Leir
