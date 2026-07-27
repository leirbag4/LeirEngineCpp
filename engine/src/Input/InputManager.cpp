#include "LeirEngine/Input/InputManager.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Leir {

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

void InputManager::Init(GLFWwindow* window)
{
    m_Window = window;
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void InputManager::Shutdown()
{
    m_Window = nullptr;
    m_CurrentKeys.clear();
    m_PreviousKeys.clear();
    m_CurrentMouse.clear();
    m_PreviousMouse.clear();
}

void InputManager::Update()
{
    m_PreviousKeys = m_CurrentKeys;
    m_PreviousMouse = m_CurrentMouse;

    double x, y;
    glfwGetCursorPos(m_Window, &x, &y);
    glm::vec2 newPos{static_cast<float>(x), static_cast<float>(y)};
    m_MouseDelta = newPos - m_MousePosition;
    m_MousePosition = newPos;

    m_ScrollDelta = 0.0f;
}

bool InputManager::IsKeyDown(KeyCode key) const
{
    auto it = m_CurrentKeys.find(static_cast<int32_t>(key));
    return it != m_CurrentKeys.end() && it->second;
}

bool InputManager::IsKeyPressed(KeyCode key) const
{
    int32_t k = static_cast<int32_t>(key);
    auto cur = m_CurrentKeys.find(k);
    auto prev = m_PreviousKeys.find(k);
    bool curDown = cur != m_CurrentKeys.end() && cur->second;
    bool prevDown = prev != m_PreviousKeys.end() && prev->second;
    return curDown && !prevDown;
}

bool InputManager::IsKeyReleased(KeyCode key) const
{
    int32_t k = static_cast<int32_t>(key);
    auto cur = m_CurrentKeys.find(k);
    auto prev = m_PreviousKeys.find(k);
    bool curDown = cur != m_CurrentKeys.end() && cur->second;
    bool prevDown = prev != m_PreviousKeys.end() && prev->second;
    return !curDown && prevDown;
}

bool InputManager::IsMouseButtonDown(MouseButton button) const
{
    auto it = m_CurrentMouse.find(static_cast<int32_t>(button));
    return it != m_CurrentMouse.end() && it->second;
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const
{
    int32_t b = static_cast<int32_t>(button);
    auto cur = m_CurrentMouse.find(b);
    auto prev = m_PreviousMouse.find(b);
    bool curDown = cur != m_CurrentMouse.end() && cur->second;
    bool prevDown = prev != m_PreviousMouse.end() && prev->second;
    return curDown && !prevDown;
}

bool InputManager::IsMouseButtonReleased(MouseButton button) const
{
    int32_t b = static_cast<int32_t>(button);
    auto cur = m_CurrentMouse.find(b);
    auto prev = m_PreviousMouse.find(b);
    bool curDown = cur != m_CurrentMouse.end() && cur->second;
    bool prevDown = prev != m_PreviousMouse.end() && prev->second;
    return !curDown && prevDown;
}

glm::vec2 InputManager::GetMousePosition() const { return m_MousePosition; }
glm::vec2 InputManager::GetMouseDelta() const { return m_MouseDelta; }
float InputManager::GetScrollDelta() const { return m_ScrollDelta; }

// --- Callbacks ---

void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    auto& inst = GetInstance();
    if (action == GLFW_PRESS)
        inst.m_CurrentKeys[key] = true;
    else if (action == GLFW_RELEASE)
        inst.m_CurrentKeys[key] = false;
}

void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;
    auto& inst = GetInstance();
    if (action == GLFW_PRESS)
        inst.m_CurrentMouse[button] = true;
    else if (action == GLFW_RELEASE)
        inst.m_CurrentMouse[button] = false;
}

void InputManager::ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    (void)xOffset;
    auto& inst = GetInstance();
    inst.m_ScrollDelta = static_cast<float>(yOffset);
}

} // namespace Leir
