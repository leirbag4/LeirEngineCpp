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

if(WIN32)
    add_custom_command(TARGET LeirEngineEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:LeirEngineEditor>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:LeirEngineEditor>/assets"
    )
endif()
