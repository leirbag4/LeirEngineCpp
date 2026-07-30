#include "LeirEngine/UI/UISlider.h"
#include <algorithm>

namespace Leir {

UISlider::UISlider() = default;
UISlider::~UISlider() = default;

void UISlider::SetValue(float value)
{
    m_Value = std::clamp(value, m_Min, m_Max);
}

Vector2 UISlider::GetMinSize() const
{
    return {100.0f, 20.0f};
}

float UISlider::HandlePos() const
{
    float range = m_Max - m_Min;
    if (range <= 0.0f) return 0.0f;
    return (m_Value - m_Min) / range;
}

float UISlider::ValueFromPos(float x) const
{
    const auto& cr = GetComputedRect();
    float t = (x - cr.x) / cr.z;
    t = std::clamp(t, 0.0f, 1.0f);
    return m_Min + t * (m_Max - m_Min);
}

void UISlider::OnPointerEnter(const Vector2& pos)
{
    m_Hovered = true;
}

void UISlider::OnPointerExit()
{
    m_Hovered = false;
}

bool UISlider::OnPointerDown(const Vector2& pos)
{
    m_Dragging = true;
    m_Value = ValueFromPos(pos.x);
    if (m_OnChange) m_OnChange(m_Value);
    return true;
}

bool UISlider::OnPointerUp(const Vector2& pos)
{
    m_Dragging = false;
    return true;
}

void UISlider::OnPointerMove(const Vector2& pos)
{
    if (m_Dragging) {
        m_Value = ValueFromPos(pos.x);
        if (m_OnChange) m_OnChange(m_Value);
    }
}

} // namespace Leir
