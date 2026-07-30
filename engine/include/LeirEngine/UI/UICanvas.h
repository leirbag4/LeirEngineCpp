#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Input/InputEvent.h"

namespace Leir {

class LEIR_API UICanvas : public UIElement {
public:
    UICanvas();
    ~UICanvas() override;

    void SetScreenSize(float width, float height);
    float GetScreenWidth() const { return m_ScreenWidth; }
    float GetScreenHeight() const { return m_ScreenHeight; }

    void UpdateLayout();

    bool HitTest(const Vector2& screenPos, UIElement*& outElement);

    // Connect to EventQueue for automatic input dispatch
    void ConnectToInputSystem();
    void DisconnectFromInputSystem();

    // Input dispatch
    void ProcessPointerEvent(const PointerEvent& e);
    void SetFocus(UIElement* element);
    UIElement* GetFocus() const { return m_FocusElement; }
    void ClearFocus() { SetFocus(nullptr); }
    void SendTextInput(uint32_t codepoint);
    void SendKeyDown(int key);

    UIElement* GetHoveredElement() const { return m_HoveredElement; }

    void CapturePointer(UIElement* element) { m_CaptureElement = element; }
    void ReleasePointer() { m_CaptureElement = nullptr; }
    UIElement* GetCaptureElement() const { return m_CaptureElement; }

private:
    float m_ScreenWidth = 1280.0f;
    float m_ScreenHeight = 720.0f;
    UIElement* m_FocusElement = nullptr;
    UIElement* m_HoveredElement = nullptr;
    UIElement* m_CaptureElement = nullptr;

    void HitTestRecursive(UIElement* element, const Vector2& pos, UIElement*& out);
    bool m_PointerDown = false;
};

} // namespace Leir
