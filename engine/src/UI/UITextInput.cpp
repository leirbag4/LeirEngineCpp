#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/Font.h"
#include <spdlog/spdlog.h>

namespace Leir {

UITextInput::UITextInput() = default;
UITextInput::~UITextInput() = default;

void UITextInput::SetText(const std::string& text)
{
    m_Text = text;
    m_CursorPos = (int)m_Text.size();
}

glm::vec2 UITextInput::GetMinSize() const
{
    float h = m_Font ? m_Font->GetLineHeight() : 20.0f;
    return {100.0f, h + 8.0f};
}

void UITextInput::OnPointerEnter(const glm::vec2& pos)
{
    m_Hovered = true;
}

void UITextInput::OnPointerExit()
{
    m_Hovered = false;
}

bool UITextInput::OnPointerDown(const glm::vec2& pos)
{
    spdlog::trace("[TextInput '{}'] OnPointerDown, setting focus", GetName().c_str());
    UpdateCursorPos();
    return true;
}

void UITextInput::OnPointerMove(const glm::vec2& pos)
{
    if (m_Focused)
        UpdateCursorPos();
}

bool UITextInput::OnTextInput(uint32_t codepoint)
{
    if (!m_Focused) return false;
    InsertChar(codepoint);
    return true;
}

void UITextInput::OnFocus()
{
    m_Focused = true;
}

void UITextInput::OnBlur()
{
    m_Focused = false;
}

void UITextInput::InsertChar(uint32_t codepoint)
{
    if (codepoint < 32 || codepoint > 126)
        return;

    std::string before = m_Text.substr(0, m_CursorPos);
    std::string after = m_Text.substr(m_CursorPos);
    m_Text = before + (char)codepoint + after;
    m_CursorPos++;

    if (m_OnChange)
        m_OnChange(m_Text);
}

void UITextInput::DeleteChar()
{
    if (m_CursorPos <= 0)
        return;
    std::string before = m_Text.substr(0, m_CursorPos - 1);
    std::string after = m_Text.substr(m_CursorPos);
    m_Text = before + after;
    m_CursorPos--;
    if (m_OnChange)
        m_OnChange(m_Text);
}

void UITextInput::UpdateCursorPos()
{
    // Approximate: set cursor to end
    m_CursorPos = (int)m_Text.size();
}

} // namespace Leir
