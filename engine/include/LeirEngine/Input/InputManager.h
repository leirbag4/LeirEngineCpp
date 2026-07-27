#pragma once

#include "LeirEngine/Core/Export.h"

#include <glm/glm.hpp>
#include <unordered_map>

struct GLFWwindow;

namespace Leir {

enum class KeyCode : int32_t;
enum class MouseButton : int32_t;

class LEIR_API InputManager {
public:
    static InputManager& GetInstance();

    void Init(GLFWwindow* window);
    void Shutdown();
    void Update();

    // Keyboard
    bool IsKeyDown(KeyCode key) const;
    bool IsKeyPressed(KeyCode key) const;
    bool IsKeyReleased(KeyCode key) const;

    // Mouse
    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;
    glm::vec2 GetMousePosition() const;
    glm::vec2 GetMouseDelta() const;
    float GetScrollDelta() const;

    // Raw access
    GLFWwindow* GetWindow() const { return m_Window; }

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* m_Window = nullptr;

    // Current + previous frame state for edge detection
    std::unordered_map<int32_t, bool> m_CurrentKeys;
    std::unordered_map<int32_t, bool> m_PreviousKeys;
    std::unordered_map<int32_t, bool> m_CurrentMouse;
    std::unordered_map<int32_t, bool> m_PreviousMouse;

    glm::vec2 m_MousePosition{0.0f};
    glm::vec2 m_MouseDelta{0.0f};
    float m_ScrollDelta = 0.0f;
};

// Key code enum matching GLFW constants
enum class KeyCode : int32_t {
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,
    Alpha0 = 48, Alpha1 = 49, Alpha2 = 50, Alpha3 = 51, Alpha4 = 52,
    Alpha5 = 53, Alpha6 = 54, Alpha7 = 55, Alpha8 = 56, Alpha9 = 57,
    Semicolon = 59,
    Equal = 61,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72,
    I = 73, J = 74, K = 75, L = 76, M = 77, N = 78, O = 79, P = 80,
    Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
    Y = 89, Z = 90,
    LeftBracket = 91, Backslash = 92, RightBracket = 93,
    GraveAccent = 96,
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260,
    Delete = 261, Right = 262, Left = 263, Down = 264, Up = 265,
    PageUp = 266, PageDown = 267, Home = 268, End = 269,
    CapsLock = 280, ScrollLock = 281, NumLock = 282, PrintScreen = 283,
    Pause = 284,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    NumPad0 = 320, NumPad1 = 321, NumPad2 = 322, NumPad3 = 323,
    NumPad4 = 324, NumPad5 = 325, NumPad6 = 326, NumPad7 = 327,
    NumPad8 = 328, NumPad9 = 329,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
    RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347
};

enum class MouseButton : int32_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7
};

} // namespace Leir
