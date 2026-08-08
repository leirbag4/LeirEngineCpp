#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include <functional>

namespace Leir {

// Vertical or horizontal scrollbar. The track is the widget's own background;
// the thumb is a child UIPanel positioned proportionally to viewport/content.
class LEIR_API UIScrollbar : public UIPanel {
public:
    explicit UIScrollbar(bool vertical = true);
    ~UIScrollbar() override;

    bool OwnsChild(const UIElement* child) const override { return child == m_Thumb; }

    void SetVertical(bool vertical) { m_Vertical = vertical; }
    bool IsVertical() const { return m_Vertical; }

    // viewport/content are in pixels along the scroll axis. When content fits,
    // the thumb fills the track and SetValue is clamped to 0.
    void SetRange(float viewportSize, float contentSize);
    float GetScrollViewport() const { return m_Viewport; }
    float GetScrollContent() const { return m_Content; }

    // Value in [0,1] (0 = top/left, 1 = bottom/right).
    void SetValue(float value);
    float GetValue() const { return m_Value; }

    void SetOnScroll(std::function<void(float)> cb) { m_OnScroll = cb; }

    Vector2 GetMinSize() const override;

    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;

protected:
    void OnLayoutComputed() override;

private:
    float TrackLen() const;
    float ThumbLen() const;
    float ThumbStart() const;
    void SetValueFromPos(const Vector2& pos);
    bool CaptureCanvasPointer();

    bool m_Vertical = true;
    float m_Viewport = 0.0f;
    float m_Content = 0.0f;
    float m_Value = 0.0f;
    std::function<void(float)> m_OnScroll;
    UIPanel* m_Thumb = nullptr;
    bool m_Dragging = false;
    float m_DragGrabOffset = 0.0f;
};

} // namespace Leir
