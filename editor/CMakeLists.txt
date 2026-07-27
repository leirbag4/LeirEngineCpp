add_executable(LeirEngineEditor
    src/main.cpp
)

target_include_directories(LeirEngineEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(LeirEngineEditor
    PRIVATE
        LeirEngine
)
