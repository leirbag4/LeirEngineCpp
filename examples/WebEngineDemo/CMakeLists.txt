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
FetchContent_MakeAvailable(nlohmann_json)
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    FetchContent_Populate(glm)
endif()
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
endif()

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
    --use-port=${LEIR_DEMO_ROOT}/dependencies/emdawnwebgpu/emdawnwebgpu.port.py
    --use-port=contrib.glfw3
    "--preload-file=${LEIR_DEMO_ROOT}/engine/shaders@/shaders"
    "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@/assets"
)