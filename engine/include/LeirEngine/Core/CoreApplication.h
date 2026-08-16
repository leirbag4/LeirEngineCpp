#pragma once

#include "LeirEngine/Core/Export.h"
#include <climits>

struct GLFWwindow;

namespace Leir {

class LEIR_API CoreApplication {
public:
    CoreApplication(const char* title, int width, int height, bool fullscreen = false,
                    int posX = INT_MIN, int posY = INT_MIN, bool maximized = false,
                    bool hidpi = true);
    virtual ~CoreApplication();

    void Run();
    void Quit();

    GLFWwindow* GetWindow() const { return m_Window; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetFramebufferWidth() const { return m_FbWidth; }
    int GetFramebufferHeight() const { return m_FbHeight; }
    // System DPI scale (1.0 = 100%). Returns 1.0 when HiDPI scaling is disabled.
    float GetContentScale() const { return m_HidpiEnabled ? m_ContentScale : 1.0f; }
    void SetHidpiEnabled(bool enabled) { m_HidpiEnabled = enabled; }

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
    virtual void OnContentScaleChanged() {}

    GLFWwindow* m_Window = nullptr;
    int m_Width = 1280;    // logical window size (screen units)
    int m_Height = 720;
    int m_FbWidth = 1280;  // physical framebuffer size (pixels)
    int m_FbHeight = 720;
    bool m_Running = false;
    double m_LastFrameTime = 0.0;
    float m_ContentScale = 1.0f;
    bool m_HidpiEnabled = true;

    // Normal-state window rect (tracked via size/pos callbacks)
    int m_NormalX = 0;
    int m_NormalY = 0;
    int m_NormalW = 0;
    int m_NormalH = 0;
    bool m_HasNormalRect = false;

private:
    void Frame(double currentTime);
    static void FrameThunk(void* userData);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowPosCallback(GLFWwindow* window, int x, int y);
    static void WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);
    void HandleWindowResize(int width, int height);
    void UpdateNormalRect(int x, int y, int w, int h);
    int ToLogical(int nativeSize) const;
    void CenterWindow();
};

} // namespace Leir
