#pragma once

/**
 * @file ISwapchainTarget.h
 * @brief RHI-neutral per-window present target interface.
 * @ingroup Rendering
 *
 * One ISwapchainTarget owns the swapchain + backbuffers + command list + frame
 * sync needed to present to a SINGLE window while sharing the renderer's device,
 * queue and descriptor heaps. This is what lets external (detached) editor
 * windows render their own UI to their own OS window on any backend.
 *
 * Implementations: VulkanSwapchainTarget (Vulkan), D3D12SwapchainTarget (D3D12),
 * WebGPU target (future). The editor/driver calls BeginFrame -> record UI ->
 * EndFrame each frame; the target must be created via
 * RenderBackend::CreateSwapchainTarget(window).
 */

#include "LeirEngine/Core/Export.h"
#include <cstdint>

struct GLFWwindow;

namespace Leir {

/**
 * @brief Abstract per-window present target (backend-neutral).
 * @ingroup Rendering
 */
class LEIR_API ISwapchainTarget {
public:
    /**
     * @brief Virtual destructor (impls hold backend resources).
     */
    virtual ~ISwapchainTarget() = default;

    /**
     * @brief Acquires the next swapchain image and begins recording.
     * @param[out] outImageIndex Acquired image index.
     * @return True if a frame can be recorded; false if the swapchain was
     *  recreated, the window is minimized, or the target is invalid.
     */
    virtual bool BeginFrame(uint32_t& outImageIndex) = 0;

    /**
     * @brief Begins the overlay render pass (color-only, LOAD_OP_LOAD) on the
     *  acquired image. The UI graph records pass-less draws into this target's
     *  command list afterwards.
     * @param[in] imageIndex Acquired image index (from BeginFrame).
     */
    virtual void BeginOverlayRenderPass(uint32_t imageIndex) = 0;

    /**
     * @brief Ends the command list, submits and presents.
     * @param[in] imageIndex Acquired image index.
     */
    virtual void EndFrame(uint32_t imageIndex) = 0;

    /**
     * @brief Recreates the swapchain after a resize/out-of-date event.
     */
    virtual void RecreateSwapchain() = 0;

    /**
     * @brief Returns the present extent width in physical pixels.
     * @return Width.
     */
    virtual uint32_t GetWidth() const = 0;

    /**
     * @brief Returns the present extent height in physical pixels.
     * @return Height.
     */
    virtual uint32_t GetHeight() const = 0;

    /**
     * @brief Returns the command list handle to record into (for RHICommandBuffer).
     * @return Opaque handle usable as RHICommandBuffer::handle.
     */
    virtual uint64_t GetCommandBufferHandle() const = 0;

    /**
     * @brief Returns the native window this target presents to.
     * @return GLFW window pointer.
     */
    virtual GLFWwindow* GetWindow() const = 0;

    /**
     * @brief Whether the swapchain exists and is valid.
     * @return True if valid.
     */
    virtual bool IsValid() const = 0;

    /**
     * @brief Marks the swapchain as needing recreation (resize callback).
     */
    virtual void MarkResized() = 0;

    /**
     * @brief Whether a resize has been requested.
     * @return True if pending.
     */
    virtual bool NeedsResize() const = 0;

    /**
     * @brief Clears the resize-requested flag.
     */
    virtual void ResetResized() = 0;
};

} // namespace Leir