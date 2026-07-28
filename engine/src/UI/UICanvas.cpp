#include "LeirEngine/UI/UICanvas.h"
#include <spdlog/spdlog.h>

namespace Leir {

UICanvas::UICanvas()
{
    SetName("Canvas");
    m_Rect.anchor = AnchorSet::Stretch();
    m_Rect.offset = {};
}

UICanvas::~UICanvas() = default;

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

    // Check children first (front-to-back: reverse order)
    const auto& children = element->GetChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        HitTestRecursive(*it, pos, out);
        if (out) return;
    }

    if (inside)
        out = element;
}

void UICanvas::UpdatePointer(const glm::vec2& screenPos, bool pointerDown, bool pointerUp)
{
    UIElement* hit = nullptr;
    HitTest(screenPos, hit);

    // Hover tracking
    if (hit != m_HoveredElement) {
        if (m_HoveredElement) {
            m_HoveredElement->OnPointerExit();
            m_HoveredElement->SetHovered(false);
        }
        m_HoveredElement = hit;
        if (m_HoveredElement) {
            m_HoveredElement->SetHovered(true);
            m_HoveredElement->OnPointerEnter(screenPos);
        }
    }

    if (m_HoveredElement) {
        m_HoveredElement->OnPointerMove(screenPos);
    }

    if (pointerDown) {
        if (hit) {
            hit->OnPointerDown(screenPos);
            SetFocus(hit);
        } else {
            ClearFocus();
        }
    }

    if (pointerUp) {
        if (hit)
            hit->OnPointerUp(screenPos);
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
