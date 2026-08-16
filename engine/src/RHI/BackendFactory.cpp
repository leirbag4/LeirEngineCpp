#include "LeirEngine/RHI/RenderBackend.h"
#include "LeirEngine/RHI/VulkanBackend.h"
#include "LeirEngine/RHI/D3D12Backend.h"
#include "LeirEngine/RHI/WebGPUBackend.h"

#include <string>

// BackendFactory: backend-neutral dispatch between the concrete backends.
// Kept in its own TU so WebGPUBackend.cpp / VulkanBackend.cpp don't pull each
// other's headers on platforms where the sibling backend is not built
// (Vulkan excluded on Emscripten; D3D12/WebGPU native excluded without MSVC).

namespace Leir {
namespace RHI {

RenderBackend* BackendFactory::Create(const std::string& backend,
    void* window, int width, int height, bool vsync, const std::string& appName) {
    if (backend == "d3d12")
        return CreateD3D12(window, width, height, vsync, appName);
    if (backend == "webgpu")
        return CreateWebGPU(window, width, height, vsync, appName);
    if (backend == "vulkan")
        return CreateVulkan(window, width, height, vsync, appName);
#if LEIR_BACKEND == LEIR_BACKEND_D3D12
    return CreateD3D12(window, width, height, vsync, appName);
#elif defined(__EMSCRIPTEN__) || LEIR_BACKEND == LEIR_BACKEND_WEBGPU
    return CreateWebGPU(window, width, height, vsync, appName);
#else
    return CreateVulkan(window, width, height, vsync, appName);
#endif
}

RenderBackend* BackendFactory::CreateVulkan(void* window, int width, int height,
    bool vsync, const std::string& appName) {
#if defined(__EMSCRIPTEN__)
    // Vulkan is not available in the browser build.
    (void)window; (void)width; (void)height; (void)vsync; (void)appName;
    return nullptr;
#else
    return new VulkanBackend(window, width, height, vsync, appName);
#endif
}

RenderBackend* BackendFactory::CreateD3D12(void* window, int width, int height,
    bool vsync, const std::string& appName) {
#if defined(_WIN32) && defined(_MSC_VER)
    return new D3D12Backend(window, width, height, vsync, appName);
#else
    // D3D12 backend requires MSVC (MinGW CI builds Vulkan only).
    (void)window; (void)width; (void)height; (void)vsync; (void)appName;
    return nullptr;
#endif
}

RenderBackend* BackendFactory::CreateWebGPU(void* window, int width, int height,
    bool vsync, const std::string& appName) {
#if defined(_WIN32) && defined(_MSC_VER)
    return new WebGPUBackend(window, width, height, vsync, appName);
#elif defined(__EMSCRIPTEN__)
    return new WebGPUBackend(window, width, height, vsync, appName);
#else
    // WebGPU backend requires MSVC (wgpu-native release is Windows/MSVC only).
    (void)window; (void)width; (void)height; (void)vsync; (void)appName;
    return nullptr;
#endif
}

void BackendFactory::Destroy(RenderBackend* backend) {
    delete backend;
}

} // namespace RHI
} // namespace Leir