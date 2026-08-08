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

target_include_directories(LeirEngineEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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

add_custom_command(TARGET LeirEngineEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LeirEngine>"
        "$<TARGET_FILE_DIR:LeirEngineEditor>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LeirEngineEditor>/assets"
)
