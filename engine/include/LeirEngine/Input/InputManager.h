#pragma once

#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"

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
    void Shutdown();
    void Update();

    GLFWwindow* GetWindow() const { return m_Window; }

    // OS cursor shape (cached, no-op if unchanged)
    static void SetCursorStyle(CursorStyle style);

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CharCallback(GLFWwindow* window, unsigned int codepoint);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double x, double y);
    static void ScrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* m_Window = nullptr;
    Vector2 m_LastMousePos{ 0.0f, 0.0f };
    int m_CurrentCursor = -1;
};

} // namespace Leir
