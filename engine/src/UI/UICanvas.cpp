#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/EventQueue.h"
#include <spdlog/spdlog.h>

namespace Leir {

UICanvas::UICanvas()
{
    SetName("Canvas");
    m_Rect.anchor = AnchorSet::Stretch();
    m_Rect.offset = {};
}

UICanvas::~UICanvas()
{
    DisconnectFromInputSystem();
}

void UICanvas::ConnectToInputSystem()
{
    auto& eq = EventQueue::Get();

    eq.SetPointerHook([this](const PointerEvent& e) {
        ProcessPointerEvent(e);
    });

    eq.SetCharHook([this](const CharEvent& e) {
        SendTextInput(e.codepoint);
    });

    eq.SetKeyHook([this](const KeyEvent& e) {
        if (e.action == EventAction::Press || e.action == EventAction::Repeat)
            SendKeyDown(static_cast<int>(e.key));
    });
}

void UICanvas::DisconnectFromInputSystem()
{
    EventQueue::Get().ClearHooks();
}

void UICanvas::SetScreenSize(float width, float height)
{
    m_ScreenWidth = width;
    m_ScreenHeight = height;
}

void UICanvas::UpdateLayout()
{
    ComputeLayout({m_ScreenWidth, m_ScreenHeight});
}

bool UICanvas::HitTest(const glm::vec2& screenPos, UIElement*& outElement)
{
    outElement = nullptr;
    HitTestRecursive(this, screenPos, outElement);
    return outElement != nullptr;
}

void UICanvas::HitTestRecursive(UIElement* element, const glm::vec2& pos, UIElement*& out)
{
    if (!element->IsActive())
        return;

    const auto& r = element->GetComputedRect();
    bool inside = pos.x >= r.x && pos.x <= r.x + r.z &&
                  pos.y >= r.y && pos.y <= r.y + r.w;

    const auto& children = element->GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        HitTestRecursive(*it, pos, out);
        if (out) return;
    }

    if (inside)
        out = element;
}

void UICanvas::ProcessPointerEvent(const PointerEvent& e)
{
    glm::vec2 pos = e.position;

    UIElement* hit = nullptr;
    HitTest(pos, hit);

    // Hover tracking
    if (hit != m_HoveredElement) {
        if (m_HoveredElement) {
            m_HoveredElement->OnPointerExit();
            m_HoveredElement->SetHovered(false);
        }
        m_HoveredElement = hit;
        if (m_HoveredElement) {
            m_HoveredElement->SetHovered(true);
            m_HoveredElement->OnPointerEnter(pos);
        }
    }

    if (m_HoveredElement) {
        m_HoveredElement->OnPointerMove(pos);
    }

    if (e.action == EventAction::Press) {
        m_PointerDown = true;
        if (hit) {
            hit->OnPointerDown(pos);
            SetFocus(hit);
        } else {
            ClearFocus();
        }
    }

    if (e.action == EventAction::Release) {
        m_PointerDown = false;
        if (hit)
            hit->OnPointerUp(pos);
    }
}

void UICanvas::SetFocus(UIElement* element)
{
    if (m_FocusElement == element) return;
    if (m_FocusElement)
        m_FocusElement->OnBlur();
    m_FocusElement = element;
    if (m_FocusElement)
        m_FocusElement->OnFocus();
}

void UICanvas::SendTextInput(uint32_t codepoint)
{
    if (m_FocusElement)
        m_FocusElement->OnTextInput(codepoint);
}

void UICanvas::SendKeyDown(int key)
{
    if (m_FocusElement)
        m_FocusElement->OnKeyDown(key);
}

} // namespace Leir
