add_executable(LeirEnginePhysicsDemo
    main.cpp
)

target_link_libraries(LeirEnginePhysicsDemo
    PRIVATE
        LeirEngine
)

# Absolute path to the repo's assets/audio so the demo can play the shared
# WAV/OGG clips regardless of the working directory (mirrors the web demo,
# which preloads the same folder to the virtual /assets/audio).
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/audio" LEIR_AUDIO_DIR)
target_compile_definitions(LeirEnginePhysicsDemo PRIVATE
    LEIR_AUDIO_DIR="${LEIR_AUDIO_DIR}")

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
