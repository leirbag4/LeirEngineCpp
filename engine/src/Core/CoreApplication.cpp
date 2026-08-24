#include "LeirEngine/Core/CoreApplication.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Scene/SceneManager.h"

#if !defined(__EMSCRIPTEN__)
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include "LeirEngine/Core/Log.h"
#include <stb_image.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

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
        XConsole::PrintError("Failed to initialize GLFW");
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
    // Defensive: a corrupted settings save (window width/height 0) must never
    // brick the launch — glfwCreateWindow(0, 0) fails.
    createW = std::max(createW, 1);
    createH = std::max(createH, 1);

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_Window = glfwCreateWindow(createW, createH, title, monitor, nullptr);
    if (!m_Window) {
        XConsole::PrintError("Failed to create GLFW window");
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

    XConsole::Println("GLFW window created ({}x{} physical)", createW, createH);

#ifdef _WIN32
    {
        // Diagnostic: report the actual DPI awareness context (Paso 0 HiDPI)
        DpiAwareness awareness = GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext());
        XConsole::Println("DPI awareness: {}", (int)awareness);
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

    XConsole::Println("Window sizes: logical {}x{}, framebuffer {}x{}, contentScale {:.2f}",
        m_Width, m_Height, m_FbWidth, m_FbHeight, m_ContentScale);

    // Seed the tracked normal rect with the restored (windowed) values. In
    // fullscreen this stays unset so a later save never overwrites the saved
    // windowed rect with monitor size.
    if (!fullscreen) {
        int x, y;
        glfwGetWindowPos(m_Window, &x, &y);
        // UpdateNormalRect discards the maximized state (keeps the last saved
        // windowed rect), so m_Width/m_Height retain the actual logical size
        // (maximized) instead of the restored size.
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
    auto t0 = std::chrono::steady_clock::now();
    InputManager::GetInstance().Shutdown();
    auto t1 = std::chrono::steady_clock::now();
    glfwDestroyWindow(m_Window);
    auto t2 = std::chrono::steady_clock::now();
    glfwTerminate();
    auto t3 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] CoreApplication dtor: InputManager {:.1f} ms, glfwDestroyWindow {:.1f} ms, glfwTerminate {:.1f} ms",
        std::chrono::duration<double, std::milli>(t1 - t0).count(),
        std::chrono::duration<double, std::milli>(t2 - t1).count(),
        std::chrono::duration<double, std::milli>(t3 - t2).count());
}

void CoreApplication::Run()
{
    m_Running = true;

    OnInit();

#if defined(__EMSCRIPTEN__)
    m_LastFrameTime = glfwGetTime();
    emscripten_set_main_loop_arg(&CoreApplication::FrameThunk, this, 0, true);
    return;
#else
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

    auto tShutdown0 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] main loop exited (running={} shouldClose={})",
        m_Running ? 1 : 0, glfwWindowShouldClose(m_Window) ? 1 : 0);
    OnShutdown();
    auto tShutdown1 = std::chrono::steady_clock::now();
    XConsole::Debug("[Timing] OnShutdown() completed in {:.1f} ms",
        std::chrono::duration<double, std::milli>(tShutdown1 - tShutdown0).count());
#endif
}

void CoreApplication::Quit()
{
    m_Running = false;
}

void CoreApplication::SetWindowTitle(const char* title)
{
    if (m_Window)
        glfwSetWindowTitle(m_Window, title);
}

void CoreApplication::SetWindowIcon(const char* pngPath)
{
    if (!m_Window || !pngPath)
        return;
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(pngPath, &w, &h, &channels, 4);
    if (!data) {
        XConsole::PrintWarning("SetWindowIcon: cannot load '{}'", pngPath);
        return;
    }
    GLFWimage image;
    image.width = w;
    image.height = h;
    image.pixels = data;
    glfwSetWindowIcon(m_Window, 1, &image);
    stbi_image_free(data);
}

void CoreApplication::Frame(double currentTime)
{
    glfwPollEvents();

    EventQueue::Get().Process();

    float deltaTime = static_cast<float>(currentTime - m_LastFrameTime);
    m_LastFrameTime = currentTime;

    if (auto* scene = SceneManager::GetInstance().GetActiveScene())
        scene->OnUpdate(deltaTime);

    OnUpdate(deltaTime);

    InputManager::GetInstance().Update();

    OnRender();
}

void CoreApplication::FrameThunk(void* userData)
{
    static_cast<CoreApplication*>(userData)->Frame(glfwGetTime());
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
    // FIX (2026-08-24): the framebuffer-size callback can fire BEFORE this one
    // when moving the window between monitors with different DPI, so
    // HandleWindowResize derived m_Width/m_Height with the OLD scale -> the
    // logical canvas size stays wrong until the next resize event (UI laid out
    // at the wrong size, panels missing/clipped). Re-derive the logical size now
    // that the scale is known; in the good callback order this is idempotent.
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    if (fbw > 0 && fbh > 0)
        app->HandleWindowResize(fbw, fbh);
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
