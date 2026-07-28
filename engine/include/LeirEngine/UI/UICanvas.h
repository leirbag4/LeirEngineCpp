#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"

namespace Leir {

class LEIR_API UICanvas : public UIElement {
public:
    UICanvas();
    ~UICanvas() override;

    void SetScreenSize(float width, float height);
    float GetScreenWidth() const { return m_ScreenWidth; }
    float GetScreenHeight() const { return m_ScreenHeight; }

    void UpdateLayout();

    bool HitTest(const glm::vec2& screenPos, UIElement*& outElement);

    // Input dispatch (call every frame)
    void UpdatePointer(const glm::vec2& screenPos, bool pointerDown, bool pointerUp);
    void SetFocus(UIElement* element);
    UIElement* GetFocus() const { return m_FocusElement; }
    void ClearFocus() { SetFocus(nullptr); }
    void SendTextInput(uint32_t codepoint);
    void SendKeyDown(int key);

private:
    float m_ScreenWidth = 1280.0f;
    float m_ScreenHeight = 720.0f;
    UIElement* m_FocusElement = nullptr;
    UIElement* m_HoveredElement = nullptr;

    void HitTestRecursive(UIElement* element, const glm::vec2& pos, UIElement*& out);
};

} // namespace Leir
