#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>
#include <string>

namespace Leir {

class Font;

class LEIR_API UITextInput : public UIElement {
public:
    UITextInput();
    ~UITextInput() override;

    void SetText(const std::string& text);
    const std::string& GetText() const { return m_Text; }

    void SetFont(Font* font) { m_Font = font; }
    Font* GetFont() const { return m_Font; }

    void SetPlaceholder(const std::string& text) { m_Placeholder = text; }
    const std::string& GetPlaceholder() const { return m_Placeholder; }

    void SetOnChange(std::function<void(const std::string&)> cb) { m_OnChange = cb; }

    glm::vec2 GetMinSize() const override;

    void OnPointerEnter(const glm::vec2& pos) override;
    void OnPointerExit() override;
    bool OnPointerDown(const glm::vec2& pos) override;
    void OnPointerMove(const glm::vec2& pos) override;
    bool OnTextInput(uint32_t codepoint) override;
    void OnFocus() override;
    void OnBlur() override;

protected:
    void InsertChar(uint32_t codepoint);
    void DeleteChar();
    void UpdateCursorPos();

    std::string m_Text;
    Font* m_Font = nullptr;
    std::function<void(const std::string&)> m_OnChange;
    int m_CursorPos = 0;
    bool m_Focused = false;
    bool m_Hovered = false;

private:
    std::string m_Placeholder;
};

} // namespace Leir
