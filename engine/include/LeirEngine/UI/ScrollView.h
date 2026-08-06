#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

// Clipped scroll container: the content is scissored to the ScrollView rect and
// may be scrolled via mouse wheel or drag. A vertical UIScrollbar is managed
// internally (visible only when the content overflows).
class LEIR_API ScrollView : public UIElement {
public:
    ScrollView();
    ~ScrollView() override;

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

    // Content size and visible viewport size (along both axes).
    Vector2 GetContentSize() const;
    Vector2 GetViewportSize() const;

    float GetMaxScrollY() const;
    float GetMaxScrollX() const;

    UIScrollbar* GetVerticalScrollbar() const { return m_VScrollbar; }

    bool OnScroll(float delta) override;
    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;

protected:
    void OnLayoutComputed() override;

private:
    void ApplyContentLayout();
    void SyncScrollbar();

    Vector2 m_ScrollOffset = {0.0f, 0.0f};
    UIElement* m_Content = nullptr;
    UIScrollbar* m_VScrollbar = nullptr;
    bool m_Dragging = false;
    Vector2 m_DragStart = {0.0f, 0.0f};
    Vector2 m_ScrollStart = {0.0f, 0.0f};
    float m_LineHeight = 16.0f;
    float m_ScrollbarWidth = 10.0f;
};

} // namespace Leir
