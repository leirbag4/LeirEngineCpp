#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>
#include <string>

namespace Leir {

class Font;

class LEIR_API UITextInput : public UIElement {
public:
    UITextInput();
    ~UITextInput() override;

    void SetText(const std::string& text);
    const std::string& GetText() const { return m_Text; }

    void SetFont(Font* font) { m_Font = font; }
    Font* GetFont() const { return m_Font; }

    void SetPlaceholder(const std::string& text) { m_Placeholder = text; }
    const std::string& GetPlaceholder() const { return m_Placeholder; }

    void SetOnChange(std::function<void(const std::string&)> cb) { m_OnChange = cb; }

    void SetTextColor(const Vector4& color) { m_TextColor = color; }
    const Vector4& GetTextColor() const { return m_TextColor; }

    virtual float GetCursorX() const;
    virtual float GetCursorXAt(int charIndex) const;
    int GetCharIndexAtX(float localX) const;
    int GetCursorPos() const { return m_CursorPos; }
    bool IsFocused() const { return m_Focused; }
    bool HasSelection() const { return m_SelectionStart >= 0 && m_SelectionStart != m_CursorPos; }
    int GetSelBegin() const { return m_SelectionStart < m_CursorPos ? m_SelectionStart : m_CursorPos; }
    int GetSelEnd() const { return m_SelectionStart > m_CursorPos ? m_SelectionStart : m_CursorPos; }
    bool IsCaretVisible() const { return m_Focused && (m_CaretCounter / 30) % 2 == 0; }
    void ResetCaretBlink() { m_CaretCounter = 0; }
    void TickCaret() { m_CaretCounter = (m_CaretCounter + 1) % 60; }

    Vector2 GetMinSize() const override;

    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;
    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;
    bool OnTextInput(uint32_t codepoint) override;
    bool OnKeyDown(int key) override;
    void OnFocus() override;
    void OnBlur() override;

protected:
    virtual void InsertChar(uint32_t codepoint);
    void DeleteChar();
    void DeleteForward();
    void UpdateCursorPos();
    void DeleteSelection();
    void ClearSelection() { m_SelectionStart = -1; }
    void CaptureDragPointer();

    std::string m_Text;
    Font* m_Font = nullptr;
    std::function<void(const std::string&)> m_OnChange;
    int m_CursorPos = 0;
    int m_SelectionStart = -1;
    bool m_Focused = false;
    bool m_Hovered = false;
    bool m_Dragging = false;

private:
    std::string m_Placeholder;
    Vector4 m_TextColor = {1.0f, 1.0f, 1.0f, 1.0f};
    int m_CaretCounter = 0;
};

} // namespace Leir
