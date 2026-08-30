#pragma once

/**
 * @file UISlider.h
 * @brief Slider widget: track with draggable handle for float values.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>

namespace Leir {

/**
 * @brief Slider: horizontal track with draggable handle for float values.
 * @ingroup UI
 */
class LEIR_API UISlider : public UIElement {
public:
    /**
     * @brief Constructs a slider with default range [0,1].
     */
    UISlider();

    /**
     * @brief Destroys the slider.
     */
    ~UISlider() override;

    /**
     * @brief Sets value range.
     * @param[in] min Minimum value.
     * @param[in] max Maximum value.
     */
    void SetRange(float min, float max) { m_Min = min; m_Max = max; }

    /**
     * @brief Returns minimum value.
     * @return Min.
     */
    float GetMin() const { return m_Min; }

    /**
     * @brief Returns maximum value.
     * @return Max.
     */
    float GetMax() const { return m_Max; }

    /**
     * @brief Sets current value (clamped to range).
     * @param[in] value Value to set.
     */
    void SetValue(float value);

    /**
     * @brief Returns current value.
     * @return Value.
     */
    float GetValue() const { return m_Value; }

    /**
     * @brief Sets change callback.
     * @param[in] cb Callback with new value.
     */
    void SetOnChange(std::function<void(float)> cb) { m_OnChange = cb; }

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
     * @brief Handles pointer press (starts drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer release (ends drag).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Handles pointer move (drag).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Returns whether dragging.
     * @return True if dragging.
     */
    bool IsDragging() const { return m_Dragging; }

    /**
     * @brief Returns whether hovered.
     * @return True if hovered.
     */
    bool IsHovered() const { return m_Hovered; }

    /**
     * @brief Returns handle position in logical pixels.
     * @return Handle X position.
     */
    float HandlePos() const;

    /**
     * @brief Converts X position to value.
     * @param[in] x X position.
     * @return Value in range.
     */
    float ValueFromPos(float x) const;

private:
    float m_Min = 0.0f;                             ///< Minimum value.
    float m_Max = 1.0f;                             ///< Maximum value.
    float m_Value = 0.5f;                           ///< Current value.
    std::function<void(float)> m_OnChange;          ///< Change callback.
    bool m_Dragging = false;                        ///< Dragging flag.
    bool m_Hovered = false;                         ///< Hovered flag.
};

} // namespace Leir
