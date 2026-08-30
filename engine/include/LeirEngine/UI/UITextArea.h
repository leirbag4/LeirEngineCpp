#pragma once

/**
 * @file UITextArea.h
 * @brief Multiline text area with scrollbars, word-wrap and caret navigation.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include <vector>

namespace Leir {

/**
 * @brief Multiline text area: scrollable, word-wrap and line-aware caret.
 * @ingroup UI
 * @details Inherits editing/selection from UITextInput. Adds vertical and
 *  horizontal scrollbars, visual-row model for word-wrap, and line/col helpers.
 */
class LEIR_API UITextArea : public UITextInput {
public:
    /**
     * @brief Constructs a multiline text area with scrollbars.
     */
    UITextArea();

    /**
     * @brief Destroys the text area and its scrollbars.
     */
    ~UITextArea() override;

    /**
     * @brief Returns whether the area owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if child is an internal scrollbar.
     */
    bool OwnsChild(const UIElement* child) const override {
        return child == m_VScrollbar || child == m_HScrollbar;
    }

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Handles key down (Enter, Up/Down with targetX, etc.).
     * @param[in] key Key code.
     * @return True if consumed.
     */
    bool OnKeyDown(int key) override;

    /**
     * @brief Handles pointer press (places caret, handles scroll).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer move (drag selection, Y-aware).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Handles scroll wheel (vertical/horizontal).
     * @param[in] delta Scroll delta.
     * @return True if consumed.
     */
    bool OnScroll(float delta) override;

    /**
     * @brief Returns number of lines (logical or visual when wrapping).
     * @return Line count.
     */
    int GetLineCount() const;

    /**
     * @brief Returns cursor line index.
     * @return Line index.
     */
    int GetCursorLine() const;

    /**
     * @brief Returns cursor column within its line.
     * @return Column index.
     */
    int GetCursorCol() const;

    /**
     * @brief Returns start byte index of a line.
     * @param[in] line Line index.
     * @return Start byte index.
     */
    int GetLineStart(int line) const;

    /**
     * @brief Returns end byte index of a line (index of \\n or end).
     * @param[in] line Line index.
     * @return End byte index.
     */
    int GetLineEnd(int line) const;

    /**
     * @brief Sets text content (may contain \\n).
     * @param[in] text Text to set.
     */
    void SetText(const std::string& text);

    /**
     * @brief Sets font (rebuilds visual rows if wrapping).
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font);

    /**
     * @brief Enables word-wrap (visual rows != logical lines).
     * @param[in] enabled True to enable wrap.
     */
    void SetWordWrap(bool enabled);

    /**
     * @brief Returns whether word-wrap is enabled.
     * @return True if wrapping.
     */
    bool IsWordWrapEnabled() const { return m_WordWrap; }

    /**
     * @brief Sets custom minimum size override for this instance.
     * @param[in] size Minimum size.
     */
    void SetCustomMinSize(const Vector2& size) { m_CustomMinSize = size; m_HasCustomMinSize = true; }

    /**
     * @brief Sets scroll offset (clamped).
     * @param[in] offset Scroll offset (x,y).
     */
    void SetScrollOffset(const Vector2& offset);

    /**
     * @brief Returns scroll offset.
     * @return Scroll offset.
     */
    const Vector2& GetScrollOffset() const { return m_ScrollOffset; }

    /**
     * @brief Returns content size (whole text block, before clipping).
     * @return Content size.
     */
    Vector2 GetContentSize() const override;

    /**
     * @brief Returns viewport size (widget rect minus scrollbar strips).
     * @return Viewport size.
     */
    Vector2 GetViewportSize() const;

    /**
     * @brief Returns max vertical scroll.
     * @return Max Y scroll.
     */
    float GetMaxScrollY() const;

    /**
     * @brief Returns max horizontal scroll.
     * @return Max X scroll.
     */
    float GetMaxScrollX() const;

    /**
     * @brief Sets scrollbar strip width.
     * @param[in] w Width in logical pixels.
     */
    void SetScrollbarWidth(float w) { m_ScrollbarWidth = w; }

    /**
     * @brief Returns scrollbar width.
     * @return Width.
     */
    float GetScrollbarWidth() const { return m_ScrollbarWidth; }

    /**
     * @brief Enables vertical scrollbar.
     * @param[in] enabled True to enable.
     */
    void SetVerticalScrollbarEnabled(bool enabled) { m_VScrollbarEnabled = enabled; }

    /**
     * @brief Returns whether vertical scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }

    /**
     * @brief Enables horizontal scrollbar.
     * @param[in] enabled True to enable.
     */
    void SetHorizontalScrollbarEnabled(bool enabled) { m_HScrollbarEnabled = enabled; }

    /**
     * @brief Returns whether horizontal scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    /**
     * @brief Returns vertical scrollbar widget.
     * @return Scrollbar pointer.
     */
    UIScrollbar* GetVerticalScrollbar() const { return m_VScrollbar; }

    /**
     * @brief Returns horizontal scrollbar widget.
     * @return Scrollbar pointer.
     */
    UIScrollbar* GetHorizontalScrollbar() const { return m_HScrollbar; }

    /**
     * @brief Returns X position of a character index (multiline-aware).
     * @param[in] charIndex Character index.
     * @return X offset.
     */
    float GetCursorXAt(int charIndex) const override;

protected:
    /**
     * @brief Inserts a character (handles \\n).
     * @param[in] codepoint Codepoint to insert.
     */
    void InsertChar(uint32_t codepoint) override;

    /**
     * @brief Called after layout to sync scrollbars and caret.
     */
    void OnLayoutComputed() override;

    /**
     * @brief Called after text mutated (invalidates wrap model).
     */
    void OnTextMutated() override;

private:
    /**
     * @brief One visual row of text: byte range and measured width.
     * @ingroup UI
     */
    struct VisualRow {
        int startByte = 0; ///< Start byte index in m_Text.
        int endByte = 0;   ///< End byte index (exclusive).
        float width = 0.0f;///< Measured width.
    };

    void SyncScrollbars();
    void EnsureCaretVisible();
    void EnsureVisualRows() const;
    int VisualRowOfChar(int byteIdx) const;
    float WrapLimit() const;
    void InvalidateWrapModel();

    float m_TargetX = -1.0f;                          ///< Target X for Up/Down navigation.
    Vector2 m_CustomMinSize = {200.0f, 100.0f};        ///< Custom min size.
    bool m_HasCustomMinSize = false;                  ///< Custom min size flag.

    Vector2 m_ScrollOffset = {0.0f, 0.0f};             ///< Scroll offset.
    UIScrollbar* m_VScrollbar = nullptr;               ///< Vertical scrollbar (owned).
    UIScrollbar* m_HScrollbar = nullptr;               ///< Horizontal scrollbar (owned).
    float m_ScrollbarWidth = 10.0f;                   ///< Scrollbar strip width.
    bool m_VScrollbarEnabled = true;                  ///< Vertical scrollbar enabled.
    bool m_HScrollbarEnabled = true;                  ///< Horizontal scrollbar enabled.

    bool m_WordWrap = false;                          ///< Word-wrap enabled.
    mutable std::vector<VisualRow> m_VisualRows;      ///< Visual rows (lazy).
    mutable unsigned m_ModelGen = 0;                  ///< Model generation.
    mutable unsigned m_BuiltGen = 0;                  ///< Built generation.
    mutable float m_BuiltWrapWidth = -1.0f;           ///< Wrap width at build time.
};

} // namespace Leir
