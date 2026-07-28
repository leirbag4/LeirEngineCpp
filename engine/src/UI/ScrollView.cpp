#include "LeirEngine/UI/ScrollView.h"

namespace Leir {

ScrollView::ScrollView() = default;
ScrollView::~ScrollView() = default;

void ScrollView::SetContent(UIElement* content)
{
    if (m_Content) {
        RemoveChild(m_Content);
        m_Content = nullptr;
    }
    m_Content = content;
    if (content) {
        AddChild(content);
        content->GetRect().anchor = AnchorSet::TopLeft();
    }
}

void ScrollView::OnLayoutComputed()
{
    if (m_Content) {
        m_Content->GetRect().offset.left = m_ScrollOffset.x;
        m_Content->GetRect().offset.top = m_ScrollOffset.y;
        // Give content the full available space (it will ComputeLayout with scroll offsets)
        const auto& cr = GetComputedRect();
        glm::vec2 avail = {cr.z, cr.w};
        m_Content->ComputeLayout(avail);
    }

    for (auto* child : GetChildren()) {
        if (child != m_Content)
            child->ComputeLayout({GetComputedRect().z, GetComputedRect().w});
    }
}

bool ScrollView::OnPointerDown(const glm::vec2& pos)
{
    m_Dragging = true;
    m_DragStart = pos;
    m_ScrollStart = m_ScrollOffset;
    return true;
}

bool ScrollView::OnPointerUp(const glm::vec2& pos)
{
    m_Dragging = false;
    return true;
}

void ScrollView::OnPointerMove(const glm::vec2& pos)
{
    if (m_Dragging) {
        glm::vec2 delta = pos - m_DragStart;
        m_ScrollOffset = m_ScrollStart + delta;
    }
}

} // namespace Leir
