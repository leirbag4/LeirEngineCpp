#include "LeirEngine/UI/UIFloatInput.h"
#include <cstdlib>
#include <cctype>
#include <spdlog/spdlog.h>

namespace Leir {

UIFloatInput::UIFloatInput()
{
    SetOnTextChanged([](const std::string& t) -> bool {
        int dots = 0;
        for (int i = 0; i < (int)t.size(); ++i) {
            char c = t[i];
            if (c == '.') {
                if (++dots > 1) return false;
            } else if (c == '-' || c == '+') {
                if (i != 0) return false;
            }
        }
        return true;
    });
}
UIFloatInput::~UIFloatInput() = default;

void UIFloatInput::SetValue(float v)
{
    m_Value = v;
    if (!m_Focused)
        SetText(std::to_string(v));
}

bool UIFloatInput::OnTextInput(uint32_t codepoint)
{
    if (!m_Focused) {
        spdlog::trace("[FloatInput] OnTextInput ignored (not focused)");
        return false;
    }

    char c = (char)codepoint;
    if (std::isdigit(c) || c == '.' || c == '-' || c == '+') {
        spdlog::trace("[FloatInput] Accept char '{}' text='{}'",
            c, GetText().c_str());
        ResetCaretBlink();
        if (HasSelection())
            DeleteSelection();
        InsertChar(codepoint);
        return true;
    }
    spdlog::trace("[FloatInput] Reject char '{}' (codepoint={})", c, codepoint);
    return false;
}

bool UIFloatInput::OnKeyDown(int key)
{
    if (!m_Focused) {
        spdlog::trace("[FloatInput] OnKeyDown ignored (not focused)");
        return false;
    }

    if (key == 257 || key == 335) {
        spdlog::trace("[FloatInput] Enter key -> CommitValue");
        CommitValue();
        return true;
    }

    spdlog::trace("[FloatInput] KeyDown key={}, forwarding to base", key);
    return UITextInput::OnKeyDown(key);
}

void UIFloatInput::OnFocus()
{
    spdlog::trace("[FloatInput] OnFocus");
    UITextInput::OnFocus();
}

void UIFloatInput::OnBlur()
{
    spdlog::trace("[FloatInput] OnBlur (was focused={})", m_Focused);
    if (m_Focused) {
        CommitValue();
    }
    UITextInput::OnBlur();
}

void UIFloatInput::OnTextMutated()
{
    const std::string& text = GetText();
    if (text.empty())
        return;

    char* end = nullptr;
    float val = std::strtof(text.c_str(), &end);
    if (end == text.c_str())
        return;

    m_Value = val;
    if (m_OnValueChanged)
        m_OnValueChanged(val);
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
