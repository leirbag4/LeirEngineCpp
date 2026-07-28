#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/Font.h"

namespace Leir {

UIButton::UIButton() = default;
UIButton::~UIButton() = default;

void UIButton::SetColors(const glm::vec4& normal, const glm::vec4& hover, const glm::vec4& pressed)
{
    m_BgNormal = normal;
    m_BgHover = hover;
    m_BgPressed = pressed;
}

glm::vec2 UIButton::GetMinSize() const
{
    if (!m_Font || m_Text.empty())
        return {100.0f, 32.0f};
    auto textSize = m_Font->MeasureText(m_Text, 0.0f);
    return {textSize.x + 16.0f, textSize.y + 10.0f};
}

void UIButton::OnPointerEnter(const glm::vec2& pos)
{
    m_State = ButtonState::Hovered;
}

void UIButton::OnPointerExit()
{
    m_State = ButtonState::Normal;
}

bool UIButton::OnPointerDown(const glm::vec2& pos)
{
    m_State = ButtonState::Pressed;
    return true;
}

bool UIButton::OnPointerUp(const glm::vec2& pos)
{
    if (m_State == ButtonState::Pressed) {
        m_State = ButtonState::Hovered;
        if (m_OnClick)
            m_OnClick();
        return true;
    }
    return false;
}

} // namespace Leir
