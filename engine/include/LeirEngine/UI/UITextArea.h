#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include <vector>

namespace Leir {

class LEIR_API UITextArea : public UITextInput {
public:
    UITextArea();
    ~UITextArea() override;

    Vector2 GetMinSize() const override;
    bool OnKeyDown(int key) override;
    bool OnPointerDown(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;
    bool OnScroll(float delta) override;

    int GetLineCount() const;
    int GetCursorLine() const;
    int GetCursorCol() const;
    int GetLineStart(int line) const;
    int GetLineEnd(int line) const;  // index of \n or end

    void SetText(const std::string& text);
    void SetFont(Font* font);

    // Word wrap: when enabled, long lines visually wrap inside the widget
    // (rows != logical lines separated by '\n'), and caret/selection/scroll
    // all operate on the visual rows. Optional per instance.
    void SetWordWrap(bool enabled);
    bool IsWordWrapEnabled() const { return m_WordWrap; }

    void SetCustomMinSize(const Vector2& size) { m_CustomMinSize = size; m_HasCustomMinSize = true; }

    // Scroll state
    void SetScrollOffset(const Vector2& offset);
    const Vector2& GetScrollOffset() const { return m_ScrollOffset; }

    // Text block size (whole content, before clipping/scroll). O(n) single pass.
    Vector2 GetContentSize() const override;
    // Visible area = widget rect minus enabled scrollbar strips.
    Vector2 GetViewportSize() const;

    float GetMaxScrollY() const;
    float GetMaxScrollX() const;

    void SetScrollbarWidth(float w) { m_ScrollbarWidth = w; }
    float GetScrollbarWidth() const { return m_ScrollbarWidth; }

    void SetVerticalScrollbarEnabled(bool enabled) { m_VScrollbarEnabled = enabled; }
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }
    void SetHorizontalScrollbarEnabled(bool enabled) { m_HScrollbarEnabled = enabled; }
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    UIScrollbar* GetVerticalScrollbar() const { return m_VScrollbar; }
    UIScrollbar* GetHorizontalScrollbar() const { return m_HScrollbar; }

    float GetCursorXAt(int charIndex) const override;

protected:
    void InsertChar(uint32_t codepoint) override;
    void OnLayoutComputed() override;
    void OnTextMutated() override;

private:
    // One visual row of the text: byte range in m_Text plus its measured width.
    struct VisualRow {
        int startByte = 0;
        int endByte = 0;
        float width = 0.0f;
    };

    void SyncScrollbars();
    void EnsureCaretVisible();
    void EnsureVisualRows() const;
    int VisualRowOfChar(int byteIdx) const;
    float WrapLimit() const;
    void InvalidateWrapModel();

    float m_TargetX = -1.0f;
    Vector2 m_CustomMinSize = {200.0f, 100.0f};
    bool m_HasCustomMinSize = false;

    Vector2 m_ScrollOffset = {0.0f, 0.0f};
    UIScrollbar* m_VScrollbar = nullptr;
    UIScrollbar* m_HScrollbar = nullptr;
    float m_ScrollbarWidth = 10.0f;
    bool m_VScrollbarEnabled = true;
    bool m_HScrollbarEnabled = true;

    // Visual wrap model (lazily rebuilt, see EnsureVisualRows).
    bool m_WordWrap = false;
    mutable std::vector<VisualRow> m_VisualRows;
    mutable unsigned m_ModelGen = 0;
    mutable unsigned m_BuiltGen = 0;
    mutable float m_BuiltWrapWidth = -1.0f;
};

} // namespace Leir
