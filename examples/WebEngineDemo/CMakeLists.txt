cmake_minimum_required(VERSION 3.20)
project(LeirEngineWebEngineDemo LANGUAGES CXX)

# Standalone Emscripten target for Fase 6 / M2: builds the full engine as a
# web-safe static lib (LeirEngineCore, engine/CMakeLists.web.txt) plus the demo
# executable. Renders through WebGPUBackend + emdawnwebgpu. Configure with the
# emscripten toolchain (preset `emscripten`).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emscripten: emit an HTML shell (with #canvas) plus the .js/.wasm/.data — emcc
# only generates the HTML when the link output ends in ".html".
set(CMAKE_EXECUTABLE_SUFFIX ".html")

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "WebEngineDemo must be configured with the Emscripten toolchain (preset emscripten)")
endif()

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
# Jolt Physics (Fase 6 / M3) — same tag/source as the desktop engine. Jolt's
# CMake edits global CMAKE_CXX_FLAGS, so pin the options that would otherwise
# break the engine build: keep RTTI + exceptions ON (the engine uses
# dynamic_cast in the UI and try/catch in Settings), drop -Wall -Werror, leave
# our own flags untouched, and disable the graphics/compute backends (CPU
# compute pulls x86-only Xbyak).
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(jolt
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
)
# SoLoud (Fase 6 / M4) — same pin as the desktop engine; the `soloud` static
# lib is created by cmake/AudioTooling.cmake (MiniAudio -> WebAudio backend on
# web, built with -std=gnu++20 because miniaudio forbids strict -std=c*).
FetchContent_Declare(soloud
    GIT_REPOSITORY https://github.com/jarikomppa/soloud.git
    GIT_TAG e82fd32c
)
FetchContent_MakeAvailable(jolt)
FetchContent_MakeAvailable(soloud)
FetchContent_MakeAvailable(nlohmann_json)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
endif()
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

include(../../cmake/AudioTooling.cmake)
include(../../engine/CMakeLists.web.txt)

# (Fase D) demo executable: links LeirEngineCore (full web-safe engine subset),
# drives a Scene with 2 textured cubes + orbit camera + UI, and preloads the
# web shader variants (engine/shaders -> /shaders) + the Roboto TTF
# (assets -> /assets) into Emscripten's virtual FS.
set(LEIR_DEMO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(WebEngineDemo main.cpp)
target_link_libraries(WebEngineDemo PRIVATE LeirEngineCore)
target_compile_options(WebEngineDemo PRIVATE
    -sASYNCIFY=1
    --use-port=contrib.glfw3
)
target_link_options(WebEngineDemo PRIVATE
    -sASYNCIFY=1
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=16777216
    # Export malloc/free to the JS Module (Module['_malloc']) so miniaudio's
    # WebAudio backend can allocate its JS<->C interop buffer (EM_ASM calls
    # Module._malloc / Module._free). Only symbols listed in EXPORTED_FUNCTIONS
    # get the Module[name] copy; the auto-generated exports (KEEPALIVE,
    # emwgpuOn*, dynCall, stack) are preserved separately by the linker.
    -sEXPORTED_FUNCTIONS=_malloc,_free,_main
    # The same EM_ASM block builds the interop view as
    # new Float32Array(Module.HEAPF32.buffer, ...) and the ScriptProcessorNode
    # callback calls the C callback via ccall(...) — the default
    # EXPORTED_RUNTIME_METHODS is empty, so HEAP* and ccall must be exported
    # explicitly to be reachable as Module.HEAPF32 / the ccall helper.
    -sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF64,ccall
    --use-port=${LEIR_DEMO_ROOT}/dependencies/emdawnwebgpu/emdawnwebgpu.port.py
    --use-port=contrib.glfw3
    "--preload-file=${LEIR_DEMO_ROOT}/engine/shaders@/shaders"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
    "--preload-file=${LEIR_DEMO_ROOT}/assets/audio@/assets/audio"
)