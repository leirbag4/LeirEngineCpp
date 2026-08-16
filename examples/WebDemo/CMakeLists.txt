cmake_minimum_required(VERSION 3.20)
project(LeirEngineWebDemo LANGUAGES CXX)

# Standalone Emscripten target for Fase 6 / M1: builds the raw-RHI cube demo
# against the web-safe engine sources (WebGPUBackend + BackendFactory +
# XConsole) without the desktop superbuild. Configure with the emscripten
# toolchain (preset `emscripten-webdemo`).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emscripten: emit an HTML shell (with #canvas) plus the .js/.wasm/.data — emcc
# only generates the HTML when the link output ends in ".html".
set(CMAKE_EXECUTABLE_SUFFIX ".html")

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "WebDemo must be configured with the Emscripten toolchain (preset emscripten-webdemo)")
endif()

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
endif()

set(LEIR_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_library(WebDemoCore STATIC
    ${LEIR_SRC}/engine/src/RHI/WebGPUBackend.cpp
    ${LEIR_SRC}/engine/src/RHI/BackendFactory.cpp
    ${LEIR_SRC}/engine/src/Core/Log.cpp
)
target_include_directories(WebDemoCore PRIVATE
    ${LEIR_SRC}/engine/include
    ${LEIR_SRC}/dependencies/emdawnwebgpu/webgpu/include
    ${glm_SOURCE_DIR}
)
target_compile_options(WebDemoCore PRIVATE
    -sASYNCIFY=1
    --use-port=contrib.glfw3
)

add_executable(LeirEngineWebDemo main.cpp)
target_include_directories(LeirEngineWebDemo PRIVATE
    ${LEIR_SRC}/engine/include
    ${LEIR_SRC}/dependencies/emdawnwebgpu/webgpu/include
    ${glm_SOURCE_DIR}
)
target_compile_options(LeirEngineWebDemo PRIVATE
    --use-port=contrib.glfw3
)
target_link_libraries(LeirEngineWebDemo PRIVATE WebDemoCore)
target_link_options(LeirEngineWebDemo PRIVATE
    -sASYNCIFY=1
    -sALLOW_MEMORY_GROWTH=1
    -sEXCEPTION_CATCHING_ALLOWED=[".."]
    --use-port=${LEIR_SRC}/dependencies/emdawnwebgpu/emdawnwebgpu.port.py
    --use-port=contrib.glfw3
    --preload-file ${LEIR_SRC}/engine/shaders@/shaders
)