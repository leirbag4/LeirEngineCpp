#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/Input/InputManager.h>
#include <functional>

// Thin vertical divider bar used to resize editor panels by dragging.
// The owner supplies the current panel width and a resize callback; the
// splitter computes newWidth = clamp(startWidth + dx, min, max) while dragging.
class UISplitter : public Leir::UIPanel {
public:
    UISplitter();
    ~UISplitter() override;

    void SetMinWidth(float w) { m_MinWidth = w; }
    void SetMaxWidth(float w) { m_MaxWidth = w; }
    void SetCurrentWidthGetter(std::function<float()> getter) { m_GetCurrent = std::move(getter); }
    void SetOnResize(std::function<void(float)> cb) { m_OnResize = std::move(cb); }
    void SetOnDragEnd(std::function<void()> cb) { m_OnDragEnd = std::move(cb); }
    void SetDragInverted(bool inv) { m_InvertDrag = inv; }

    bool OnPointerDown(const Leir::Vector2& pos) override;
    void OnPointerMove(const Leir::Vector2& pos) override;
    bool OnPointerUp(const Leir::Vector2& pos) override;
    void OnPointerEnter(const Leir::Vector2& pos) override;
    void OnPointerExit() override;

    Leir::Vector2 GetMinSize() const override;

private:
    bool m_Dragging = false;
    bool m_InvertDrag = false;
    float m_DragStartX = 0.0f;
    float m_StartWidth = 0.0f;
    float m_MinWidth = 140.0f;
    float m_MaxWidth = 600.0f;
    std::function<float()> m_GetCurrent;
    std::function<void(float)> m_OnResize;
    std::function<void()> m_OnDragEnd;
};
