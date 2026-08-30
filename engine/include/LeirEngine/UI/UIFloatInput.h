#pragma once

/**
 * @file UIFloatInput.h
 * @brief Float input widget (filters to numeric characters).
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UITextInput.h"
#include <functional>

namespace Leir {

/**
 * @brief Float input: single-line text field that filters to numeric input.
 * @ingroup UI
 * @details Accepts [0-9] + - . , commits on Enter/Blur.
 */
class LEIR_API UIFloatInput : public UITextInput {
public:
    /**
     * @brief Constructs a float input with value 0.
     */
    UIFloatInput();

    /**
     * @brief Destroys the float input.
     */
    ~UIFloatInput() override;

    /**
     * @brief Sets the float value (updates text).
     * @param[in] v Value to set.
     */
    void SetValue(float v);

    /**
     * @brief Returns the current float value.
     * @return Value.
     */
    float GetValue() const { return m_Value; }

    /**
     * @brief Sets the callback invoked when value changes.
     * @param[in] cb Callback with new value.
     */
    void SetOnValueChanged(std::function<void(float)> cb) { m_OnValueChanged = cb; }

    /**
     * @brief Handles text input (filters to numeric chars).
     * @param[in] codepoint Codepoint.
     * @return True if consumed.
     */
    bool OnTextInput(uint32_t codepoint) override;

    /**
     * @brief Handles key down (Enter commits).
     * @param[in] key Key code.
     * @return True if consumed.
     */
    bool OnKeyDown(int key) override;

    /**
     * @brief Called when focused.
     */
    void OnFocus() override;

    /**
     * @brief Called when blurred (commits value).
     */
    void OnBlur() override;

private:
    void CommitValue();
    void OnTextMutated() override;

    float m_Value = 0.0f;                               ///< Current value.
    std::function<void(float)> m_OnValueChanged;        ///< Value changed callback.
};

} // namespace Leir
