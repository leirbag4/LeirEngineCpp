#pragma once

#include "LeirEngine/Input/Key.h"
#include "LeirEngine/Input/PointerButton.h"
#include "LeirEngine/Core/Export.h"
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Leir {

class LEIR_API InputManager {
public:
    static InputManager& GetInstance();

    void Init(GLFWwindow* window);
    void Shutdown();
    void Update();

    GLFWwindow* GetWindow() const { return m_Window; }

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
    glm::vec2 m_LastMousePos{ 0.0f };
};

} // namespace Leir
