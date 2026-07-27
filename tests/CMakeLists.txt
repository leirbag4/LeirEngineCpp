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

add_test(NAME PhysicsTest COMMAND LeirEngineTests)
