#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"
#include <functional>

namespace Leir {

class LEIR_API UIFloatInput : public UITextInput {
public:
    UIFloatInput();
    ~UIFloatInput() override;

    void SetValue(float v);
    float GetValue() const { return m_Value; }

    void SetOnValueChanged(std::function<void(float)> cb) { m_OnValueChanged = cb; }

    bool OnTextInput(uint32_t codepoint) override;
    bool OnKeyDown(int key) override;
    void OnFocus() override;
    void OnBlur() override;

private:
    void CommitValue();
    void OnTextMutated() override;

    float m_Value = 0.0f;
    std::function<void(float)> m_OnValueChanged;
};

} // namespace Leir
