#include "LeirEngine/UI/UITextArea.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/Keyboard.h"
#include <cstdlib>
#include "LeirEngine/Core/Log.h"

namespace Leir {

UITextArea::UITextArea() = default;
UITextArea::~UITextArea() = default;

Vector2 UITextArea::GetMinSize() const
{
    return m_HasCustomMinSize ? m_CustomMinSize : Vector2{200.0f, 100.0f};
}

int UITextArea::GetLineCount() const
{
    if (m_Text.empty()) return 1;
    int count = 1;
    for (char c : m_Text)
        if (c == '\n') count++;
    return count;
}

int UITextArea::GetCursorLine() const
{
    int line = 0;
    for (int i = 0; i < m_CursorPos && i < (int)m_Text.size(); ++i)
        if (m_Text[i] == '\n') line++;
    return line;
}

int UITextArea::GetCursorCol() const
{
    int lineStart = 0;
    for (int i = m_CursorPos - 1; i >= 0; --i)
        if (m_Text[i] == '\n') { lineStart = i + 1; break; }
    return m_CursorPos - lineStart;
}

int UITextArea::GetLineStart(int line) const
{
    int current = 0;
    for (int i = 0; i < (int)m_Text.size(); ++i) {
        if (current == line) return i;
        if (m_Text[i] == '\n') current++;
    }
    return (int)m_Text.size();
}

int UITextArea::GetLineEnd(int line) const
{
    int current = 0;
    for (int i = 0; i < (int)m_Text.size(); ++i) {
        if (current == line) {
            while (i < (int)m_Text.size() && m_Text[i] != '\n') ++i;
            return i;
        }
        if (m_Text[i] == '\n') current++;
    }
    return (int)m_Text.size();
}

void UITextArea::InsertChar(uint32_t codepoint)
{
    if (codepoint == '\n') {
        m_Text.insert(m_CursorPos, 1, '\n');
        m_CursorPos++;
        if (m_OnChange) m_OnChange(m_Text);
        return;
    }
    UITextInput::InsertChar(codepoint);
}

bool UITextArea::OnKeyDown(int key)
{
    if (!m_Focused) return false;

    ResetCaretBlink();

    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);

    if (key == static_cast<int>(Key::Enter) || key == 335) {
        if (HasSelection()) DeleteSelection();
        InsertChar('\n');
        m_TargetX = -1.0f;
        return true;
    }

    if (key == static_cast<int>(Key::Up)) {
        if (m_TargetX < 0) m_TargetX = GetCursorX();
        int line = GetCursorLine();
        if (line > 0) {
            int prevLineStart = GetLineStart(line - 1);
            int prevLineEnd = GetLineEnd(line - 1);
            int prevLineLen = prevLineEnd - prevLineStart;
            int targetCol = 0;
            if (!m_Font) {
                targetCol = std::min((int)(m_TargetX / 8.0f), prevLineLen);
            } else {
                float x = 0.0f;
                for (int i = 0; i < prevLineLen;) {
                    int idx = prevLineStart + i;
                    uint32_t cp = (unsigned char)m_Text[idx];
                    if (cp < 0x80) { ++i; }
                    else if ((cp & 0xE0) == 0xC0 && idx + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[idx+1] & 0x3F); i += 2; }
                    else if ((cp & 0xF0) == 0xE0 && idx + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[idx+1] & 0x3F) << 6) | (m_Text[idx+2] & 0x3F); i += 3; }
                    else { ++i; continue; }
                    float nextX = x + m_Font->GetGlyphInfo(cp).advance;
                    if (nextX > m_TargetX) {
                        targetCol = (m_TargetX - x < nextX - m_TargetX) ? (i - (cp < 0x80 ? 1 : (cp < 0xE0 ? 2 : 3))) : i;
                        break;
                    }
                    x = nextX;
                    targetCol = i;
                }
            }
            if (!shift) ClearSelection();
            else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            m_CursorPos = prevLineStart + std::min(targetCol, prevLineLen);
        }
        return true;
    }

    if (key == static_cast<int>(Key::Down)) {
        if (m_TargetX < 0) m_TargetX = GetCursorX();
        int line = GetCursorLine();
        int totalLines = GetLineCount();
        if (line + 1 < totalLines) {
            int nextLineStart = GetLineStart(line + 1);
            int nextLineEnd = GetLineEnd(line + 1);
            int nextLineLen = nextLineEnd - nextLineStart;
            int targetCol = 0;
            if (!m_Font) {
                targetCol = std::min((int)(m_TargetX / 8.0f), nextLineLen);
            } else {
                float x = 0.0f;
                for (int i = 0; i < nextLineLen;) {
                    int idx = nextLineStart + i;
                    uint32_t cp = (unsigned char)m_Text[idx];
                    if (cp < 0x80) { ++i; }
                    else if ((cp & 0xE0) == 0xC0 && idx + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[idx+1] & 0x3F); i += 2; }
                    else if ((cp & 0xF0) == 0xE0 && idx + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[idx+1] & 0x3F) << 6) | (m_Text[idx+2] & 0x3F); i += 3; }
                    else { ++i; continue; }
                    float nextX = x + m_Font->GetGlyphInfo(cp).advance;
                    if (nextX > m_TargetX) {
                        targetCol = (m_TargetX - x < nextX - m_TargetX) ? (i - (cp < 0x80 ? 1 : (cp < 0xE0 ? 2 : 3))) : i;
                        break;
                    }
                    x = nextX;
                    targetCol = i;
                }
            }
            if (!shift) ClearSelection();
            else if (m_SelectionStart < 0) m_SelectionStart = m_CursorPos;
            m_CursorPos = nextLineStart + std::min(targetCol, nextLineLen);
        }
        return true;
    }

    return UITextInput::OnKeyDown(key);
}

bool UITextArea::OnPointerDown(const Vector2& pos)
{
    ResetCaretBlink();
    if (m_Font) {
        const auto& cr = GetComputedRect();
        float lineH = m_Font->GetLineHeight();
        float localY = pos.y - cr.y;
        int line = std::min((int)(localY / lineH), GetLineCount() - 1);
        if (line < 0) line = 0;

        float localX = pos.x - (cr.x + 4.0f);
        int lineStart = GetLineStart(line);
        int lineEnd = GetLineEnd(line);
        int lineLen = lineEnd - lineStart;

        // Find column from x within the line
        int col = 0;
        float x = 0.0f;
        for (int i = 0; i < lineLen && lineStart + i < (int)m_Text.size();) {
            int idx = lineStart + i;
            uint32_t cp = (unsigned char)m_Text[idx];
            int step = 1;
            if (cp < 0x80) { step = 1; }
            else if ((cp & 0xE0) == 0xC0 && idx + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[idx+1] & 0x3F); step = 2; }
            else if ((cp & 0xF0) == 0xE0 && idx + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[idx+1] & 0x3F) << 6) | (m_Text[idx+2] & 0x3F); step = 3; }
            else { ++i; continue; }

            if (cp == '\n') { x = 0.0f; i += step; continue; }

            float advance = (cp == ' ') ? m_Font->GetSpaceWidth() : m_Font->GetGlyphInfo(cp).advance;
            float nextX = x + advance;
            if (localX < nextX) {
                col = (localX - x < nextX - localX) ? i : i + step;
                break;
            }
            x = nextX;
            i += step;
            col = i;
        }

        m_CursorPos = lineStart + std::min(col, lineLen);
        m_TargetX = -1.0f;
    } else {
        m_CursorPos = (int)m_Text.size();
    }

    // Double-click detection: within ~15 frames (~250ms) at nearly the same position
    int framesSinceLast = m_FrameCounter - m_LastClickFrame;
    int posDiff = abs(m_LastClickPos - m_CursorPos);
    bool noShift = !Keyboard::IsDown(Key::LeftShift) && !Keyboard::IsDown(Key::RightShift);
    bool doubleClick = noShift && m_LastClickPos >= 0 && framesSinceLast < 15 && posDiff <= 3;
    m_LastClickFrame = m_FrameCounter;
    m_LastClickPos = m_CursorPos;

    if (doubleClick) {
        XConsole::Trace("[TextArea '{}'] Double-click detected (frames={} posDiff={})", GetName().c_str(), framesSinceLast, posDiff);
        SelectWordAt(m_CursorPos);
        m_Dragging = false;
        return true;
    }

    bool shiftDown = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    if (shiftDown) {
        if (m_SelectionStart < 0)
            m_SelectionStart = m_CursorPos;
    } else {
        ClearSelection();
    }
    m_Dragging = true;
    CaptureDragPointer();
    return true;
}

void UITextArea::OnPointerMove(const Vector2& pos)
{
    if (!m_Focused || !m_Font || !m_Dragging) return;

    if (m_SelectionStart < 0)
        m_SelectionStart = m_CursorPos;

    const auto& cr = GetComputedRect();
    float lineH = m_Font->GetLineHeight();
    float localY = pos.y - cr.y;
    int line = std::min((int)(localY / lineH), GetLineCount() - 1);
    if (line < 0) line = 0;

    float localX = pos.x - (cr.x + 4.0f);
    int lineStart = GetLineStart(line);
    int lineEnd = GetLineEnd(line);
    int lineLen = lineEnd - lineStart;

    int col = 0;
    float x = 0.0f;
    for (int i = 0; i < lineLen && lineStart + i < (int)m_Text.size();) {
        int idx = lineStart + i;
        uint32_t cp = (unsigned char)m_Text[idx];
        int step = 1;
        if (cp < 0x80) { step = 1; }
        else if ((cp & 0xE0) == 0xC0 && idx + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[idx+1] & 0x3F); step = 2; }
        else if ((cp & 0xF0) == 0xE0 && idx + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[idx+1] & 0x3F) << 6) | (m_Text[idx+2] & 0x3F); step = 3; }
        else { ++i; continue; }

        if (cp == '\n') { x = 0.0f; i += step; continue; }

        float advance = (cp == ' ') ? m_Font->GetSpaceWidth() : m_Font->GetGlyphInfo(cp).advance;
        float nextX = x + advance;
        if (localX < nextX) {
            col = (localX - x < nextX - localX) ? i : i + step;
            break;
        }
        x = nextX;
        i += step;
        col = i;
    }

    m_CursorPos = lineStart + std::min(col, lineLen);
}

} // namespace Leir
