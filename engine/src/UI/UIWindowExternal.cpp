#include "LeirEngine/UI/UIWindowExternal.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/Rendering/SwapchainTarget.h"
#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/Input/InputManager.h"
#include "LeirEngine/Core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <algorithm>

namespace Leir {

namespace {
void ExternalFramebufferSizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    auto* self = static_cast<UIWindowExternal*>(glfwGetWindowUserPointer(window));
    if (self && self->GetSwapchainTarget())
        self->GetSwapchainTarget()->MarkResized();
}

void ExternalCloseCallback(GLFWwindow* window)
{
    // The OS close button (X) / Alt+F4 pressed. GLFW would only set the close
    // flag; without a handler nothing ever polls it for external windows, so
    // the window stayed open. Just request the close: the actual Close() runs
    // in RenderFrame() next frame, OUTSIDE this GLFW callback (destroying the
    // native window/swapchain inside the callback would be reentrant).
    auto* self = static_cast<UIWindowExternal*>(glfwGetWindowUserPointer(window));
    if (self) self->RequestClose();
}
} // namespace

UIWindowExternal::UIWindowExternal(RHI::RenderBackend* backend, const std::string& title)
    : UIWindow(title)
    , m_Backend(backend)
{
    SetName("UIWindowExternal");
}

UIWindowExternal::~UIWindowExternal()
{
    DestroyNative();
}

void UIWindowExternal::DestroyNative()
{
    // Teardown order matters: the SwapchainTarget's destructor calls
    // vkDeviceWaitIdle, so it MUST be destroyed first — that guarantees the GPU
    // has finished reading the command buffers that reference the UIRenderer's
    // pipeline/vertex buffers. Destroying the UIRenderer before the idle wait
    // triggered "vkDestroyPipeline/Buffer: in use by VkCommandBuffer" VUIDs.
    if (m_SwapchainTarget) {
        delete m_SwapchainTarget;  // vkDeviceWaitIdle + CleanupSwapchain
        m_SwapchainTarget = nullptr;
    }
    if (m_WindowRenderer) {
        delete m_WindowRenderer;   // now safe: GPU is idle
        m_WindowRenderer = nullptr;
    }
    if (m_WindowCanvas) {
        m_WindowCanvas->DisconnectFromInputSystem();
        delete m_WindowCanvas;
        m_WindowCanvas = nullptr;
    }
    if (m_NativeWindow) {
        glfwDestroyWindow(m_NativeWindow);
        m_NativeWindow = nullptr;
    }
}

void UIWindowExternal::Show(UIWindow* parent)
{
    if (m_NativeWindow)
        return;

    // Create the native GLFW window (no API context needed for Vulkan).
    // Size it from the logical m_WindowSize × monitor content scale (same
    // HiDPI rule as CoreApplication), NOT a hardcoded 320×240. Otherwise the
    // canvas logical size (= physical extent ÷ scale) is smaller than the
    // requested size and content near the bottom — e.g. the About window's
    // OK button — is laid out OUTSIDE the visible area and never shows.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, m_Resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); // OS chrome title bar (default)
    float primaryScale = 1.0f;
    if (GLFWmonitor* primary = glfwGetPrimaryMonitor()) {
        float sx, sy;
        glfwGetMonitorContentScale(primary, &sx, &sy);
        primaryScale = std::max(sx, sy);
    }
    const int createW = std::max((int)std::lround(m_WindowSize.x * primaryScale), 1);
    const int createH = std::max((int)std::lround(m_WindowSize.y * primaryScale), 1);
    m_NativeWindow = glfwCreateWindow(createW, createH, m_Title.c_str(), nullptr, nullptr);
    if (!m_NativeWindow) {
        XConsole::PrintError("UIWindowExternal: failed to create GLFW window");
        return;
    }
    glfwSetWindowUserPointer(m_NativeWindow, this);
    glfwSetFramebufferSizeCallback(m_NativeWindow, ExternalFramebufferSizeCallback);
    // Without this callback the OS close button (X) only set the close flag and
    // nothing polled it — external windows could not be closed. The callback
    // defers the real Close() to the next RenderFrame (see RequestClose()).
    glfwSetWindowCloseCallback(m_NativeWindow, ExternalCloseCallback);

    // Create the swapchain target sharing the main device.
    if (m_Backend) {
        m_SwapchainTarget = m_Backend->CreateSwapchainTarget(m_NativeWindow);
        if (!m_SwapchainTarget) {
            XConsole::PrintError("UIWindowExternal: CreateSwapchainTarget failed (non-Vulkan backend?)");
            glfwDestroyWindow(m_NativeWindow);
            m_NativeWindow = nullptr;
            return;
        }
    }

    // Create a dedicated canvas for this window.
    m_WindowCanvas = new UICanvas();
    m_WindowCanvas->SetInputWindow(m_NativeWindow);
    m_WindowCanvas->ConnectToInputSystem();

    // Dedicated UIRenderer for this window (shares the backend device).
    m_WindowRenderer = new UIRenderer(m_Backend);
    float csx, csy;
    glfwGetWindowContentScale(m_NativeWindow, &csx, &csy);
    const float scale = std::max(csx, csy);
    m_WindowRenderer->SetContentScale(scale);
    Leir::InputManager::GetInstance().SetContentScaleForWindow(m_NativeWindow, scale);

    // Opaque background FIRST (behind any content): the overlay render pass is
    // LOAD_OP_LOAD (does not clear), so the UI must paint the whole window
    // opaque or the swapchain's stale/garbage memory shows through (black +
    // colored noise around sparse widgets). Same as the editor canvas, whose
    // DockManager background covers everything.
    m_Background = new UIPanel();
    m_Background->SetName("WindowBackground");
    m_Background->SetColor({0.13f, 0.13f, 0.15f, 1.0f});
    m_Background->GetRect().anchor = AnchorSet::Stretch();
    m_Background->GetRect().offset = {};
    m_WindowCanvas->AddChild(m_Background);

    // Canvas works in logical units: extent (physical) / scale. Refreshed
    // every frame in RenderFrame so resize follows the swapchain.
    m_WindowCanvas->SetScreenSize(
        (float)m_SwapchainTarget->GetExtent().width / scale,
        (float)m_SwapchainTarget->GetExtent().height / scale);

    // Register callbacks on the external window so events reach EventQueue.
    Leir::InputManager::GetInstance().AddWindow(m_NativeWindow);

    XConsole::Println("UIWindowExternal: '{}' created ({}x{})", m_Title,
        m_SwapchainTarget->GetExtent().width, m_SwapchainTarget->GetExtent().height);

    m_Visible = true;
    OnShow();
}

void UIWindowExternal::Close()
{
    if (!m_Visible)
        return;
    m_Visible = false;
    OnClose();
    DestroyNative();
    if (m_OnResult) m_OnResult(m_Result);
    if (m_OnClosed) m_OnClosed();
}

void UIWindowExternal::Hide()
{
    if (m_NativeWindow)
        glfwIconifyWindow(m_NativeWindow);
    m_Visible = false;
}

void UIWindowExternal::BringToFront()
{
    if (m_NativeWindow)
        glfwFocusWindow(m_NativeWindow);
}

void UIWindowExternal::SetTitle(const std::string& title)
{
    UIWindow::SetTitle(title);
    if (m_NativeWindow)
        glfwSetWindowTitle(m_NativeWindow, title.c_str());
}

bool UIWindowExternal::RenderFrame()
{
    // A close was requested by the OS (X button / Alt+F4) while GLFW was
    // polling. Run the real Close() here — outside the GLFW close callback —
    // so the native window/swapchain/canvas are destroyed cleanly.
    if (m_CloseRequested) {
        m_CloseRequested = false;
        Close();
        return false;
    }

    if (!m_SwapchainTarget || !m_SwapchainTarget->IsValid())
        return false;

    uint32_t imageIndex;
    if (!m_SwapchainTarget->BeginFrame(imageIndex))
        return false;

    // Begin the overlay render pass on this target's swapchain image.
    m_SwapchainTarget->BeginOverlayRenderPass(imageIndex);

    // Layout the canvas and render its content (draw records only; the pass
    // is already running). The graph executes against the target's command
    // buffer, so this window's UI lands in this window's swapchain.
    if (m_WindowCanvas && m_WindowRenderer) {
        // Keep the canvas in sync with the (possibly resized) swapchain:
        // logical size = physical extent / content scale.
        const float scale = m_WindowRenderer->GetContentScale();
        m_WindowCanvas->SetScreenSize(
            (float)m_SwapchainTarget->GetExtent().width / scale,
            (float)m_SwapchainTarget->GetExtent().height / scale);
        m_WindowCanvas->UpdateLayout();
        m_WindowGraph.Clear();
        m_WindowRenderer->Render(m_WindowGraph, m_WindowCanvas);

        RHI::RHICommandBuffer rhiCmd;
        rhiCmd.handle = reinterpret_cast<uint64_t>(m_SwapchainTarget->GetCommandBuffer());
        m_Backend->CmdExecuteGraph(rhiCmd, m_WindowGraph);
    }

    m_SwapchainTarget->EndFrame(imageIndex);
    return true;
}

} // namespace Leir