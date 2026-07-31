#include "LeirEngine/Core/CoreApplication.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Scene/SceneManager.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef _WIN32
namespace {
using DpiAwarenessContext = void*;
using DpiAwareness = int; // 0=unaware 1=system 2=per-monitor
}
extern "C" {
__declspec(dllimport) DpiAwarenessContext __stdcall GetThreadDpiAwarenessContext(void);
__declspec(dllimport) DpiAwareness __stdcall GetAwarenessFromDpiAwarenessContext(DpiAwarenessContext);
}
#endif

namespace Leir {

CoreApplication::CoreApplication(const char* title, int width, int height, bool fullscreen,
                                 int posX, int posY, bool maximized, bool hidpi)
    : m_Width(width)
    , m_Height(height)
    , m_HidpiEnabled(hidpi)
{
    if (!glfwInit()) {
        spdlog::critical("Failed to initialize GLFW");
        std::exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // Content scale of the primary monitor, known before the window exists.
    // The window is created in physical pixels; when HiDPI scaling is enabled
    // the requested logical size is scaled so the window occupies the same
    // logical area regardless of the system DPI.
    float primaryScale = 1.0f;
    if (GLFWmonitor* primary = glfwGetPrimaryMonitor()) {
        float sx, sy;
        glfwGetMonitorContentScale(primary, &sx, &sy);
        primaryScale = std::max(sx, sy);
    }
    m_ContentScale = primaryScale;

    int createW = width;
    int createH = height;
    if (m_HidpiEnabled) {
        createW = (int)std::lround(width * primaryScale);
        createH = (int)std::lround(height * primaryScale);
    }

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_Window = glfwCreateWindow(createW, createH, title, monitor, nullptr);
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

    spdlog::info("GLFW window created ({}x{} physical)", createW, createH);

#ifdef _WIN32
    {
        // Diagnostic: report the actual DPI awareness context (Paso 0 HiDPI)
        DpiAwareness awareness = GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
        spdlog::info("DPI awareness: {}", (int)awareness);
    }
#endif

    // Refresh the actual per-monitor content scale now that the window exists.
    float csx, csy;
    glfwGetWindowContentScale(m_Window, &csx, &csy);
    m_ContentScale = std::max(csx, csy);

    // Physical framebuffer size.
    glfwGetFramebufferSize(m_Window, &m_FbWidth, &m_FbHeight);

    // Logical size = framebuffer / content scale. This is uniform across
    // platforms: on Windows GLFW reports window==framebuffer in physical
    // pixels for a DPI-aware process; on macOS/Linux window size is logical
    // while framebuffer is physical.
    float effScale = GetContentScale();
    m_Width = (int)std::lround(m_FbWidth / effScale);
    m_Height = (int)std::lround(m_FbHeight / effScale);

    spdlog::info("Window sizes: logical {}x{}, framebuffer {}x{}, contentScale {:.2f}",
        m_Width, m_Height, m_FbWidth, m_FbHeight, m_ContentScale);

    // Seed the tracked normal rect with the restored (windowed) values. In
    // fullscreen this stays unset so a later save never overwrites the saved
    // windowed rect with monitor size.
    if (!fullscreen) {
        int x, y;
        glfwGetWindowPos(m_Window, &x, &y);
        if (maximized) {
            // Window size already reflects the maximized size; keep the
            // requested restored size instead.
            m_Width = width;
            m_Height = height;
        }
        UpdateNormalRect(x, y, m_Width, m_Height);
    }

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);
    glfwSetWindowSizeCallback(m_Window, WindowSizeCallback);
    glfwSetWindowPosCallback(m_Window, WindowPosCallback);
    glfwSetWindowContentScaleCallback(m_Window, WindowContentScaleCallback);

    // Set the effective scale before Init so the seeded mouse position is
    // already in logical units.
    InputManager::GetInstance().SetContentScale(GetContentScale());
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
    m_FbWidth = width;
    m_FbHeight = height;
    // Framebuffer is always physical; derive the logical size from it.
    float scale = GetContentScale();
    m_Width = (int)std::lround(width / scale);
    m_Height = (int)std::lround(height / scale);
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
    // Window-size units differ per platform (physical on Windows for a
    // DPI-aware process, logical on macOS/Linux). Normalize to logical.
    app->UpdateNormalRect(x, y, app->ToLogical(width), app->ToLogical(height));
}

void CoreApplication::WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale)
{
    auto* app = static_cast<CoreApplication*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    app->m_ContentScale = std::max(xscale, yscale);
    InputManager::GetInstance().SetContentScale(app->GetContentScale());
    app->OnContentScaleChanged();
}

void CoreApplication::WindowPosCallback(GLFWwindow* window, int x, int y)
{
    auto* app = static_cast<CoreApplication*>(glfwGetWindowUserPointer(window));
    if (!app)
        return;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    app->UpdateNormalRect(x, y, app->ToLogical(w), app->ToLogical(h));
}

int CoreApplication::ToLogical(int nativeSize) const
{
#ifdef _WIN32
    // On Windows GLFW reports window/framebuffer sizes in physical pixels for
    // a DPI-aware process; logical = physical / scale.
    float scale = m_ContentScale > 0.0f ? m_ContentScale : 1.0f;
    return (int)std::lround(nativeSize / scale);
#else
    // macOS/Linux already report window size in logical units.
    return nativeSize;
#endif
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
    // Monitor workarea and window position/size share the same units on each
    // platform (physical pixels on Windows for a DPI-aware process, logical
    // units on macOS/Linux), so centering needs no unit conversion.
    int vx, vy, vw, vh;
    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &vx, &vy, &vw, &vh);
    int w, h;
    glfwGetWindowSize(m_Window, &w, &h);
    glfwSetWindowPos(m_Window, vx + (vw - w) / 2, vy + (vh - h) / 2);
}

} // namespace Leir
