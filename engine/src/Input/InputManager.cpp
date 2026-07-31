#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Touch.h"
#include "LeirEngine/Input/Pointer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace Leir {

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

void InputManager::Init(GLFWwindow* window)
{
    m_Window = window;

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_LastMousePos = { static_cast<float>(x), static_cast<float>(y) };

    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCharCallback(window, CharCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void InputManager::Shutdown()
{
    m_Window = nullptr;
}

void InputManager::SetCursorStyle(CursorStyle style)
{
    auto& im = GetInstance();
    if (!im.m_Window) return;
    if (im.m_CurrentCursor == (int)style) return;

    int shape = GLFW_ARROW_CURSOR;
    switch (style) {
        case CursorStyle::Arrow:    shape = GLFW_ARROW_CURSOR; break;
        case CursorStyle::Hand:     shape = GLFW_HAND_CURSOR; break;
        case CursorStyle::IBeam:    shape = GLFW_IBEAM_CURSOR; break;
        case CursorStyle::ResizeEW: shape = GLFW_RESIZE_EW_CURSOR; break;
        case CursorStyle::ResizeNS: shape = GLFW_RESIZE_NS_CURSOR; break;
    }

    GLFWcursor* cursor = nullptr;
    if (shape != GLFW_ARROW_CURSOR) {
        static std::unordered_map<int, GLFWcursor*> sCursors;
        auto it = sCursors.find(shape);
        if (it == sCursors.end()) {
            cursor = glfwCreateStandardCursor(shape);
            sCursors[shape] = cursor;
        } else {
            cursor = it->second;
        }
    }
    glfwSetCursor(im.m_Window, cursor);
    im.m_CurrentCursor = (int)style;
}

void InputManager::Update()
{
    // Reset frame state for all polling classes
    Keyboard::ResetFrame();
    Mouse::ResetFrame();
    Touch::ResetFrame();
    Pointer::ResetFrame();
}

// --- Callbacks ---

void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)window;
    EventAction evAction = EventAction::Press;
    if (action == GLFW_RELEASE) evAction = EventAction::Release;
    else if (action == GLFW_REPEAT) evAction = EventAction::Repeat;

    KeyEvent e;
    e.key = static_cast<Key>(key);
    e.scancode = scancode;
    e.action = evAction;
    e.mods = mods;

    EventQueue::Get().Push(e);
}

void InputManager::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    (void)window;
    CharEvent e;
    e.codepoint = codepoint;
    EventQueue::Get().Push(e);
}

void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)window;
    (void)mods;

    PointerButton btn = PointerButton::None;
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:   btn = PointerButton::Left;   break;
        case GLFW_MOUSE_BUTTON_RIGHT:  btn = PointerButton::Right;  break;
        case GLFW_MOUSE_BUTTON_MIDDLE: btn = PointerButton::Middle; break;
        case GLFW_MOUSE_BUTTON_4:      btn = PointerButton::Extra1; break;
        case GLFW_MOUSE_BUTTON_5:      btn = PointerButton::Extra2; break;
        default: return;
    }

    auto& inst = GetInstance();
    double x, y;
    glfwGetCursorPos(inst.m_Window, &x, &y);

    PointerEvent e;
    e.source = PointerSource::Mouse;
    e.pointerId = 0;
    e.position = { static_cast<float>(x), static_cast<float>(y) };
    e.delta = e.position - inst.m_LastMousePos;
    e.button = btn;
    e.action = (action == GLFW_PRESS) ? EventAction::Press : EventAction::Release;
    e.pressure = 1.0f;

    EventQueue::Get().Push(e);
}

void InputManager::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& inst = GetInstance();
    Vector2 newPos{ static_cast<float>(x), static_cast<float>(y) };

    PointerEvent e;
    e.source = PointerSource::Mouse;
    e.pointerId = 0;
    e.position = newPos;
    e.delta = newPos - inst.m_LastMousePos;
    e.button = PointerButton::None;
    e.action = EventAction::Move;
    e.pressure = 1.0f;

    inst.m_LastMousePos = newPos;
    EventQueue::Get().Push(e);
}

void InputManager::ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    (void)window;

    ScrollEvent e;
    e.offset = { static_cast<float>(xOffset), static_cast<float>(yOffset) };
    EventQueue::Get().Push(e);
}

} // namespace Leir
