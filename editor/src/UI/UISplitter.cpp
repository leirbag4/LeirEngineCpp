#include "UISplitter.h"
#include <LeirEngine/UI/UICanvas.h>
#include <algorithm>
#include <spdlog/spdlog.h>

UISplitter::UISplitter()
{
    SetName("Splitter");
    SetColor({0.25f, 0.25f, 0.30f, 1.0f});
}

UISplitter::~UISplitter() = default;

bool UISplitter::OnPointerDown(const Leir::Vector2& pos)
{
    m_Dragging = true;
    m_DragStartX = pos.x;
    m_StartWidth = m_GetCurrent ? m_GetCurrent() : 0.0f;

    Leir::UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<Leir::UICanvas*>(e)) {
            spdlog::trace("[Splitter] capturing pointer for drag (startWidth={:.1f})", m_StartWidth);
            c->CapturePointer(this);
            break;
        }
        e = e->GetParent();
    }
    return true;
}

void UISplitter::OnPointerMove(const Leir::Vector2& pos)
{
    if (!m_Dragging) return;

    float newWidth = std::clamp(m_StartWidth + (pos.x - m_DragStartX), m_MinWidth, m_MaxWidth);
    spdlog::trace("[Splitter] dragMove: startX={:.1f} curX={:.1f} width={:.1f}", m_DragStartX, pos.x, newWidth);
    if (m_OnResize)
        m_OnResize(newWidth);
}

bool UISplitter::OnPointerUp(const Leir::Vector2& pos)
{
    if (!m_Dragging) return false;
    m_Dragging = false;
    spdlog::trace("[Splitter] dragEnd");
    if (m_OnDragEnd)
        m_OnDragEnd();
    Leir::InputManager::SetCursorStyle(Leir::CursorStyle::ResizeEW);
    return true;
}

void UISplitter::OnPointerEnter(const Leir::Vector2& pos)
{
    Leir::InputManager::SetCursorStyle(Leir::CursorStyle::ResizeEW);
}

void UISplitter::OnPointerExit()
{
    if (!m_Dragging)
        Leir::InputManager::SetCursorStyle(Leir::CursorStyle::Arrow);
}

Leir::Vector2 UISplitter::GetMinSize() const
{
    return {6.0f, 0.0f};
}
