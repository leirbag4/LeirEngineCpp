add_executable(LeirEngineEditor
    src/main.cpp
    src/CrashDiagnostics.cpp
    src/Camera/EditorCamera.cpp
    src/UI/UIDragFloatInput.cpp
    src/UI/UITestPanel.cpp
    src/UI/CameraTestPanel.cpp
    src/UI/DebugTextPanel.cpp
    src/UI/TextAreaDebugPanel.cpp
    src/UI/TextAreaWrapPanel.cpp
    src/UI/ConsolePanel.cpp
    src/UI/InspectorTransformPanel.cpp
    src/UI/DebugPanel.cpp
)

# ---- Shader tooling (Plan A, TODO_RHI_SLANG.md §5) ----
# IShaderCompiler implementation + multi-format exporter + shader hot-reload.
# Editor-only dev tools: they link libslang, so they are only built when the
# Slang libraries from the Vulkan SDK are available (Windows + MSVC). The
# engine DLL itself stays slang-free (isolation §6).
if(WIN32 AND MSVC AND DEFINED ENV{VULKAN_SDK})
    target_sources(LeirEngineEditor PRIVATE
        src/Shaders/SlangShaderCompiler.cpp
        src/Shaders/ShaderExporter.cpp
        src/Shaders/ShaderHotReloader.cpp
    )
    target_compile_definitions(LeirEngineEditor PRIVATE LEIR_EDITOR_SLANG=1)
endif()

target_include_directories(LeirEngineEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Output dir for the multi-format shader export (ShaderExporter writes the
# compiled .slang -> SPIR-V/DXIL/MSL/WGSL/GLSL files here for inspection).
target_compile_definitions(LeirEngineEditor PRIVATE
    LEIR_SHADER_EXPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/shaders_export"
)

target_link_libraries(LeirEngineEditor
    PRIVATE
        LeirEngine
)

# Crash diagnostics (editor/src/CrashDiagnostics.cpp) uses DbgHelp on Windows.
# `dbghelp` is a real SDK/mingw import lib, so linking it via CMake works for
# BOTH MSVC and MinGW — the pragma in the .cpp is MSVC-only and would leave
# MinGW (CI) with undefined SymInitialize/SymGetModuleInfo64/SymFromAddr.
if(WIN32)
    target_link_libraries(LeirEngineEditor PRIVATE dbghelp)
endif()

# ---- Slang compiler (editor shader tooling) ----
# Uses the libslang bundled with the Vulkan SDK ($VULKAN_SDK/Include/slang,
# $VULKAN_SDK/Lib/slang.lib, $VULKAN_SDK/Bin/slang*.dll) — the exact version
# (2026.13.1) that ships with the SDK, so it matches the `slangc` that compiles
# the engine shaders at build time. Deliberate deviation from the original Plan
# A ("static libslang via FetchContent"): a source build pulls the whole Slang
# compiler + LLVM and is slow/fragile; the SDK prebuilt is version-exact with
# the spike and avoids the heavy build. See TODO_RHI_SLANG.md §5 Plan A.
if(WIN32 AND MSVC AND DEFINED ENV{VULKAN_SDK})
    target_include_directories(LeirEngineEditor PRIVATE "$ENV{VULKAN_SDK}/Include/slang")
    target_link_libraries(LeirEngineEditor PRIVATE "$ENV{VULKAN_SDK}/Lib/slang.lib")
    # Copy the Slang runtime DLLs next to the editor exe ($VULKAN_SDK/Bin is not
    # guaranteed to be on PATH when the editor is launched).
    foreach(SLANG_DLL IN ITEMS slang.dll slang-compiler.dll slang-rt.dll slang-glslang.dll slang-glsl-module.dll)
        add_custom_command(TARGET LeirEngineEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$ENV{VULKAN_SDK}/Bin/${SLANG_DLL}"
                "$<TARGET_FILE_DIR:LeirEngineEditor>/${SLANG_DLL}"
        )
    endforeach()
endif()

add_custom_command(TARGET LeirEngineEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LeirEngine>"
        "$<TARGET_FILE_DIR:LeirEngineEditor>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LeirEngineEditor>/assets"
)
