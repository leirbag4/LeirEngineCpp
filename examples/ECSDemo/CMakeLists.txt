add_executable(LeirEngineECSDemo
    main.cpp
)

target_link_libraries(LeirEngineECSDemo
    PRIVATE
        LeirEngine
)

if(WIN32)
    add_custom_command(TARGET LeirEngineECSDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:LeirEngineECSDemo>"
    )
    if(MSVC)
        include(FetchContent)
        FetchContent_GetProperties(wgpu)
        add_custom_command(TARGET LeirEngineECSDemo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${wgpu_SOURCE_DIR}/lib/wgpu_native.dll"
                "$<TARGET_FILE_DIR:LeirEngineECSDemo>"
        )
    endif()
endif()