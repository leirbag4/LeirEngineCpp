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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_Window = glfwCreateWindow(width, height, title, monitor, nullptr);
    if (!m_Window) {
        spdlog::critical("Failed to create GLFW window");
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }

    spdlog::info("GLFW window created ({}x{})", width, height);

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

} // namespace Leir
