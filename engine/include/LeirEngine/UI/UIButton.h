#pragma once

/**
 * @file UIButton.h
 * @brief Clickable button widget with text, colors and hover/press states.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>

namespace Leir {

class Font;

/**
 * @brief Visual state of a button.
 * @ingroup UI
 */
enum class LEIR_API ButtonState {
    Normal,  ///< Idle.
    Hovered, ///< Pointer over.
    Pressed, ///< Pointer down.
};

/**
 * @brief Horizontal text alignment inside a button.
 * @ingroup UI
 */
enum class LEIR_API ButtonTextAlign {
    Left,   ///< Left with 6px inset (legacy).
    Center, ///< Centered.
    Right,  ///< Right aligned.
};

/**
 * @brief Clickable button: text label with background colors per state.
 * @ingroup UI
 * @details Handles pointer enter/exit/down/up and invokes OnClick on release
 *  inside the hit area. Background color follows the current ButtonState.
 */
class LEIR_API UIButton : public UIElement {
public:
    /**
     * @brief Constructs a button with default colors and Left alignment.
     */
    UIButton();

    /**
     * @brief Destroys the button.
     */
    ~UIButton() override;

    /**
     * @brief Sets the button text.
     * @param[in] text UTF-8 text.
     */
    void SetText(const std::string& text) { m_Text = text; }

    /**
     * @brief Returns the button text.
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
     * @brief Sets background colors for the three states.
     * @param[in] normal Normal state color.
     * @param[in] hover Hovered state color.
     * @param[in] pressed Pressed state color.
     */
    void SetColors(const Vector4& normal, const Vector4& hover, const Vector4& pressed);

    /**
     * @brief Returns the normal background color.
     * @return Color.
     */
    const Vector4& GetBgNormal() const { return m_BgNormal; }

    /**
     * @brief Returns the hover background color.
     * @return Color.
     */
    const Vector4& GetBgHover() const { return m_BgHover; }

    /**
     * @brief Returns the pressed background color.
     * @return Color.
     */
    const Vector4& GetBgPressed() const { return m_BgPressed; }

    /**
     * @brief Sets the text color.
     * @param[in] color Text RGBA color.
     */
    void SetTextColor(const Vector4& color) { m_TextColor = color; }

    /**
     * @brief Returns the text color.
     * @return Color.
     */
    const Vector4& GetTextColor() const { return m_TextColor; }

    /**
     * @brief Sets text alignment inside the button.
     * @param[in] align Alignment.
     */
    void SetTextAlign(ButtonTextAlign align) { m_TextAlign = align; }

    /**
     * @brief Returns text alignment.
     * @return Alignment.
     */
    ButtonTextAlign GetTextAlign() const { return m_TextAlign; }

    /**
     * @brief Sets the click callback.
     * @param[in] callback Function invoked on click (release inside).
     */
    void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }

    /**
     * @brief Returns the current visual state.
     * @return ButtonState.
     */
    ButtonState GetState() const { return m_State; }

    /**
     * @brief Returns the minimum size for layout (text extent + padding).
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
     * @brief Called on pointer press.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Called on pointer release (may fire OnClick).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

private:
    std::string m_Text;                                     ///< Button text.
    Font* m_Font = nullptr;                                 ///< Font (not owned).
    std::function<void()> m_OnClick;                        ///< Click callback.
    ButtonState m_State = ButtonState::Normal;              ///< Current state.
    ButtonTextAlign m_TextAlign = ButtonTextAlign::Left;    ///< Text alignment.
    Vector4 m_BgNormal = {0.3f, 0.3f, 0.3f, 1.0f};            ///< Normal background.
    Vector4 m_BgHover = {0.4f, 0.4f, 0.4f, 1.0f};             ///< Hover background.
    Vector4 m_BgPressed = {0.2f, 0.2f, 0.2f, 1.0f};           ///< Pressed background.
    Vector4 m_TextColor = {1.0f, 1.0f, 1.0f, 1.0f};           ///< Text color.
};

} // namespace Leir
