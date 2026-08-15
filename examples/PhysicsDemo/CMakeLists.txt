add_executable(LeirEnginePhysicsDemo
    main.cpp
)

target_link_libraries(LeirEnginePhysicsDemo
    PRIVATE
        LeirEngine
)

if(WIN32)
    add_custom_command(TARGET LeirEnginePhysicsDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:LeirEngine>"
            "$<TARGET_FILE_DIR:LeirEnginePhysicsDemo>"
    )
    if(MSVC)
        include(FetchContent)
        FetchContent_GetProperties(wgpu)
        add_custom_command(TARGET LeirEnginePhysicsDemo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${wgpu_SOURCE_DIR}/lib/wgpu_native.dll"
                "$<TARGET_FILE_DIR:LeirEnginePhysicsDemo>"
        )
    endif()
endif()
