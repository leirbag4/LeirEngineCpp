#include "LeirEngine/UI/ScrollView.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Input/Keyboard.h"
#include <algorithm>
#include <cmath>

namespace Leir {

ScrollView::ScrollView()
{
    SetClip(true);
    SetName("ScrollView");

    // Internal viewport node: clips the content to the usable area. The content
    // becomes its child, while the scrollbars remain children of the ScrollView
    // (siblings of the viewport) so they render in their own strip and never
    // overlap/clip the text (Model A — like Unity ScrollRect / scrollbar-gutter).
    m_Viewport = new UIElement();
    m_Viewport->SetClip(true);
    m_Viewport->SetName("ScrollViewViewport");
    AddChild(m_Viewport);

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
    if (m_Viewport) {
        RemoveChild(m_Viewport);
        delete m_Viewport;
        m_Viewport = nullptr;
    }
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
        m_Viewport->RemoveChild(m_Content);
        m_Content = nullptr;
    }
    m_Content = content;
    if (content) {
        m_Viewport->AddChild(content);
        content->GetRect().anchor = AnchorSet::TopLeft();
    }
    // Keep the scrollbars as the topmost children so they are hit-tested and
    // rendered above the content — the content lives inside the clipped viewport
    // (a sibling of the scrollbars), so it can never press on the thumb / cover
    // the bar strip.
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
    const float availH = std::max(1.0f, GetViewportSize().y);
    const float layoutW = std::max(availW, GetContentSize().x);
    const float layoutH = std::max(availH, std::max(GetContentSize().y, 1.0f));

    // Clamp to the current content size before positioning.
    m_ScrollOffset.x = std::clamp(m_ScrollOffset.x, 0.0f, std::max(0.0f, GetMaxScrollX()));
    m_ScrollOffset.y = std::clamp(m_ScrollOffset.y, 0.0f, std::max(0.0f, GetMaxScrollY()));

    // The viewport is a Free child of the ScrollView; position it over the usable
    // area (RELATIVE to the ScrollView + parentOffset={cr.xy} so m_ComputedRect
    // lands absolute; the ScrollView's own Free pass passes the same offset — no
    // transient double-counting) and refresh its computed rect so its clip region
    // is exactly the content surface (never reaching the scrollbar strips).
    // Snapped to integer pixels so the scissor doesn't drop partial edge rows.
    m_Viewport->GetRect().anchor = AnchorSet::TopLeft();
    m_Viewport->GetRect().offset = {
        std::round(cr.x) - cr.x, std::round(cr.y) - cr.y,
        std::round(cr.x + availW) - cr.x, std::round(cr.y + availH) - cr.y
    };
    m_Viewport->ComputeLayout({availW, availH}, {cr.x, cr.y});

    ApplyContentLayout(layoutW, layoutH);

    SyncScrollbar();

    // The scrollbar callback may have adjusted the offset; re-apply (idempotent).
    ApplyContentLayout(layoutW, layoutH);
}

void ScrollView::ApplyContentLayout(float layoutW, float layoutH)
{
    if (!m_Viewport || !m_Content)
        return;
    // Content is a child of the viewport: positioned RELATIVE to it (scrolled by
    // m_ScrollOffset) + parentOffset={vp.xy} so it inherits the real global
    // position and stays inside the viewport's clip region (no transient double
    // from the viewport's own Free pass).
    const auto& vp = m_Viewport->GetComputedRect();
    m_Content->GetRect().anchor = AnchorSet::TopLeft();
    m_Content->GetRect().offset.left = -m_ScrollOffset.x;
    m_Content->GetRect().offset.top = -m_ScrollOffset.y;
    m_Content->GetRect().offset.right = -m_ScrollOffset.x + layoutW;
    m_Content->GetRect().offset.bottom = -m_ScrollOffset.y + layoutH;
    m_Content->ComputeLayout({layoutW, layoutH}, {vp.x, vp.y});
}

void ScrollView::SyncScrollbar()
{
    const auto& cr = GetComputedRect();
    const float viewportW = GetViewportSize().x;
    const float viewportH = GetViewportSize().y;
    const Vector2 content = GetContentSize();
    const float rightEdge = std::round(cr.x + cr.z);
    const float bottomEdge = std::round(cr.y + cr.w);

    const bool vOverflow = m_VScrollbarEnabled && content.y > viewportH && viewportH > 1.0f;
    const bool hOverflow = m_HScrollbarEnabled && content.x > viewportW && viewportW > 1.0f;

    if (m_VScrollbar) {
        m_VScrollbar->SetActive(vOverflow);

        if (vOverflow) {
            // Flush track: starts exactly at the viewport's right edge so the
            // clip (W - scrollbarWidth) never overlaps with the bar. Edges are
            // pixel-snapped and the thickness kept exact so partial rows are
            // never dropped by rasterization. Positioned RELATIVE to the
            // ScrollView + parentOffset={cr.xy} (no transient double).
            m_VScrollbar->GetRect().anchor = AnchorSet::TopLeft();
            m_VScrollbar->GetRect().offset = {
                rightEdge - m_ScrollbarWidth - cr.x, std::round(cr.y) - cr.y,
                rightEdge - cr.x, (hOverflow ? (bottomEdge - m_ScrollbarWidth) : bottomEdge) - cr.y
            };
            m_VScrollbar->ComputeLayout({cr.z, cr.w}, {cr.x, cr.y});

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
                std::round(cr.x) - cr.x, bottomEdge - m_ScrollbarWidth - cr.y,
                (vOverflow ? (rightEdge - m_ScrollbarWidth) : rightEdge) - cr.x,
                bottomEdge - cr.y
            };
            m_HScrollbar->ComputeLayout({cr.z, cr.w}, {cr.x, cr.y});

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
