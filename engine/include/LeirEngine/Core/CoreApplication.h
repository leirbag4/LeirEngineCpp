#pragma once

#include "LeirEngine/Core/Export.h"

struct GLFWwindow;

namespace Leir {

class LEIR_API CoreApplication {
public:
    CoreApplication(const char* title, int width, int height, bool fullscreen = false);
    virtual ~CoreApplication();

    void Run();
    void Quit();

    GLFWwindow* GetWindow() const { return m_Window; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

protected:
    virtual void OnInit() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnShutdown() {}
    virtual void OnWindowResized(int width, int height) {}

    GLFWwindow* m_Window = nullptr;
    int m_Width = 1280;
    int m_Height = 720;
    bool m_Running = false;

private:
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    void HandleWindowResize(int width, int height);
};

} // namespace Leir
