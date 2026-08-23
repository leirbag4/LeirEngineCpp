add_executable(LeirEngineEditor
    src/main.cpp
    src/CrashDiagnostics.cpp
src/Camera/EditorCamera.cpp
src/Grid/EditorGrid.cpp
src/Gizmos/GizmoRenderer.cpp
    src/Gizmos/TransformGizmo.cpp
    src/UI/UIDragFloatInput.cpp
    src/UI/UITestPanel.cpp
    src/UI/GizmoLineTestPanel.cpp
    src/UI/GridPanel.cpp
    src/UI/CameraTestPanel.cpp
    src/UI/DebugTextPanel.cpp
    src/UI/TextAreaDebugPanel.cpp
    src/UI/TextAreaWrapPanel.cpp
    src/UI/ConsolePanel.cpp
    src/UI/InspectorTransformPanel.cpp
    src/UI/DebugPanel.cpp
    src/UI/GizmoLogPanel.cpp
    src/UI/TreeViewDebugPanel.cpp
    src/UI/ToolbarPanel.cpp
)

# ---- Shader tooling (Plan A, TODO_RHI_SLANG.md §5) ----
# IShaderCompiler implementation + multi-format exporter + shader hot-reload.
# Editor-only dev tools: they link libslang, vendored under vendor/slang/
# (v2026.14.1, shader-slang release — see vendor/slang/README.md). Available on
# every platform; the engine DLL itself stays slang-free (isolation §6).
target_sources(LeirEngineEditor PRIVATE
    src/Shaders/SlangShaderCompiler.cpp
    src/Shaders/ShaderExporter.cpp
    src/Shaders/ShaderHotReloader.cpp
)
target_compile_definitions(LeirEngineEditor PRIVATE LEIR_EDITOR_SLANG=1)

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
# Vendored libslang (v2026.14.1) under vendor/slang/ — per-platform link + the
# runtime libs copied next to the exe. Shared with the SlangExportTest smoke
# test via cmake/SlangTooling.cmake (see that file + vendor/slang/README.md).
include("${CMAKE_SOURCE_DIR}/cmake/SlangTooling.cmake")
leir_setup_slang_target(LeirEngineEditor)

add_custom_command(TARGET LeirEngineEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LeirEngine>"
        "$<TARGET_FILE_DIR:LeirEngineEditor>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LeirEngineEditor>/assets"
)

# wgpu_native.dll (Fase 5, WebGPU backend) — loaded dynamically at runtime via
# LoadLibrary, so it must sit next to the exe. Windows/MSVC only (same gate as
# the backend source).
if(WIN32 AND MSVC)
    include(FetchContent)
    FetchContent_GetProperties(wgpu)
    add_custom_command(TARGET LeirEngineEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${wgpu_SOURCE_DIR}/lib/wgpu_native.dll"
            "$<TARGET_FILE_DIR:LeirEngineEditor>"
    )
endif()
