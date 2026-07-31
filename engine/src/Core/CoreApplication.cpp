#include "LeirEngine/Core/CoreApplication.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Scene/SceneManager.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <cstdlib>

namespace Leir {

CoreApplication::CoreApplication(const char* title, int width, int height, bool fullscreen,
                                 int posX, int posY, bool maximized)
    : m_Width(width)
    , m_Height(height)
{
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        std::exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_Window = glfwCreateWindow(width, height, title, monitor, nullptr);
    if (!m_Window) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }

    // Restore saved placement (only meaningful for windowed mode)
    if (!fullscreen) {
        if (posX != INT_MIN && posY != INT_MIN)
            glfwSetWindowPos(m_Window, posX, posY);
        else
            CenterWindow();
        if (maximized)
            glfwMaximizeWindow(m_Window);
    }

    spdlog::info("GLFW window created ({}x{})", width, height);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(m_Window, &fbWidth, &fbHeight);
    if (fbWidth > 0 && fbHeight > 0) {
        m_Width = fbWidth;
        m_Height = fbHeight;
    }

    // Seed the tracked normal rect with the restored (windowed) values. In
    // fullscreen this stays unset so a later save never overwrites the saved
    // windowed rect with monitor size.
    if (!fullscreen) {
        int x, y, w, h;
        glfwGetWindowPos(m_Window, &x, &y);
        glfwGetWindowSize(m_Window, &w, &h);
        if (maximized) {
            // Window size already reflects the maximized size; keep the
            // requested restored size instead.
            w = width;
            h = height;
        }
        m_NormalX = x;
        m_NormalY = y;
        m_NormalW = w;
        m_NormalH = h;
        m_HasNormalRect = true;
    }

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetWindowSizeCallback(m_Window, WindowSizeCallback);
    glfwSetWindowPosCallback(m_Window, WindowPosCallback);

    InputManager::GetInstance().Init(m_Window);
}

CoreApplication::~CoreApplication()
{
    InputManager::GetInstance().Shutdown();
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void CoreApplication::Run()
{
    m_Running = true;

    OnInit();

    double lastTime = glfwGetTime();
    while (m_Running && !glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        EventQueue::Get().Process();

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        if (auto* scene = SceneManager::GetInstance().GetActiveScene())
            scene->OnUpdate(deltaTime);

        OnUpdate(deltaTime);

        InputManager::GetInstance().Update();

        OnRender();
    }

    OnShutdown();
}

void CoreApplication::Quit()
{
    m_Running = false;
}

void CoreApplication::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* app = static_cast<CoreApplication*>(glfwGetWindowUserPointer(window));
    if (app)
        app->HandleWindowResize(width, height);
}

void CoreApplication::HandleWindowResize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    m_Width = width;
    m_Height = height;
    OnWindowResized(width, height);
}

void CoreApplication::GetWindowPosition(int& x, int& y) const
{
    x = 0;
    y = 0;
    if (m_Window)
        glfwGetWindowPos(m_Window, &x, &y);
}

void CoreApplication::GetWindowSize(int& w, int& h) const
{
    w = m_Width;
    h = m_Height;
    if (m_Window)
        glfwGetWindowSize(m_Window, &w, &h);
}

bool CoreApplication::IsMaximized() const
{
    return m_Window && glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
}

bool CoreApplication::GetNormalWindowRect(int& x, int& y, int& w, int& h) const
{
    if (!m_HasNormalRect)
        return false;
    x = m_NormalX;
    y = m_NormalY;
    w = m_NormalW;
    h = m_NormalH;
    return true;
}

void CoreApplication::WindowSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* app = static_cast<CoreApplication*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    app->UpdateNormalRect(x, y, width, height);
}

void CoreApplication::WindowPosCallback(GLFWwindow* window, int x, int y)
{
    auto* app = static_cast<CoreApplication*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    app->UpdateNormalRect(x, y, w, h);
}

void CoreApplication::UpdateNormalRect(int x, int y, int w, int h)
{
    if (!m_Window)
        return;
    if (glfwGetWindowMonitor(m_Window) != nullptr)
        return; // fullscreen: keep last windowed rect
    if (glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED))
        return; // maximized: only track restored size/pos
    m_NormalX = x;
    m_NormalY = y;
    m_NormalW = w;
    m_NormalH = h;
    m_HasNormalRect = true;
}

void CoreApplication::CenterWindow()
{
    if (!m_Window)
        return;
    int vx, vy, vw, vh;
    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &vx, &vy, &vw, &vh);
    int w, h;
    glfwGetWindowSize(m_Window, &w, &h);
    glfwSetWindowPos(m_Window, vx + (vw - w) / 2, vy + (vh - h) / 2);
}

} // namespace Leir
