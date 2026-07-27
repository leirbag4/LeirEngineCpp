add_executable(LeirEngineTests
    PhysicsTest.cpp
)

target_link_libraries(LeirEngineTests PRIVATE LeirEngine)

target_include_directories(LeirEngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME PhysicsTest COMMAND LeirEngineTests)
