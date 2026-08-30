#pragma once

/**
 * @file ScrollView.h
 * @brief Clipped scroll container with viewport and scrollbars.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

/**
 * @brief Clipped scroll container: content scissored to the view rect with wheel/drag scrolling.
 * @ingroup UI
 * @details The content is scissored to the ScrollView rect and may be scrolled via
 *  mouse wheel or drag. Vertical and horizontal UIScrollbars are managed internally
 *  (visible only when content overflows).
 */
class LEIR_API ScrollView : public UIElement {
public:
    /**
     * @brief Constructs a scroll view with viewport and scrollbars.
     */
    ScrollView();

    /**
     * @brief Destroys the scroll view and its owned viewport/scrollbars.
     */
    ~ScrollView() override;

    /**
     * @brief Returns whether the view owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if child is viewport or a scrollbar.
     */
    bool OwnsChild(const UIElement* child) const override {
        return child == m_Viewport || child == m_VScrollbar || child == m_HScrollbar;
    }

    /**
     * @brief Sets the content element (not owned; caller provides it).
     * @param[in] content Content element.
     */
    void SetContent(UIElement* content);

    /**
     * @brief Returns the content element.
     * @return Content pointer or nullptr.
     */
    UIElement* GetContent() const { return m_Content; }

    /**
     * @brief Sets scroll offset (positive = content moved up/left).
     * @param[in] offset Offset in logical pixels.
     */
    void SetScrollOffset(const Vector2& offset);

    /**
     * @brief Returns scroll offset.
     * @return Offset.
     */
    const Vector2& GetScrollOffset() const { return m_ScrollOffset; }

    /**
     * @brief Sets line height for wheel scrolling.
     * @param[in] lineHeight Pixels per wheel notch.
     */
    void SetLineHeight(float lineHeight) { m_LineHeight = lineHeight; }

    /**
     * @brief Returns line height.
     * @return Line height.
     */
    float GetLineHeight() const { return m_LineHeight; }

    /**
     * @brief Sets scrollbar strip width.
     * @param[in] w Width in logical pixels.
     */
    void SetScrollbarWidth(float w) { m_ScrollbarWidth = w; }

    /**
     * @brief Returns scrollbar width.
     * @return Width.
     */
    float GetScrollbarWidth() const { return m_ScrollbarWidth; }

    /**
     * @brief Enables vertical scrollbar.
     * @param[in] enabled True to enable.
     */
    void SetVerticalScrollbarEnabled(bool enabled) { m_VScrollbarEnabled = enabled; }

    /**
     * @brief Returns whether vertical scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }

    /**
     * @brief Enables horizontal scrollbar.
     * @param[in] enabled True to enable.
     */
    void SetHorizontalScrollbarEnabled(bool enabled) { m_HScrollbarEnabled = enabled; }

    /**
     * @brief Returns whether horizontal scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    /**
     * @brief Returns content size (whole content block).
     * @return Content size.
     */
    Vector2 GetContentSize() const;

    /**
     * @brief Returns viewport size (visible area minus scrollbar strips).
     * @return Viewport size.
     */
    Vector2 GetViewportSize() const;

    /**
     * @brief Returns max vertical scroll.
     * @return Max Y scroll.
     */
    float GetMaxScrollY() const;

    /**
     * @brief Returns max horizontal scroll.
     * @return Max X scroll.
     */
    float GetMaxScrollX() const;

    /**
     * @brief Returns vertical scrollbar widget.
     * @return Scrollbar pointer.
     */
    UIScrollbar* GetVerticalScrollbar() const { return m_VScrollbar; }

    /**
     * @brief Returns horizontal scrollbar widget.
     * @return Scrollbar pointer.
     */
    UIScrollbar* GetHorizontalScrollbar() const { return m_HScrollbar; }

    /**
     * @brief Handles wheel scroll.
     * @param[in] delta Scroll delta.
     * @return True if consumed.
     */
    bool OnScroll(float delta) override;

    /**
     * @brief Handles pointer press (starts drag).
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
     * @brief Called after layout to sync content and scrollbars.
     */
    void OnLayoutComputed() override;

private:
    void ApplyContentLayout(float layoutW, float layoutH);
    void SyncScrollbar();

    Vector2 m_ScrollOffset = {0.0f, 0.0f};           ///< Scroll offset.
    UIElement* m_Viewport = nullptr;                ///< Viewport node (owned).
    UIElement* m_Content = nullptr;                 ///< Content (not owned).
    UIScrollbar* m_VScrollbar = nullptr;            ///< Vertical scrollbar (owned).
    UIScrollbar* m_HScrollbar = nullptr;            ///< Horizontal scrollbar (owned).
    bool m_Dragging = false;                        ///< Dragging flag.
    Vector2 m_DragStart = {0.0f, 0.0f};              ///< Drag start position.
    Vector2 m_ScrollStart = {0.0f, 0.0f};            ///< Scroll at drag start.
    float m_LineHeight = 16.0f;                     ///< Line height for wheel.
    float m_ScrollbarWidth = 10.0f;                 ///< Scrollbar strip width.
    bool m_VScrollbarEnabled = true;                ///< Vertical enabled.
    bool m_HScrollbarEnabled = true;                ///< Horizontal enabled.
};

} // namespace Leir
