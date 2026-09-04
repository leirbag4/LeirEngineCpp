#pragma once

/**
 * @file UICanvas.h
 * @brief Root canvas: screen size, layout, hit-testing and input dispatch.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Input/InputEvent.h"
#include "LeirEngine/Input/EventQueue.h"

namespace Leir {

/**
 * @brief Root UI canvas: owns the element tree and drives layout and input.
 * @ingroup UI
 * @details Manages screen size, UpdateLayout, hit-testing (with clip mirroring)
 *  and EventQueue hooks (pointer/key/scroll dispatch, focus and capture).
 */
class LEIR_API UICanvas : public UIElement {
public:
    /**
     * @brief Constructs a canvas with default screen size.
     */
    UICanvas();

    /**
     * @brief Destroys the canvas.
     */
    ~UICanvas() override;

    /**
     * @brief Sets the logical screen size.
     * @param[in] width Screen width.
     * @param[in] height Screen height.
     */
    void SetScreenSize(float width, float height);

    /**
     * @brief Sets the native window this canvas receives input from.
     * @details Multi-window: events whose `window` field does not match are
     *  ignored. Nullptr (default) = accept all events (single-window apps).
     * @param[in] window Native window handle (GLFWwindow*).
     */
    void SetInputWindow(void* window) { m_InputWindow = window; }

    /**
     * @brief Returns the native window this canvas is bound to.
     * @return Native window handle or nullptr.
     */
    void* GetInputWindow() const { return m_InputWindow; }

    /**
     * @brief Returns screen width.
     * @return Width in logical pixels.
     */
    float GetScreenWidth() const { return m_ScreenWidth; }

    /**
     * @brief Returns screen height.
     * @return Height in logical pixels.
     */
    float GetScreenHeight() const { return m_ScreenHeight; }

    /**
     * @brief Recomputes layout for the whole tree.
     */
    void UpdateLayout();

    /**
     * @brief Hit-tests the tree at a screen position.
     * @param[in] screenPos Position in logical pixels.
     * @param[out] outElement Deepest hit element or nullptr.
     * @return True if hit.
     */
    bool HitTest(const Vector2& screenPos, UIElement*& outElement);

    /**
     * @brief Connects the canvas to the EventQueue input hooks.
     */
    void ConnectToInputSystem();

    /**
     * @brief Disconnects from the EventQueue.
     */
    void DisconnectFromInputSystem();

    /**
     * @brief Dispatches a pointer event (hit-test, hover, capture).
     * @param[in] e Pointer event.
     */
    void ProcessPointerEvent(const PointerEvent& e);

    /**
     * @brief Dispatches a scroll event to the hovered element.
     * @param[in] e Scroll event.
     */
    void ProcessScrollEvent(const ScrollEvent& e);

    /**
     * @brief Sets the focused element (sends OnBlur/OnFocus).
     * @param[in] element Element to focus or nullptr to clear.
     */
    void SetFocus(UIElement* element);

    /**
     * @brief Returns the focused element.
     * @return Focused element or nullptr.
     */
    UIElement* GetFocus() const { return m_FocusElement; }

    /**
     * @brief Clears focus.
     */
    void ClearFocus() { SetFocus(nullptr); }

    /**
     * @brief Sends text input to the focused element.
     * @param[in] codepoint Unicode codepoint.
     */
    void SendTextInput(uint32_t codepoint);

    /**
     * @brief Sends key down to the focused element.
     * @param[in] key Key code.
     */
    void SendKeyDown(int key);

    /**
     * @brief Returns the hovered element.
     * @return Hovered element or nullptr.
     */
    UIElement* GetHoveredElement() const { return m_HoveredElement; }

    /**
     * @brief Drops hover/focus state without firing exit/blur callbacks.
     * @details Safe to call after an element has been deleted.
     */
    void ClearHoverAndFocus() { m_HoveredElement = nullptr; m_FocusElement = nullptr; }

    /**
     * @brief Notifies that the pointer left the canvas/window.
     * @details Fires OnPointerExit + SetHovered(false) on the hovered element,
     *  then clears hover/focus. Used by external windows on GLFW cursor-leave so
     *  a detached viewport stops reporting hover once the mouse leaves its
     *  window (otherwise its stale hovered element keeps the editor's
     *  inViewport true and picking runs with the main window's mouse position).
     */
    void NotifyPointerLeave();

    /**
     * @brief Captures pointer (all move/release go to the element).
     * @param[in] element Element to capture.
     */
    void CapturePointer(UIElement* element) { m_CaptureElement = element; }

    /**
     * @brief Releases pointer capture.
     */
    void ReleasePointer() { m_CaptureElement = nullptr; }

    /**
     * @brief Returns the capture element.
     * @return Capture element or nullptr.
     */
    UIElement* GetCaptureElement() const { return m_CaptureElement; }

private:
    float m_ScreenWidth = 1280.0f;                  ///< Screen width (logical).
    float m_ScreenHeight = 720.0f;                  ///< Screen height (logical).
    UIElement* m_FocusElement = nullptr;            ///< Focused element.
    UIElement* m_HoveredElement = nullptr;          ///< Hovered element.
    UIElement* m_CaptureElement = nullptr;          ///< Pointer capture element.
    void* m_InputWindow = nullptr;                  ///< Native window handle for multi-window filtering.
    EventQueue::HookId m_HookTokens[4] = {0,0,0,0};///< Hook tokens for removal.

    void HitTestRecursive(UIElement* element, const Vector2& pos, UIElement*& out,
                          const Vector4* clip);
    bool m_PointerDown = false;                     ///< Pointer down flag.
};

} // namespace Leir
