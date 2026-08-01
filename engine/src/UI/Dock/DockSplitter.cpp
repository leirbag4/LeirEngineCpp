#include "LeirEngine/UI/Dock/DockSplitter.h"
#include "LeirEngine/UI/Dock/DockSplitNode.h"
#include "LeirEngine/UI/Dock/DockManager.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/InputManager.h"
#include <spdlog/spdlog.h>

namespace Leir {

DockSplitter::DockSplitter()
{
    SetName("DockSplitter");
    SetColor({0.22f, 0.22f, 0.26f, 1.0f});
}

DockSplitter::~DockSplitter() = default;

void DockSplitter::Configure(DockSplitNode* node, size_t index)
{
    m_Node = node;
    m_Index = index;
}

bool DockSplitter::OnPointerDown(const Vector2& pos)
{
    m_Dragging = true;
    m_DragStart = pos;

    UIElement* e = this;
    while (e) {
        if (auto* canvas = dynamic_cast<UICanvas*>(e)) {
            spdlog::trace("[DockSplitter] capturing pointer for drag");
            canvas->CapturePointer(this);
            break;
        }
        e = e->GetParent();
    }
    ApplyCursor();
    return true;
}

void DockSplitter::OnPointerMove(const Vector2& pos)
{
    if (!m_Dragging || !m_Node)
        return;

    float px = 0.0f;
    if (m_Node->GetOrientation() == DockOrientation::Horizontal)
        px = pos.x - m_DragStart.x;
    else
        px = pos.y - m_DragStart.y;

    m_Node->DragSplitter(m_Index, px);
}

bool DockSplitter::OnPointerUp(const Vector2& pos)
{
    (void)pos;
    if (!m_Dragging)
        return false;
    m_Dragging = false;
    ApplyCursor();

    // Persist after a drag ends
    UIElement* e = this;
    while (e) {
        if (auto* dm = dynamic_cast<DockManager*>(e)) {
            dm->NotifyLayoutChanged();
            break;
        }
        e = e->GetParent();
    }
    return true;
}

void DockSplitter::OnPointerEnter(const Vector2& pos)
{
    (void)pos;
    ApplyCursor();
}

void DockSplitter::OnPointerExit()
{
    if (!m_Dragging)
        InputManager::SetCursorStyle(CursorStyle::Arrow);
}

Vector2 DockSplitter::GetMinSize() const
{
    if (m_Node && m_Node->GetOrientation() == DockOrientation::Vertical)
        return {0.0f, 6.0f};
    return {6.0f, 0.0f};
}

void DockSplitter::ApplyCursor()
{
    if (!m_Node)
        return;
    if (m_Node->GetOrientation() == DockOrientation::Vertical)
        InputManager::SetCursorStyle(CursorStyle::ResizeNS);
    else
        InputManager::SetCursorStyle(CursorStyle::ResizeEW);
}

} // namespace Leir
