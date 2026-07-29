#include "LeirEngine/UI/UIFloatInput.h"
#include <cstdlib>
#include <cctype>

namespace Leir {

UIFloatInput::UIFloatInput() = default;
UIFloatInput::~UIFloatInput() = default;

void UIFloatInput::SetValue(float v)
{
    m_Value = v;
    SetText(std::to_string(v));
}

bool UIFloatInput::OnTextInput(uint32_t codepoint)
{
    if (!m_Focused) return false;

    char c = (char)codepoint;
    if (std::isdigit(c) || c == '.' || c == '-' || c == '+') {
        InsertChar(codepoint);
        return true;
    }
    return false;
}

bool UIFloatInput::OnKeyDown(int key)
{
    if (!m_Focused) return false;

    if (key == 257 || key == 335) {
        CommitValue();
        return true;
    }

    return false;
}

void UIFloatInput::OnFocus()
{
    m_Focused = true;
    UITextInput::OnFocus();
}

void UIFloatInput::OnBlur()
{
    if (m_Focused) {
        m_Focused = false;
        CommitValue();
    }
    UITextInput::OnBlur();
}

void UIFloatInput::CommitValue()
{
    const std::string& text = GetText();
    if (text.empty()) {
        m_Value = 0.0f;
    } else {
        char* end = nullptr;
        float val = std::strtof(text.c_str(), &end);
        if (end && *end == '\0')
            m_Value = val;
    }
    SetText(std::to_string(m_Value));
    if (m_OnValueChanged)
        m_OnValueChanged(m_Value);
}

} // namespace Leir
