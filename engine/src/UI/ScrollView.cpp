#include "LeirEngine/UI/ScrollView.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/Keyboard.h"
#include <algorithm>

namespace Leir {

ScrollView::ScrollView()
{
    SetClip(true);
    SetName("ScrollView");

    m_VScrollbar = new UIScrollbar(true);
    m_VScrollbar->SetName("ScrollViewScrollbar");
    AddChild(m_VScrollbar);
    m_VScrollbar->SetOnScroll([this](float v) {
        Vector2 off = m_ScrollOffset;
        off.y = v * GetMaxScrollY();
        SetScrollOffset(off);
    });

    m_HScrollbar = new UIScrollbar(false);
    m_HScrollbar->SetName("ScrollViewHScrollbar");
    AddChild(m_HScrollbar);
    m_HScrollbar->SetOnScroll([this](float v) {
        Vector2 off = m_ScrollOffset;
        off.x = v * GetMaxScrollX();
        SetScrollOffset(off);
    });
}

ScrollView::~ScrollView()
{
    if (m_VScrollbar) {
        RemoveChild(m_VScrollbar);
        delete m_VScrollbar;
    }
    if (m_HScrollbar) {
        RemoveChild(m_HScrollbar);
        delete m_HScrollbar;
    }
}

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
    // Keep the scrollbars as the topmost children so they are hit-tested and
    // rendered above the (tall) content rect — otherwise the content column
    // covers the horizontal bar's strip and presses on the thumb hit it
    // instead, dragging the content and inverting the thumb direction.
    if (m_VScrollbar) {
        RemoveChild(m_VScrollbar);
        AddChild(m_VScrollbar);
    }
    if (m_HScrollbar) {
        RemoveChild(m_HScrollbar);
        AddChild(m_HScrollbar);
    }
}

Vector2 ScrollView::GetContentSize() const
{
    if (m_Content)
        return m_Content->GetContentSize();
    return {0.0f, 0.0f};
}

Vector2 ScrollView::GetViewportSize() const
{
    const auto& cr = GetComputedRect();
    const float vw = m_VScrollbarEnabled ? m_ScrollbarWidth : 0.0f;
    const float hh = m_HScrollbarEnabled ? m_ScrollbarWidth : 0.0f;
    return {std::max(0.0f, cr.z - vw), std::max(0.0f, cr.w - hh)};
}

float ScrollView::GetMaxScrollY() const
{
    return GetContentSize().y - GetViewportSize().y;
}

float ScrollView::GetMaxScrollX() const
{
    return GetContentSize().x - GetViewportSize().x;
}

void ScrollView::SetScrollOffset(const Vector2& offset)
{
    m_ScrollOffset.x = std::clamp(offset.x, 0.0f, std::max(0.0f, GetMaxScrollX()));
    m_ScrollOffset.y = std::clamp(offset.y, 0.0f, std::max(0.0f, GetMaxScrollY()));
}

void ScrollView::OnLayoutComputed()
{
    const auto& cr = GetComputedRect();
    const float availW = std::max(1.0f, GetViewportSize().x);

    // Clamp to the current content size before positioning.
    m_ScrollOffset.x = std::clamp(m_ScrollOffset.x, 0.0f, std::max(0.0f, GetMaxScrollX()));
    m_ScrollOffset.y = std::clamp(m_ScrollOffset.y, 0.0f, std::max(0.0f, GetMaxScrollY()));

    // Content is laid out at absolute coords so descendants inherit real positions.
    if (m_Content) {
        m_Content->GetRect().anchor = AnchorSet::TopLeft();
        m_Content->GetRect().offset.left = cr.x - m_ScrollOffset.x;
        m_Content->GetRect().offset.top = cr.y - m_ScrollOffset.y;
        m_Content->GetRect().offset.right = cr.x - m_ScrollOffset.x + availW;
        m_Content->GetRect().offset.bottom = cr.y - m_ScrollOffset.y + 8192.0f;
        m_Content->ComputeLayout({availW, 8192.0f});
    }

    SyncScrollbar();

    // The scrollbar callback may have adjusted the offset; re-apply.
    if (m_Content) {
        m_Content->GetRect().offset.left = cr.x - m_ScrollOffset.x;
        m_Content->GetRect().offset.top = cr.y - m_ScrollOffset.y;
        m_Content->GetRect().offset.right = cr.x - m_ScrollOffset.x + availW;
        m_Content->GetRect().offset.bottom = cr.y - m_ScrollOffset.y + 8192.0f;
        m_Content->ComputeLayout({availW, 8192.0f});
    }
}

void ScrollView::SyncScrollbar()
{
    const auto& cr = GetComputedRect();
    const float viewportW = GetViewportSize().x;
    const float viewportH = GetViewportSize().y;
    const Vector2 content = GetContentSize();

    const bool vOverflow = m_VScrollbarEnabled && content.y > viewportH && viewportH > 1.0f;
    const bool hOverflow = m_HScrollbarEnabled && content.x > viewportW && viewportW > 1.0f;

    if (m_VScrollbar) {
        m_VScrollbar->SetActive(vOverflow);

        if (vOverflow) {
            m_VScrollbar->GetRect().anchor = AnchorSet::TopLeft();
            m_VScrollbar->GetRect().offset = {
                cr.x + cr.z - m_ScrollbarWidth - 2.0f, cr.y + 2.0f,
                cr.x + cr.z - 2.0f, cr.y + cr.w - 2.0f
            };
            m_VScrollbar->ComputeLayout({cr.z, cr.w});

            m_VScrollbar->SetRange(viewportH, content.y);
            const float maxY = GetMaxScrollY();
            if (maxY > 0.0f)
                m_VScrollbar->SetValue(m_ScrollOffset.y / maxY);
        }
    }

    if (m_HScrollbar) {
        m_HScrollbar->SetActive(hOverflow);

        if (hOverflow) {
            m_HScrollbar->GetRect().anchor = AnchorSet::TopLeft();
            m_HScrollbar->GetRect().offset = {
                cr.x + 2.0f, cr.y + cr.w - m_ScrollbarWidth - 2.0f,
                cr.x + cr.z - 2.0f, cr.y + cr.w - 2.0f
            };
            m_HScrollbar->ComputeLayout({cr.z, cr.w});

            m_HScrollbar->SetRange(viewportW, content.x);
            const float maxX = GetMaxScrollX();
            if (maxX > 0.0f)
                m_HScrollbar->SetValue(m_ScrollOffset.x / maxX);
        }
    }
}

bool ScrollView::OnScroll(float delta)
{
    if (GetMaxScrollY() <= 0.0f && GetMaxScrollX() <= 0.0f)
        return false;

    Vector2 off = m_ScrollOffset;
    const bool horizontal = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    if (horizontal && GetMaxScrollX() > 0.0f)
        off.x -= delta * m_LineHeight;
    else
        off.y -= delta * m_LineHeight;
    SetScrollOffset(off);
    return true;
}

bool ScrollView::OnPointerDown(const Vector2& pos)
{
    if (GetMaxScrollY() <= 0.0f && GetMaxScrollX() <= 0.0f)
        return false;

    m_Dragging = true;
    m_DragStart = pos;
    m_ScrollStart = m_ScrollOffset;
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) {
            c->CapturePointer(this);
            break;
        }
        e = e->GetParent();
    }
    return true;
}

bool ScrollView::OnPointerUp(const Vector2& pos)
{
    (void)pos;
    m_Dragging = false;
    return true;
}

void ScrollView::OnPointerMove(const Vector2& pos)
{
    if (m_Dragging) {
        Vector2 delta = pos - m_DragStart;
        Vector2 off = m_ScrollStart;
        off.x -= delta.x;
        off.y -= delta.y;
        SetScrollOffset(off);
    }
}

} // namespace Leir
