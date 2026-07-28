#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>
#include <glm/glm.hpp>
#include <string>

namespace Leir {

class Font;

enum class LEIR_API ButtonState {
    Normal,
    Hovered,
    Pressed,
};

class LEIR_API UIButton : public UIElement {
public:
    UIButton();
    ~UIButton() override;

    void SetText(const std::string& text) { m_Text = text; }
    const std::string& GetText() const { return m_Text; }

    void SetFont(Font* font) { m_Font = font; }
    Font* GetFont() const { return m_Font; }

    void SetColors(const glm::vec4& normal, const glm::vec4& hover, const glm::vec4& pressed);
    const glm::vec4& GetBgNormal() const { return m_BgNormal; }
    const glm::vec4& GetBgHover() const { return m_BgHover; }
    const glm::vec4& GetBgPressed() const { return m_BgPressed; }

    void SetTextColor(const glm::vec4& color) { m_TextColor = color; }
    const glm::vec4& GetTextColor() const { return m_TextColor; }

    void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }

    ButtonState GetState() const { return m_State; }

    glm::vec2 GetMinSize() const override;

    void OnPointerEnter(const glm::vec2& pos) override;
    void OnPointerExit() override;
    bool OnPointerDown(const glm::vec2& pos) override;
    bool OnPointerUp(const glm::vec2& pos) override;

private:
    std::string m_Text;
    Font* m_Font = nullptr;
    std::function<void()> m_OnClick;
    ButtonState m_State = ButtonState::Normal;
    glm::vec4 m_BgNormal = {0.3f, 0.3f, 0.3f, 1.0f};
    glm::vec4 m_BgHover = {0.4f, 0.4f, 0.4f, 1.0f};
    glm::vec4 m_BgPressed = {0.2f, 0.2f, 0.2f, 1.0f};
    glm::vec4 m_TextColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

} // namespace Leir
