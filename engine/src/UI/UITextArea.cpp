#include "LeirEngine/UI/UITextArea.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/Keyboard.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "LeirEngine/Core/Log.h"

namespace Leir {

UITextArea::UITextArea()
{
    SetClip(true);

    m_VScrollbar = new UIScrollbar(true);
    m_VScrollbar->SetName("TextAreaVScrollbar");
    AddChild(m_VScrollbar);
    m_VScrollbar->SetOnScroll([this](float v) {
        Vector2 off = m_ScrollOffset;
        off.y = v * GetMaxScrollY();
        SetScrollOffset(off);
    });

    m_HScrollbar = new UIScrollbar(false);
    m_HScrollbar->SetName("TextAreaHScrollbar");
    AddChild(m_HScrollbar);
    m_HScrollbar->SetOnScroll([this](float v) {
        Vector2 off = m_ScrollOffset;
        off.x = v * GetMaxScrollX();
        SetScrollOffset(off);
    });
}

UITextArea::~UITextArea()
{
    if (m_VScrollbar) {
        RemoveChild(m_VScrollbar);
        delete m_VScrollbar;
        m_VScrollbar = nullptr;
    }
    if (m_HScrollbar) {
        RemoveChild(m_HScrollbar);
        delete m_HScrollbar;
        m_HScrollbar = nullptr;
    }
}

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

Vector2 UITextArea::GetContentSize() const
{
    if (!m_Font) return {0.0f, 0.0f};
    float maxLineW = 0.0f;
    float lineW = 0.0f;
    int lines = 1;
    for (int i = 0; i < (int)m_Text.size();) {
        uint32_t cp = (unsigned char)m_Text[i];
        if (cp < 0x80) { ++i; }
        else if ((cp & 0xE0) == 0xC0 && i + 1 < (int)m_Text.size()) { cp = ((cp & 0x1F) << 6) | (m_Text[i+1] & 0x3F); i += 2; }
        else if ((cp & 0xF0) == 0xE0 && i + 2 < (int)m_Text.size()) { cp = ((cp & 0x0F) << 12) | ((m_Text[i+1] & 0x3F) << 6) | (m_Text[i+2] & 0x3F); i += 3; }
        else { ++i; continue; }
        if (cp == '\n') {
            maxLineW = std::max(maxLineW, lineW);
            lineW = 0.0f;
            ++lines;
            continue;
        }
        if (cp == ' ') lineW += m_Font->GetSpaceWidth();
        else lineW += m_Font->GetGlyphInfo(cp).advance;
    }
    maxLineW = std::max(maxLineW, lineW);
    return {maxLineW + 8.0f, (float)lines * m_Font->GetLineHeight() + 8.0f};
}

Vector2 UITextArea::GetViewportSize() const
{
    const auto& cr = GetComputedRect();
    const float vw = m_VScrollbarEnabled ? m_ScrollbarWidth : 0.0f;
    const float hh = m_HScrollbarEnabled ? m_ScrollbarWidth : 0.0f;
    return {std::max(0.0f, cr.z - vw), std::max(0.0f, cr.w - hh)};
}

float UITextArea::GetMaxScrollY() const
{
    return std::max(0.0f, GetContentSize().y - GetViewportSize().y);
}

float UITextArea::GetMaxScrollX() const
{
    return std::max(0.0f, GetContentSize().x - GetViewportSize().x);
}

void UITextArea::SetScrollOffset(const Vector2& offset)
{
    m_ScrollOffset.x = std::clamp(offset.x, 0.0f, std::max(0.0f, GetMaxScrollX()));
    m_ScrollOffset.y = std::clamp(offset.y, 0.0f, std::max(0.0f, GetMaxScrollY()));
}

void UITextArea::SyncScrollbars()
{
    const auto& cr = GetComputedRect();
    const Vector2 vp = GetViewportSize();
    const Vector2 content = GetContentSize();
    const float rightEdge = std::round(cr.x + cr.z);
    const float bottomEdge = std::round(cr.y + cr.w);

    const bool vOverflow = m_VScrollbarEnabled && content.y > vp.y && vp.y > 1.0f;
    const bool hOverflow = m_HScrollbarEnabled && content.x > vp.x && vp.x > 1.0f;

    if (m_VScrollbar) {
        m_VScrollbar->SetActive(vOverflow);
        if (vOverflow) {
            m_VScrollbar->GetRect().anchor = AnchorSet::TopLeft();
            m_VScrollbar->GetRect().offset = {
                rightEdge - m_ScrollbarWidth, std::round(cr.y),
                rightEdge, hOverflow ? (bottomEdge - m_ScrollbarWidth) : bottomEdge
            };
            m_VScrollbar->ComputeLayout({cr.z, cr.w});
            m_VScrollbar->SetRange(vp.y, content.y);
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
                std::round(cr.x), bottomEdge - m_ScrollbarWidth,
                vOverflow ? (rightEdge - m_ScrollbarWidth) : rightEdge,
                bottomEdge
            };
            m_HScrollbar->ComputeLayout({cr.z, cr.w});
            m_HScrollbar->SetRange(vp.x, content.x);
            const float maxX = GetMaxScrollX();
            if (maxX > 0.0f)
                m_HScrollbar->SetValue(m_ScrollOffset.x / maxX);
        }
    }
}

void UITextArea::OnLayoutComputed()
{
    // Clamp the offset to the current content/viewport, then sync the bars.
    SetScrollOffset(m_ScrollOffset);
    SyncScrollbars();
}

void UITextArea::EnsureCaretVisible()
{
    if (!m_Font) return;

    const Vector2 vp = GetViewportSize();
    const float lineH = m_Font->GetLineHeight();
    const int line = GetCursorLine();

    Vector2 off = m_ScrollOffset;
    const float topPad = 4.0f;
    float lineTop = topPad + line * lineH;
    float lineBottom = lineTop + lineH;

    if (lineTop < off.y)
        off.y = lineTop;
    else if (lineBottom > off.y + vp.y)
        off.y = lineBottom - vp.y;

    const float colX = topPad + GetCursorX();
    if (colX < off.x)
        off.x = colX;
    else if (colX + 1.0f > off.x + vp.x)
        off.x = colX + 1.0f - vp.x;

    SetScrollOffset(off);
}

void UITextArea::InsertChar(uint32_t codepoint)
{
    if (codepoint == '\n') {
        if (!m_Editable) return;
        m_Text.insert(m_CursorPos, 1, '\n');
        m_CursorPos++;
        if (m_OnChange) m_OnChange(m_Text);
        OnTextMutated();
        EnsureCaretVisible();
        return;
    }
    UITextInput::InsertChar(codepoint);
    EnsureCaretVisible();
}

bool UITextArea::OnKeyDown(int key)
{
    if (!m_Focused || !m_Editable) return false;

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
        EnsureCaretVisible();
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
        EnsureCaretVisible();
        return true;
    }

    bool handled = UITextInput::OnKeyDown(key);
    if (handled)
        EnsureCaretVisible();
    return handled;
}

bool UITextArea::OnPointerDown(const Vector2& pos)
{
    if (!m_Editable) return false;
    ResetCaretBlink();
    if (m_Font) {
        const auto& cr = GetComputedRect();
        float lineH = m_Font->GetLineHeight();
        float localY = pos.y - cr.y + m_ScrollOffset.y;
        int line = std::min((int)(localY / lineH), GetLineCount() - 1);
        if (line < 0) line = 0;

        float localX = pos.x - (cr.x + 4.0f) + m_ScrollOffset.x;
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
        EnsureCaretVisible();
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
    EnsureCaretVisible();
    return true;
}

void UITextArea::OnPointerMove(const Vector2& pos)
{
    if (!m_Focused || !m_Font || !m_Dragging || !m_Editable) return;

    if (m_SelectionStart < 0)
        m_SelectionStart = m_CursorPos;

    const auto& cr = GetComputedRect();
    float lineH = m_Font->GetLineHeight();
    float localY = pos.y - cr.y + m_ScrollOffset.y;
    int line = std::min((int)(localY / lineH), GetLineCount() - 1);
    if (line < 0) line = 0;

    float localX = pos.x - (cr.x + 4.0f) + m_ScrollOffset.x;
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
    EnsureCaretVisible();
}

bool UITextArea::OnScroll(float delta)
{
    if (GetMaxScrollY() <= 0.0f && GetMaxScrollX() <= 0.0f)
        return false;

    Vector2 off = m_ScrollOffset;
    const bool horizontal = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    const float lineHeight = m_Font ? m_Font->GetLineHeight() : 16.0f;
    if (horizontal && GetMaxScrollX() > 0.0f)
        off.x -= delta * lineHeight;
    else
        off.y -= delta * lineHeight;
    SetScrollOffset(off);
    return true;
}

} // namespace Leir
