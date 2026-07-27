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
