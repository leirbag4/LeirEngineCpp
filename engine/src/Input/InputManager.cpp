#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Touch.h"
#include "LeirEngine/Input/Pointer.h"

#if !defined(__EMSCRIPTEN__)
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>
#include <unordered_map>

namespace Leir {

InputManager& InputManager::GetInstance()
{
    static InputManager instance;
    return instance;
}

void InputManager::RegisterCallbacks(GLFWwindow* window)
{
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCharCallback(window, CharCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void InputManager::Init(GLFWwindow* window)
{
    m_Window = window;

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    auto& st = GetState(window);
    st.lastMousePos = ToLogical(window, x, y);
    st.contentScale = m_ContentScale;

    RegisterCallbacks(window);
}

void InputManager::AddWindow(GLFWwindow* window)
{
    if (!window || m_Window == window)
        return;
    // Register callbacks on the additional window (mouse state is created on demand).
    RegisterCallbacks(window);
}

void InputManager::Shutdown()
{
    m_Window = nullptr;
    m_WindowStates.clear();
}

void InputManager::SetContentScaleForWindow(GLFWwindow* window, float scale)
{
    if (!window) {
        m_ContentScale = scale;
        return;
    }
    GetState(window).contentScale = scale;
}

InputManager::WindowState& InputManager::GetState(GLFWwindow* window)
{
    auto it = m_WindowStates.find(window);
    if (it == m_WindowStates.end()) {
        it = m_WindowStates.emplace(window, WindowState{}).first;
        it->second.contentScale = m_ContentScale;
    }
    return it->second;
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
    EventAction evAction = EventAction::Press;
    if (action == GLFW_RELEASE) evAction = EventAction::Release;
    else if (action == GLFW_REPEAT) evAction = EventAction::Repeat;

    KeyEvent e;
    e.key = static_cast<Key>(key);
    e.scancode = scancode;
    e.action = evAction;
    e.mods = mods;
    e.window = window;

    EventQueue::Get().Push(e);
}

void InputManager::CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    CharEvent e;
    e.codepoint = codepoint;
    e.window = window;
    EventQueue::Get().Push(e);
}

void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
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
    auto& st = inst.GetState(window);
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    Vector2 pos = inst.ToLogical(window, x, y);

    PointerEvent e;
    e.source = PointerSource::Mouse;
    e.pointerId = 0;
    e.position = pos;
    e.delta = pos - st.lastMousePos;
    e.button = btn;
    e.action = (action == GLFW_PRESS) ? EventAction::Press : EventAction::Release;
    e.pressure = 1.0f;
    e.window = window;

    EventQueue::Get().Push(e);
}

void InputManager::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    auto& inst = GetInstance();
    auto& st = inst.GetState(window);
    Vector2 newPos = inst.ToLogical(window, x, y);

    PointerEvent e;
    e.source = PointerSource::Mouse;
    e.pointerId = 0;
    e.position = newPos;
    e.delta = newPos - st.lastMousePos;
    e.button = PointerButton::None;
    e.action = EventAction::Move;
    e.pressure = 1.0f;
    e.window = window;

    st.lastMousePos = newPos;
    EventQueue::Get().Push(e);
}

Vector2 InputManager::ToLogical(GLFWwindow* window, double x, double y) const
{
    Vector2 pos{ static_cast<float>(x), static_cast<float>(y) };
#ifdef _WIN32
    // On Windows a DPI-aware process receives cursor positions in physical
    // pixels (like the window size). Convert to logical UI units. On
    // macOS/Linux GLFW already reports logical units, so this is a no-op.
    auto it = m_WindowStates.find(window);
    float scale = (it != m_WindowStates.end()) ? it->second.contentScale : m_ContentScale;
    if (scale > 0.0f)
        pos /= scale;
#endif
    return pos;
}

void InputManager::ScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    ScrollEvent e;
    e.offset = { static_cast<float>(xOffset), static_cast<float>(yOffset) };
    e.window = window;
    EventQueue::Get().Push(e);
}

} // namespace Leir