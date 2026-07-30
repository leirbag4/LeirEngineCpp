#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/Keyboard.h"
#include <spdlog/spdlog.h>

namespace Leir {

UITextInput::UITextInput() = default;
UITextInput::~UITextInput() = default;

void UITextInput::SetText(const std::string& text)
{
    m_Text = text;
    m_CursorPos = (int)m_Text.size();
}

Vector2 UITextInput::GetMinSize() const
{
    float h = m_Font ? m_Font->GetLineHeight() : 20.0f;
    return {100.0f, h + 8.0f};
}

void UITextInput::OnPointerEnter(const Vector2& pos)
{
    m_Hovered = true;
}

void UITextInput::OnPointerExit()
{
    m_Hovered = false;
}

bool UITextInput::OnPointerDown(const Vector2& pos)
{
    spdlog::trace("[TextInput '{}'] OnPointerDown", GetName().c_str());
    ResetCaretBlink();
    if (m_Font) {
        const auto& cr = GetComputedRect();
        float localX = pos.x - (cr.x + 4.0f);
        int idx = GetCharIndexAtX(localX);
        if (idx >= 0)
            m_CursorPos = idx;
    } else {
        m_CursorPos = (int)m_Text.size();
    }
    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    if (shift) {
        if (m_SelectionStart < 0)
            m_SelectionStart = m_CursorPos;
    } else {
        ClearSelection();
    }
    m_Dragging = true;
    return true;
}

bool UITextInput::OnPointerUp(const Vector2& pos)
{
    m_Dragging = false;
    return true;
}

void UITextInput::OnPointerMove(const Vector2& pos)
{
    if (!m_Focused || !m_Font || !m_Dragging) return;

    if (m_SelectionStart < 0)
        m_SelectionStart = m_CursorPos;

    const auto& cr = GetComputedRect();
    float localX = pos.x - (cr.x + 4.0f);
    int idx = GetCharIndexAtX(localX);
    if (idx >= 0)
        m_CursorPos = idx;
}

bool UITextInput::OnKeyDown(int key)
{
    if (!m_Focused) return false;

    ResetCaretBlink();

    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);

    if (key == static_cast<int>(Key::Backspace)) {
        if (HasSelection()) {
            DeleteSelection();
        } else {
            DeleteChar();
        }
        return true;
    }
    if (key == static_cast<int>(Key::Delete)) {
        if (HasSelection()) {
            DeleteSelection();
        } else {
            DeleteForward();
        }
        return true;
    }
    if (key == static_cast<int>(Key::Left)) {
        if (!shift) ClearSelection();
        else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
        if (m_CursorPos > 0)
            m_CursorPos--;
        return true;
    }
    if (key == static_cast<int>(Key::Right)) {
        if (!shift) ClearSelection();
        else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
        if (m_CursorPos < (int)m_Text.size())
            m_CursorPos++;
        return true;
    }
    if (key == static_cast<int>(Key::Home)) {
        if (!shift) ClearSelection();
        else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
        m_CursorPos = 0;
        return true;
    }
    if (key == static_cast<int>(Key::End)) {
        if (!shift) ClearSelection();
        else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
        m_CursorPos = (int)m_Text.size();
        return true;
    }

    return false;
}

bool UITextInput::OnTextInput(uint32_t codepoint)
{
    if (!m_Focused) return false;
    ResetCaretBlink();
    if (HasSelection())
        DeleteSelection();
    InsertChar(codepoint);
    return true;
}

void UITextInput::DeleteSelection()
{
    if (!HasSelection()) return;
    int b = GetSelBegin();
    int e = GetSelEnd();
    m_Text.erase(b, e - b);
    m_CursorPos = b;
    ClearSelection();
    if (m_OnChange)
        m_OnChange(m_Text);
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

    m_Text.insert(m_CursorPos, 1, (char)codepoint);
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

void UITextInput::DeleteForward()
{
    if (m_CursorPos >= (int)m_Text.size())
        return;
    std::string before = m_Text.substr(0, m_CursorPos);
    std::string after = m_Text.substr(m_CursorPos + 1);
    m_Text = before + after;
    if (m_OnChange)
        m_OnChange(m_Text);
}

float UITextInput::GetCursorX() const
{
    return GetCursorXAt(m_CursorPos);
}

float UITextInput::GetCursorXAt(int charIndex) const
{
    if (!m_Font) return 0.0f;
    float x = 0.0f;
    for (int i = 0; i < charIndex && i < (int)m_Text.size();) {
        uint32_t cp = (unsigned char)m_Text[i];
        if (cp < 0x80) { ++i; }
        else if ((cp & 0xE0) == 0xC0 && i + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[i+1] & 0x3F); i += 2; }
        else if ((cp & 0xF0) == 0xE0 && i + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[i+1] & 0x3F) << 6) | (m_Text[i+2] & 0x3F); i += 3; }
        else { ++i; continue; }
        auto& g = m_Font->GetGlyphInfo(cp);
        x += g.advance;
    }
    return x;
}

int UITextInput::GetCharIndexAtX(float localX) const
{
    if (!m_Font || localX <= 0) return 0;
    float x = 0.0f;
    int charIdx = 0;
    for (int i = 0; i < (int)m_Text.size();) {
        uint32_t cp = (unsigned char)m_Text[i];
        int step = 1;
        if (cp < 0x80) { step = 1; }
        else if ((cp & 0xE0) == 0xC0 && i + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[i+1] & 0x3F); step = 2; }
        else if ((cp & 0xF0) == 0xE0 && i + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[i+1] & 0x3F) << 6) | (m_Text[i+2] & 0x3F); step = 3; }
        else { ++i; ++charIdx; continue; }

        auto& g = m_Font->GetGlyphInfo(cp);
        float nextX = x + g.advance;
        if (localX < nextX) {
            // Return the closer edge
            return (localX - x < nextX - localX) ? charIdx : charIdx + 1;
        }
        x = nextX;
        i += step;
        ++charIdx;
    }
    return (int)m_Text.size();
}

void UITextInput::UpdateCursorPos()
{
    m_CursorPos = (int)m_Text.size();
}

} // namespace Leir
