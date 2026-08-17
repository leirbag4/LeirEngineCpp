include(FetchContent)

# GLFW — windowing + input
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

# GLM — math
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

# stb — image loading, font rasterization
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

# Jolt Physics
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE ON CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
)

# SoLoud — audio
# Pinned to the commit we fetch (e82fd32c, 2026-08-17). SoLoud has no root
# CMakeLists.txt; the `soloud` target is built by cmake/AudioTooling.cmake
# (core + WAV/OGG decode + one output backend per platform).
FetchContent_Declare(soloud
    GIT_REPOSITORY https://github.com/jarikomppa/soloud.git
    GIT_TAG e82fd32c
)

# nlohmann/json — JSON serialization
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# cereal — binary serialization (header-only)
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE)
set(CEREAL_INSTALL ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_SANDBOX OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    GIT_TAG v1.3.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glm stb jolt soloud nlohmann_json cereal)

# Build the SoLoud static lib (no root CMakeLists.txt in the repo).
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/AudioTooling.cmake)

# wgpu-native — WebGPU backend (Fase 5, see TODO_RHI_SLANG.md).
# Prebuilt release asset, used header-only: the engine loads wgpu_native.dll at
# runtime (WGPU_SKIP_DECLARATIONS + LoadLibrary/GetProcAddress), so only the
# webgpu.h header is needed at build time. The DLL is copied next to the exes
# by the editor/example targets. Windows/MSVC only (the WebGPUBackend.cpp source
# is compiled under the same gate).
if(WIN32 AND MSVC)
    set(WGPU_RELEASE_URL
        "https://github.com/gfx-rs/wgpu-native/releases/download/v29.0.1.1/wgpu-windows-x86_64-msvc-release.zip")
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(WGPU_EXTRACT_TIMESTAMP DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    endif()
    FetchContent_Declare(wgpu
        URL ${WGPU_RELEASE_URL}
        ${WGPU_EXTRACT_TIMESTAMP}
    )
    FetchContent_MakeAvailable(wgpu)
endif()

