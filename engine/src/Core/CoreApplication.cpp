#include "LeirEngine/Core/CoreApplication.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Scene/SceneManager.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <cstdlib>

namespace Leir {

CoreApplication::CoreApplication(const char* title, int width, int height, bool fullscreen)
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

    spdlog::info("GLFW window created ({}x{})", width, height);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(m_Window, &fbWidth, &fbHeight);
    if (fbWidth > 0 && fbHeight > 0) {
        m_Width = fbWidth;
        m_Height = fbHeight;
    }

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);

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
    spdlog::info("DIAG HandleWindowResize {}x{}", width, height);
    if (width <= 0 || height <= 0)
        return;
    m_Width = width;
    m_Height = height;
    OnWindowResized(width, height);
}

} // namespace Leir
