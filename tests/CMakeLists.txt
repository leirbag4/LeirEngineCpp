add_executable(LeirEngineTests
    PhysicsTest.cpp
)

target_link_libraries(LeirEngineTests PRIVATE LeirEngine)

# Embed RPATH so test executables can find LeirEngine shared lib without env vars
if(APPLE OR UNIX)
    set_target_properties(LeirEngineTests PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/engine"
    )
endif()

target_include_directories(LeirEngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    add_custom_command(TARGET LeirEngineTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:LeirEngineTests>"
    )
endif()

add_test(NAME PhysicsTest COMMAND LeirEngineTests)

# ---- Hybrid ECS core test (Entity + sparse-set pools + World + journal) ----
add_executable(LeirECSTests
    ECSTest.cpp
)
target_link_libraries(LeirECSTests PRIVATE LeirEngine)
target_include_directories(LeirECSTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE OR UNIX)
    set_target_properties(LeirECSTests PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/engine"
    )
endif()
if(WIN32)
    add_custom_command(TARGET LeirECSTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:LeirECSTests>"
    )
endif()
add_test(NAME ECSTest COMMAND LeirECSTests)

# ---- Shader export smoke test (CI runtime validation of vendored Slang) ----
# Compiles the 6 .slang shaders to all 5 targets through the editor's
# SlangShaderCompiler + ShaderExporter. Runs on every platform in CI, proving
# the vendored libslang links, loads (dlopen/dyld) and codegens correctly.
include("${CMAKE_SOURCE_DIR}/cmake/SlangTooling.cmake")
add_executable(SlangExportTest
    ../editor/src/Shaders/SlangShaderCompiler.cpp
    ../editor/src/Shaders/ShaderExporter.cpp
    SlangExportTest.cpp
)
target_link_libraries(SlangExportTest PRIVATE LeirEngine)
target_include_directories(SlangExportTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/editor/src/Shaders
)
target_compile_definitions(SlangExportTest PRIVATE
    LEIR_SHADER_EXPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}/slang_export"
)
if(WIN32)
    add_custom_command(TARGET SlangExportTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:SlangExportTest>"
    )
endif()
leir_setup_slang_target(SlangExportTest)

add_test(NAME SlangExportTest COMMAND SlangExportTest)
