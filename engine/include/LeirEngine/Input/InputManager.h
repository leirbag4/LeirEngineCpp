#pragma once

#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"

#include <unordered_map>

struct GLFWwindow;

namespace Leir {

enum class LEIR_API CursorStyle {
    Arrow = 0,
    Hand = 1,
    IBeam = 2,
    ResizeEW = 3,
    ResizeNS = 4,
};

class LEIR_API InputManager {
public:
    static InputManager& GetInstance();

    void Init(GLFWwindow* window);
    void AddWindow(GLFWwindow* window);  // register callbacks on an additional window
    void Update();
    void Shutdown();

    GLFWwindow* GetWindow() const { return m_Window; }

    // The primary window (the app's main window). Events from OTHER windows
    // (external/undocked) must not update the global polling state (Keyboard/
    // Mouse/Touch/Pointer), which tracks only the primary window — otherwise
    // moving the pointer over an external window would move the main editor's
    // mouse/hover state. nullptr = single-window apps (accept all events).
    static GLFWwindow* GetPrimaryWindow() { return GetInstance().m_Window; }

    // Effective logical scale (1.0 when HiDPI scaling is disabled). Set by
    // CoreApplication so cursor positions can be converted from the native
    // (physical on Windows) coordinate space to logical UI units.
    void SetContentScale(float scale) { m_ContentScale = scale; }
    void SetContentScaleForWindow(GLFWwindow* window, float scale);

    // OS cursor shape (cached, no-op if unchanged)
    static void SetCursorStyle(CursorStyle style);

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void RegisterCallbacks(GLFWwindow* window);

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CharCallback(GLFWwindow* window, unsigned int codepoint);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double x, double y);
    static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    Vector2 ToLogical(GLFWwindow* window, double x, double y) const;

    struct WindowState {
        Vector2 lastMousePos{ 0.0f, 0.0f };
        float contentScale = 1.0f;
    };

    WindowState& GetState(GLFWwindow* window);

    GLFWwindow* m_Window = nullptr;   // primary window (for polling)
    std::unordered_map<GLFWwindow*, WindowState> m_WindowStates;
    int m_CurrentCursor = -1;
    float m_ContentScale = 1.0f;      // legacy for the primary window
};

} // namespace Leir
