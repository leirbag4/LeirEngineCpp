#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"

namespace Leir {

class LEIR_API UITextArea : public UITextInput {
public:
    UITextArea();
    ~UITextArea() override;

    Vector2 GetMinSize() const override;
    bool OnKeyDown(int key) override;
    bool OnPointerDown(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;

    int GetLineCount() const;
    int GetCursorLine() const;
    int GetCursorCol() const;
    int GetLineStart(int line) const;
    int GetLineEnd(int line) const;  // index of \n or end

protected:
    void InsertChar(uint32_t codepoint) override;

private:
    float m_TargetX = -1.0f;
};

} // namespace Leir
