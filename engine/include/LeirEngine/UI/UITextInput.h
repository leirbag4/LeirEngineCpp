#pragma once

/**
 * @file UITextInput.h
 * @brief Single-line text input widget with caret, selection and clipboard.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>
#include <string>

namespace Leir {

class Font;

/**
 * @brief Single-line text input with caret, selection, word-jump and clipboard.
 * @ingroup UI
 * @details Handles focus, pointer capture for drag-selection, double-click word
 *  selection, Ctrl+A, and text mutation via InsertChar. Renders caret and
 *  selection via UIRenderer.
 */
class LEIR_API UITextInput : public UIElement {
public:
    /**
     * @brief Constructs an empty text input.
     */
    UITextInput();

    /**
     * @brief Destroys the text input.
     */
    ~UITextInput() override;

    /**
     * @brief Sets the text content.
     * @param[in] text Text to set.
     */
    void SetText(const std::string& text);

    /**
     * @brief Returns the text content.
     * @return Text string.
     */
    const std::string& GetText() const { return m_Text; }

    /**
     * @brief Sets the font.
     * @param[in] font Font pointer (not owned).
     */
    void SetFont(Font* font) { m_Font = font; }

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Sets placeholder text shown when empty and unfocused.
     * @param[in] text Placeholder text.
     */
    void SetPlaceholder(const std::string& text) { m_Placeholder = text; }

    /**
     * @brief Returns placeholder text.
     * @return Placeholder string.
     */
    const std::string& GetPlaceholder() const { return m_Placeholder; }

    /**
     * @brief Sets the callback invoked when text changes.
     * @param[in] cb Callback with new text.
     */
    void SetOnChange(std::function<void(const std::string&)> cb) { m_OnChange = cb; }

    /**
     * @brief Sets the callback invoked before text changes (return false to reject).
     * @param[in] cb Callback with new text; return true to accept.
     */
    void SetOnTextChanged(std::function<bool(const std::string&)> cb) { m_OnTextChanged = cb; }

    /**
     * @brief Enables auto-select all on focus.
     * @param[in] v True to auto-select.
     */
    void SetAutoSelect(bool v) { m_AutoSelect = v; }

    /**
     * @brief Returns auto-select flag.
     * @return True if auto-select.
     */
    bool GetAutoSelect() const { return m_AutoSelect; }

    /**
     * @brief Sets editable flag.
     * @details When false the control is read-only: text renders and can still
     *  scroll, but caret cannot be placed and editing/selection is disabled.
     * @param[in] editable True to make editable.
     */
    void SetEditable(bool editable);

    /**
     * @brief Returns editable flag.
     * @return True if editable.
     */
    bool IsEditable() const { return m_Editable; }

    /**
     * @brief Sets text color.
     * @param[in] color Text RGBA color.
     */
    void SetTextColor(const Vector4& color) { m_TextColor = color; }

    /**
     * @brief Returns text color.
     * @return Color.
     */
    const Vector4& GetTextColor() const { return m_TextColor; }

    /**
     * @brief Returns X position of caret within text.
     * @return X offset in logical pixels.
     */
    virtual float GetCursorX() const;

    /**
     * @brief Returns X position of a character index.
     * @param[in] charIndex Character index.
     * @return X offset.
     */
    virtual float GetCursorXAt(int charIndex) const;

    /**
     * @brief Returns character index at a local X position.
     * @param[in] localX Local X in logical pixels.
     * @return Character index.
     */
    int GetCharIndexAtX(float localX) const;

    /**
     * @brief Returns cursor character index.
     * @return Cursor position.
     */
    int GetCursorPos() const { return m_CursorPos; }

    /**
     * @brief Returns focused state.
     * @return True if focused.
     */
    bool IsFocused() const { return m_Focused; }

    /**
     * @brief Returns whether there is a selection.
     * @return True if selecting.
     */
    bool HasSelection() const { return m_SelectionStart >= 0 && m_SelectionStart != m_CursorPos; }

    /**
     * @brief Returns selection begin (min of cursor/selectionStart).
     * @return Begin index.
     */
    int GetSelBegin() const { return m_SelectionStart < m_CursorPos ? m_SelectionStart : m_CursorPos; }

    /**
     * @brief Returns selection end (max of cursor/selectionStart).
     * @return End index.
     */
    int GetSelEnd() const { return m_SelectionStart > m_CursorPos ? m_SelectionStart : m_CursorPos; }

    /**
     * @brief Returns selected text.
     * @return Selected substring or empty if no selection.
     */
    std::string GetSelectedText() const;

    /**
     * @brief Returns whether caret should be visible (blink).
     * @return True if caret visible.
     */
    bool IsCaretVisible() const { return m_Editable && m_Focused && (m_CaretCounter / 30) % 2 == 0; }

    /**
     * @brief Resets caret blink timer.
     */
    void ResetCaretBlink() { m_CaretCounter = 0; }

    /**
     * @brief Advances caret blink counter (called per frame by UIRenderer).
     */
    void TickCaret() { m_CaretCounter = (m_CaretCounter + 1) % 60; m_FrameCounter++; }

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Called when pointer enters.
     * @param[in] pos Pointer position.
     */
    void OnPointerEnter(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

    /**
     * @brief Called on pointer press (places caret, handles selection).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Called on pointer release (ends drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Called on pointer move (drag selection).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Handles text input (insert char).
     * @param[in] codepoint Unicode codepoint.
     * @return True if consumed.
     */
    bool OnTextInput(uint32_t codepoint) override;

    /**
     * @brief Handles key down (navigation, deletion, word-jump).
     * @param[in] key Key code.
     * @return True if consumed.
     */
    bool OnKeyDown(int key) override;

    /**
     * @brief Called when element gains focus.
     */
    void OnFocus() override;

    /**
     * @brief Called when element loses focus.
     */
    void OnBlur() override;

protected:
    /**
     * @brief Inserts a character at the caret (handles selection).
     * @param[in] codepoint Codepoint to insert.
     */
    virtual void InsertChar(uint32_t codepoint);

    /**
     * @brief Deletes character before caret or selection.
     */
    void DeleteChar();

    /**
     * @brief Deletes character after caret (Delete key).
     */
    void DeleteForward();

    /**
     * @brief Updates cursor position after mutation.
     */
    void UpdateCursorPos();

    /**
     * @brief Deletes current selection.
     */
    void DeleteSelection();

    /**
     * @brief Clears selection.
     */
    void ClearSelection() { m_SelectionStart = -1; }

    /**
     * @brief Captures pointer for drag.
     */
    void CaptureDragPointer();

    /**
     * @brief Selects word at a character position.
     * @param[in] pos Character index.
     */
    void SelectWordAt(int pos);

    /**
     * @brief Finds previous word boundary.
     * @param[in] from Start index.
     * @return Boundary index.
     */
    int FindPrevWordBoundary(int from) const;

    /**
     * @brief Finds next word boundary.
     * @param[in] from Start index.
     * @return Boundary index.
     */
    int FindNextWordBoundary(int from) const;

    /**
     * @brief Called after text mutated (rebuild, notify).
     */
    virtual void OnTextMutated();

    std::string m_Text;                                     ///< Text content.
    Font* m_Font = nullptr;                                 ///< Font (not owned).
    std::function<void(const std::string&)> m_OnChange;     ///< Change callback.
    std::function<bool(const std::string&)> m_OnTextChanged;///< Pre-change callback.
    int m_CursorPos = 0;                                    ///< Cursor index.
    int m_SelectionStart = -1;                              ///< Selection anchor.
    bool m_Focused = false;                                 ///< Focused flag.
    bool m_Hovered = false;                                 ///< Hovered flag.
    bool m_Dragging = false;                                ///< Dragging flag.
    bool m_AutoSelect = false;                              ///< Auto-select on focus.
    bool m_Editable = true;                                 ///< Editable flag.
    int m_CaretCounter = 0;                                 ///< Blink counter.
    int m_FrameCounter = 0;                                 ///< Frame counter.
    int m_LastClickFrame = -30;                             ///< Last click frame for double-click.
    int m_LastClickPos = -1;                                ///< Last click position.

private:
    static int ClassifyChar(char c);

    std::string m_Placeholder;                              ///< Placeholder text.
    Vector4 m_TextColor = {1.0f, 1.0f, 1.0f, 1.0f};           ///< Text color.
};

} // namespace Leir
