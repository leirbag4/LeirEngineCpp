#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UIScrollbar.h"

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

protected:
    void InsertChar(uint32_t codepoint) override;
    void OnLayoutComputed() override;

private:
    void SyncScrollbars();
    void EnsureCaretVisible();

    float m_TargetX = -1.0f;
    Vector2 m_CustomMinSize = {200.0f, 100.0f};
    bool m_HasCustomMinSize = false;

    Vector2 m_ScrollOffset = {0.0f, 0.0f};
    UIScrollbar* m_VScrollbar = nullptr;
    UIScrollbar* m_HScrollbar = nullptr;
    float m_ScrollbarWidth = 10.0f;
    bool m_VScrollbarEnabled = true;
    bool m_HScrollbarEnabled = true;
};

} // namespace Leir
