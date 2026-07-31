#pragma once

#include "LeirEngine/Core/Export.h"
#include <climits>

struct GLFWwindow;

namespace Leir {

class LEIR_API CoreApplication {
public:
    CoreApplication(const char* title, int width, int height, bool fullscreen = false,
                    int posX = INT_MIN, int posY = INT_MIN, bool maximized = false);
    virtual ~CoreApplication();

    void Run();
    void Quit();

    GLFWwindow* GetWindow() const { return m_Window; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // Window placement queries (window coordinates, not framebuffer)
    void GetWindowPosition(int& x, int& y) const;
    void GetWindowSize(int& w, int& h) const;
    bool IsMaximized() const;
    // Last known rect while in normal (non-maximized, non-fullscreen) state.
    // Returns false if no normal state has been observed yet.
    bool GetNormalWindowRect(int& x, int& y, int& w, int& h) const;

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

    // Normal-state window rect (tracked via size/pos callbacks)
    int m_NormalX = 0;
    int m_NormalY = 0;
    int m_NormalW = 0;
    int m_NormalH = 0;
    bool m_HasNormalRect = false;

private:
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowPosCallback(GLFWwindow* window, int x, int y);
    void HandleWindowResize(int width, int height);
    void UpdateNormalRect(int x, int y, int w, int h);
    void CenterWindow();
};

} // namespace Leir
