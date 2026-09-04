#pragma once

/**
 * @file UIWindowExternal.h
 * @brief External (OS-native) window using a SwapchainTarget + dedicated canvas.
 * @ingroup UI
 *
 * A UIWindow that lives in its own GLFW window with a Vulkan swapchain
 * shared from the main device. It has its own UICanvas and UIRenderer
 * so the content tree renders into the external window.
 *
 * On platforms without OS windows (mobile/web), construction returns
 * nullptr — use UIWindowInternal (the embedded mode) instead.
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIWindow.h"
#include "LeirEngine/UI/UIRenderer.h"
#include "LeirEngine/RHI/GCommandGraph.h"
#include "LeirEngine/Math/Vector2.h"

#include <memory>
#include <string>

struct GLFWwindow;

namespace Leir {

class ISwapchainTarget;
class UICanvas;

namespace RHI { class RenderBackend; }

/**
 * @brief External desktop window: own GLFW window, shared-Vulkan swapchain,
 *        dedicated canvas and renderer.
 * @ingroup UI
 *
 * The window is created in Show() and destroyed in Close()/ destructor.
 * The editor/driver calls RenderFrame() each frame to present the canvas.
 */
class LEIR_API UIWindowExternal : public UIWindow {
public:
    /**
     * @brief Constructs an external window reference (no native window yet).
     * @param[in] backend Render backend (must be non-null; VulkanBackend only for now).
     * @param[in] title Window title.
     */
    UIWindowExternal(RHI::RenderBackend* backend, const std::string& title = "Window");

    /**
     * @brief Destructor — closes the native window and frees swapchain/canvas.
     */
    ~UIWindowExternal() override;

    // ---- Lifecycle (overrides UIWindow) ----

    /**
     * @brief Creates the native GLFW window and swapchain target.
     * @param[in] parent Parent window (ignored for external; kept for hierarchy).
     */
    void Show(UIWindow* parent = nullptr) override;

    /**
     * @brief Closes the native window and destroys swapchain/canvas/renderer.
     */
    void Close() override;

    /**
     * @brief Hides the native window (iconify/minimize).
     */
    void Hide() override;

    /**
     * @brief Brings the native OS window to the front.
     */
    void BringToFront() override;

    /**
     * @brief Sets the window title.
     * @param[in] title UTF-8 title.
     */
    void SetTitle(const std::string& title) override; // actually we mark as override too

    // ---- Per-frame render ----

    /**
     * @brief Renders one frame: acquires the swapchain image, renders the
     * canvas content (if any), and presents.
     *
     * Called by the editor/driver every frame AFTER the main window render.
     *
     * If no canvas content exists yet, clears to a neutral color.
     */
    bool RenderFrame();

    // ---- Getters ----

    /**
     * @brief Requests the native window to close (OS title-bar X, Alt+F4).
     * @details Called from the GLFW close callback. The actual Close() runs in
     *  the next RenderFrame() — never inside the GLFW callback — so the native
     *  window is destroyed outside GLFW's event dispatch (no reentrancy).
     */
    void RequestClose() { m_CloseRequested = true; }

    /**
     * @brief Returns the native GLFW window handle (for input routing).
     */
    GLFWwindow* GetNativeWindow() const { return m_NativeWindow; }

    /**
     * @brief Returns the swapchain target (for frame sync).
     */
    ISwapchainTarget* GetSwapchainTarget() const { return m_SwapchainTarget; }

    /**
     * @brief Returns the dedicated canvas for this window.
     */
    UICanvas* GetCanvas() const { return m_WindowCanvas; }

    /**
     * @brief Whether the native window exists and is valid.
     */
    bool IsNativeWindowValid() const { return m_NativeWindow != nullptr; }

private:
    void DestroyNative();

    RHI::RenderBackend* m_Backend = nullptr;
    GLFWwindow* m_NativeWindow = nullptr;
    ISwapchainTarget* m_SwapchainTarget = nullptr;
    UICanvas* m_WindowCanvas = nullptr;
    UIRenderer* m_WindowRenderer = nullptr;
    UIPanel* m_Background = nullptr;   // opaque full-window background (covers swapchain garbage).
    RHI::GCommandGraph m_WindowGraph;
    bool m_CloseRequested = false;     ///< Close requested by the OS (X button); processed in RenderFrame.
};

} // namespace Leir