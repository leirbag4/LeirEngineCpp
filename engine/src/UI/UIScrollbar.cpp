#include "LeirEngine/UI/UIScrollbar.h"
#include "LeirEngine/UI/UICanvas.h"
#include <algorithm>
#include <cmath>

namespace Leir {

UIScrollbar::UIScrollbar(bool vertical)
    : m_Vertical(vertical)
{
    SetColor({0.12f, 0.12f, 0.14f, 1.0f});

    m_Thumb = new UIPanel();
    m_Thumb->SetName("ScrollbarThumb");
    m_Thumb->SetColor({0.42f, 0.42f, 0.46f, 1.0f});
    AddChild(m_Thumb);
}

UIScrollbar::~UIScrollbar()
{
    if (m_Thumb) {
        RemoveChild(m_Thumb);
        delete m_Thumb;
    }
}

Vector2 UIScrollbar::GetMinSize() const
{
    return m_Vertical ? Vector2{10.0f, 40.0f} : Vector2{40.0f, 10.0f};
}

void UIScrollbar::SetRange(float viewportSize, float contentSize)
{
    m_Viewport = std::max(0.0f, viewportSize);
    m_Content = std::max(0.0f, contentSize);
    if (m_Content <= m_Viewport)
        SetValue(0.0f);
    else
        SetValue(m_Value);
}

void UIScrollbar::SetValue(float value)
{
    float v = std::clamp(value, 0.0f, 1.0f);
    if (m_Content <= m_Viewport)
        v = 0.0f;
    if (v == m_Value)
        return;
    m_Value = v;
    if (m_OnScroll)
        m_OnScroll(m_Value);
}

float UIScrollbar::TrackLen() const
{
    const auto& cr = GetComputedRect();
    return m_Vertical ? cr.w : cr.z;
}

float UIScrollbar::ThumbLen() const
{
    const float track = TrackLen();
    if (track <= 0.0f)
        return 0.0f;
    if (m_Content <= m_Viewport || m_Viewport <= 0.0f)
        return track;
    const float len = track * (m_Viewport / m_Content);
    return std::clamp(len, 16.0f, track);
}

float UIScrollbar::ThumbStart() const
{
    const float track = TrackLen();
    const float len = ThumbLen();
    return m_Value * std::max(0.0f, track - len);
}

void UIScrollbar::OnLayoutComputed()
{
    if (!m_Thumb)
        return;

    const auto& cr = GetComputedRect();
    const float inset = 2.0f;
    const float len = ThumbLen();
    const float start = ThumbStart();

    Rect2D& tr = m_Thumb->GetRect();
    tr.anchor = AnchorSet::TopLeft();
    // Snap to integer pixels so the thumb thickness (grosor - 2*inset) and its
    // symmetric inset margins are never degraded by fractional edge rows.
    if (m_Vertical) {
        tr.offset.left = std::round(cr.x + inset);
        tr.offset.top = std::round(cr.y + start);
        tr.offset.right = std::round(cr.x + cr.z - inset);
        tr.offset.bottom = std::round(cr.y + start + len);
    } else {
        tr.offset.left = std::round(cr.x + start);
        tr.offset.top = std::round(cr.y + inset);
        tr.offset.right = std::round(cr.x + start + len);
        tr.offset.bottom = std::round(cr.y + cr.w - inset);
    }
    m_Thumb->ComputeLayout({cr.z, cr.w});
}

bool UIScrollbar::OnPointerDown(const Vector2& pos)
{
    if (m_Content <= m_Viewport)
        return false;

    m_Dragging = true;
    CaptureCanvasPointer();

    const auto& cr = GetComputedRect();
    const float inset = 2.0f;
    const float len = ThumbLen();
    const float start = ThumbStart();

    if (m_Vertical) {
        const float thumbTop = cr.y + start;
        const bool onThumb = pos.y >= thumbTop && pos.y <= thumbTop + len;
        m_DragGrabOffset = onThumb ? (pos.y - thumbTop) : (len * 0.5f);
    } else {
        const float thumbLeft = cr.x + start;
        const bool onThumb = pos.x >= thumbLeft && pos.x <= thumbLeft + len;
        m_DragGrabOffset = onThumb ? (pos.x - thumbLeft) : (len * 0.5f);
    }

    SetValueFromPos(pos);
    return true;
}

bool UIScrollbar::OnPointerUp(const Vector2& pos)
{
    (void)pos;
    m_Dragging = false;
    return true;
}

void UIScrollbar::OnPointerMove(const Vector2& pos)
{
    if (m_Dragging)
        SetValueFromPos(pos);
}

void UIScrollbar::SetValueFromPos(const Vector2& pos)
{
    const auto& cr = GetComputedRect();
    const float track = TrackLen();
    const float len = ThumbLen();
    if (track <= len)
        return;

    float t;
    if (m_Vertical)
        t = (pos.y - cr.y - m_DragGrabOffset) / (track - len);
    else
        t = (pos.x - cr.x - m_DragGrabOffset) / (track - len);

    SetValue(t);
}

bool UIScrollbar::CaptureCanvasPointer()
{
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) {
            c->CapturePointer(this);
            return true;
        }
        e = e->GetParent();
    }
    return false;
}

} // namespace Leir
