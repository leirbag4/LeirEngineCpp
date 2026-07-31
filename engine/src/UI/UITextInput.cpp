#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/Keyboard.h"
#include <cstdlib>
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
    spdlog::trace("[TextInput '{}'] OnPointerDown frame={}", GetName().c_str(), m_FrameCounter);
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

    // Double-click detection: within ~15 frames (~250ms) at nearly the same position
    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    int framesSinceLast = m_FrameCounter - m_LastClickFrame;
    int posDiff = abs(m_LastClickPos - m_CursorPos);
    bool doubleClick = !shift && m_LastClickPos >= 0 && framesSinceLast < 15 && posDiff <= 3;
    m_LastClickFrame = m_FrameCounter;
    m_LastClickPos = m_CursorPos;

    if (doubleClick) {
        spdlog::trace("[TextInput '{}'] Double-click detected (frames={} posDiff={})", GetName().c_str(), framesSinceLast, posDiff);
        SelectWordAt(m_CursorPos);
        m_Dragging = false;
        return true;
    }
    if (shift) {
        if (m_SelectionStart < 0)
            m_SelectionStart = m_CursorPos;
    } else {
        ClearSelection();
    }
    m_Dragging = true;
    CaptureDragPointer();
    return true;
}

bool UITextInput::OnPointerUp(const Vector2& pos)
{
    if (!m_Dragging) return false;
    m_Dragging = false;
    return true;
}

void UITextInput::CaptureDragPointer()
{
    UIElement* e = this;
    while (e) {
        auto* c = dynamic_cast<UICanvas*>(e);
        if (c) { c->CapturePointer(this); return; }
        e = e->GetParent();
    }
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

    bool ctrl = Keyboard::IsDown(Key::LeftControl) || Keyboard::IsDown(Key::RightControl);
    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);

    if (ctrl && key == static_cast<int>(Key::A)) {
        m_CursorPos = (int)m_Text.size();
        m_SelectionStart = 0;
        return true;
    }

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
        if (ctrl) {
            int b = FindPrevWordBoundary(m_CursorPos);
            if (shift && m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            if (!shift) ClearSelection();
            m_CursorPos = b;
        } else {
            if (!shift) ClearSelection();
            else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            if (m_CursorPos > 0)
                m_CursorPos--;
        }
        return true;
    }
    if (key == static_cast<int>(Key::Right)) {
        if (ctrl) {
            int b = FindNextWordBoundary(m_CursorPos);
            if (shift && m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            if (!shift) ClearSelection();
            m_CursorPos = b;
        } else {
            if (!shift) ClearSelection();
            else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            if (m_CursorPos < (int)m_Text.size())
                m_CursorPos++;
        }
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
    m_Dragging = false;
    UIElement* e = this;
    while (e) {
        auto* c = dynamic_cast<UICanvas*>(e);
        if (c) {
            if (c->GetCaptureElement() == this)
                c->ReleasePointer();
            break;
        }
        e = e->GetParent();
    }
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
        if (cp == '\n') { x = 0.0f; continue; }
        if (cp == ' ') { x += m_Font->GetSpaceWidth(); continue; }
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

        if (cp == '\n') { x = 0.0f; i += step; ++charIdx; continue; }

        auto& g = m_Font->GetGlyphInfo(cp);
        float advance = (cp == ' ') ? m_Font->GetSpaceWidth() : g.advance;
        float nextX = x + advance;
        if (localX < nextX) {
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

int UITextInput::ClassifyChar(char c)
{
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return 0;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') return 1;
    return 2;
}

void UITextInput::SelectWordAt(int pos)
{
    if (pos <= 0 || pos > (int)m_Text.size()) return;
    char c = m_Text[pos - 1];
    int cls = ClassifyChar(c);
    if (cls == 0) return;

    int start = pos - 1;
    while (start > 0 && ClassifyChar(m_Text[start - 1]) == cls)
        start--;

    int end = pos;
    while (end < (int)m_Text.size() && ClassifyChar(m_Text[end]) == cls)
        end++;

    m_SelectionStart = start;
    m_CursorPos = end;
}

int UITextInput::FindPrevWordBoundary(int from) const
{
    if (from <= 0) return 0;
    int i = from - 1;
    while (i >= 0 && ClassifyChar(m_Text[i]) == 0)
        i--;
    if (i < 0) return 0;
    int cls = ClassifyChar(m_Text[i]);
    while (i >= 0 && ClassifyChar(m_Text[i]) == cls)
        i--;
    return i + 1;
}

int UITextInput::FindNextWordBoundary(int from) const
{
    if (from >= (int)m_Text.size()) return (int)m_Text.size();
    int i = from;
    if (i < (int)m_Text.size() && ClassifyChar(m_Text[i]) != 0) {
        int cls = ClassifyChar(m_Text[i]);
        while (i < (int)m_Text.size() && ClassifyChar(m_Text[i]) == cls)
            i++;
    }
    while (i < (int)m_Text.size() && ClassifyChar(m_Text[i]) == 0)
        i++;
    return i;
}

} // namespace Leir
