#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

// Clipped scroll container: the content is scissored to the ScrollView rect and
// may be scrolled via mouse wheel or drag. Vertical and horizontal UIScrollbars
// are managed internally (visible only when the content overflows).
class LEIR_API ScrollView : public UIElement {
public:
    ScrollView();
    ~ScrollView() override;

    // The ScrollView owns its viewport node and both scrollbars (its dtor
    // deletes them). The content under the viewport is NOT owned by the
    // ScrollView (the caller provides it via SetContent), so the DeleteUiSubtree
    // helpers must recurse into the viewport to reach editor-owned children but
    // must not delete the viewport/scrollbars themselves.
    bool OwnsChild(const UIElement* child) const override {
        return child == m_Viewport || child == m_VScrollbar || child == m_HScrollbar;
    }

    void SetContent(UIElement* content);
    UIElement* GetContent() const { return m_Content; }

    // Offset in pixels (positive = content moved up / left, showing later content).
    void SetScrollOffset(const Vector2& offset);
    const Vector2& GetScrollOffset() const { return m_ScrollOffset; }

    // Height of one scroll line used by the wheel (pixels per wheel notch).
    void SetLineHeight(float lineHeight) { m_LineHeight = lineHeight; }
    float GetLineHeight() const { return m_LineHeight; }

    void SetScrollbarWidth(float w) { m_ScrollbarWidth = w; }
    float GetScrollbarWidth() const { return m_ScrollbarWidth; }

    // Enable/disable each scrollbar. A disabled scrollbar is not shown and does
    // not reserve space in the viewport.
    void SetVerticalScrollbarEnabled(bool enabled) { m_VScrollbarEnabled = enabled; }
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }
    void SetHorizontalScrollbarEnabled(bool enabled) { m_HScrollbarEnabled = enabled; }
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    // Content size and visible viewport size (along both axes).
    Vector2 GetContentSize() const;
    Vector2 GetViewportSize() const;

    float GetMaxScrollY() const;
    float GetMaxScrollX() const;

    UIScrollbar* GetVerticalScrollbar() const { return m_VScrollbar; }
    UIScrollbar* GetHorizontalScrollbar() const { return m_HScrollbar; }

    bool OnScroll(float delta) override;
    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;

protected:
    void OnLayoutComputed() override;

private:
    void ApplyContentLayout(float layoutW, float layoutH);
    void SyncScrollbar();

    Vector2 m_ScrollOffset = {0.0f, 0.0f};
    UIElement* m_Viewport = nullptr;
    UIElement* m_Content = nullptr;
    UIScrollbar* m_VScrollbar = nullptr;
    UIScrollbar* m_HScrollbar = nullptr;
    bool m_Dragging = false;
    Vector2 m_DragStart = {0.0f, 0.0f};
    Vector2 m_ScrollStart = {0.0f, 0.0f};
    float m_LineHeight = 16.0f;
    float m_ScrollbarWidth = 10.0f;
    bool m_VScrollbarEnabled = true;
    bool m_HScrollbarEnabled = true;
};

} // namespace Leir
