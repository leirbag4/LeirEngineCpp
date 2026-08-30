#pragma once

/**
 * @file UIScrollbar.h
 * @brief Scrollbar widget: track + draggable thumb.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include <functional>

namespace Leir {

/**
 * @brief Vertical or horizontal scrollbar: track with draggable thumb.
 * @ingroup UI
 * @details The track is the widget's background; the thumb is a child UIPanel
 *  positioned proportionally to viewport/content. Supports drag with CapturePointer.
 */
class LEIR_API UIScrollbar : public UIPanel {
public:
    /**
     * @brief Constructs a scrollbar.
     * @param[in] vertical True for vertical, false for horizontal.
     */
    explicit UIScrollbar(bool vertical = true);

    /**
     * @brief Destroys the scrollbar and its thumb.
     */
    ~UIScrollbar() override;

    /**
     * @brief Returns whether the scrollbar owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if child is the thumb.
     */
    bool OwnsChild(const UIElement* child) const override { return child == m_Thumb; }

    /**
     * @brief Sets orientation.
     * @param[in] vertical True for vertical.
     */
    void SetVertical(bool vertical) { m_Vertical = vertical; }

    /**
     * @brief Returns orientation.
     * @return True if vertical.
     */
    bool IsVertical() const { return m_Vertical; }

    /**
     * @brief Sets range (viewport and content sizes along the scroll axis).
     * @details When content fits, thumb fills the track and value is clamped to 0.
     * @param[in] viewportSize Viewport size.
     * @param[in] contentSize Content size.
     */
    void SetRange(float viewportSize, float contentSize);

    /**
     * @brief Returns viewport size.
     * @return Viewport size.
     */
    float GetScrollViewport() const { return m_Viewport; }

    /**
     * @brief Returns content size.
     * @return Content size.
     */
    float GetScrollContent() const { return m_Content; }

    /**
     * @brief Sets normalized value [0,1] (0 = top/left, 1 = bottom/right).
     * @param[in] value Normalized value.
     */
    void SetValue(float value);

    /**
     * @brief Returns normalized value.
     * @return Value in [0,1].
     */
    float GetValue() const { return m_Value; }

    /**
     * @brief Sets scroll callback.
     * @param[in] cb Callback with normalized value.
     */
    void SetOnScroll(std::function<void(float)> cb) { m_OnScroll = cb; }

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Handles pointer press (track jump or thumb grab).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer release (ends drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Handles pointer move (drag).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

protected:
    /**
     * @brief Called after layout to position the thumb.
     */
    void OnLayoutComputed() override;

private:
    float TrackLen() const;
    float ThumbLen() const;
    float ThumbStart() const;
    void SetValueFromPos(const Vector2& pos);
    bool CaptureCanvasPointer();

    bool m_Vertical = true;                             ///< Vertical flag.
    float m_Viewport = 0.0f;                            ///< Viewport size.
    float m_Content = 0.0f;                             ///< Content size.
    float m_Value = 0.0f;                               ///< Normalized value [0,1].
    std::function<void(float)> m_OnScroll;              ///< Scroll callback.
    UIPanel* m_Thumb = nullptr;                         ///< Thumb panel (owned).
    bool m_Dragging = false;                            ///< Dragging flag.
    float m_DragGrabOffset = 0.0f;                      ///< Grab offset for thumb drag.
};

} // namespace Leir
