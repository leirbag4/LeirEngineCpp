add_library(LeirEngine SHARED
    src/Core/CoreApplication.cpp
    src/Core/CoreObject.cpp
    src/Core/Component.cpp
    src/Core/Transform.cpp
    src/Core/UUID.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneManager.cpp
    src/Objects/Object3D.cpp
    src/Objects/Object2D.cpp
    src/Input/InputManager.cpp
    src/Rendering/VulkanDevice.cpp
    src/Rendering/Shader.cpp
    src/Rendering/Texture2D.cpp
    src/Rendering/Mesh.cpp
    src/Rendering/Material.cpp
    src/Rendering/RenderPipeline.cpp
    src/Components/MeshRenderer.cpp
    src/Components/SpriteRenderer.cpp
    src/Components/Camera.cpp
    src/Components/Light.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/Collider.cpp
    src/Physics/RigidBody.cpp
)

# Shader compilation
find_program(GLSLC glslc)
if(GLSLC)
    set(SHADER_SOURCES
        shaders/Basic.vert
        shaders/Basic.frag
        shaders/Sprite.vert
        shaders/Sprite.frag
    )
    set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
        add_custom_command(
            OUTPUT ${SPV_FILE}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            COMMAND ${GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${SPV_FILE}
            COMMENT "Compiling ${SHADER_NAME}"
        )
        list(APPEND SHADER_SPVS ${SPV_FILE})
    endforeach()
    add_custom_target(LeirShaders ALL DEPENDS ${SHADER_SPVS})
    add_dependencies(LeirEngine LeirShaders)
    target_compile_definitions(LeirEngine PUBLIC LEIR_SHADER_DIR="${SHADER_OUTPUT_DIR}")
else()
    message(WARNING "glslc not found — shaders will not be compiled at build time")
endif()

# stb headers are fetched via FetchContent; bring them into scope
include(FetchContent)
FetchContent_GetProperties(stb)

target_include_directories(LeirEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${stb_SOURCE_DIR}
)

target_link_libraries(LeirEngine
    PUBLIC
        glfw
        glm
        spdlog::spdlog
    PRIVATE
        nlohmann_json::nlohmann_json
        cereal::cereal
)

# Vulkan
if(APPLE)
    find_path(Vulkan_INCLUDE_DIR vulkan/vulkan.h)
    find_library(Vulkan_LIBRARY NAMES MoltenVK vulkan)
    if(Vulkan_INCLUDE_DIR AND Vulkan_LIBRARY)
        add_library(Vulkan::Vulkan UNKNOWN IMPORTED)
        set_target_properties(Vulkan::Vulkan PROPERTIES
            IMPORTED_LOCATION "${Vulkan_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIR}"
        )
        target_link_libraries(LeirEngine PUBLIC Vulkan::Vulkan)
        target_compile_definitions(LeirEngine PUBLIC LEIR_HAS_VULKAN)
        message(STATUS "Vulkan SDK found via MoltenVK at ${Vulkan_LIBRARY}")
    else()
        message(FATAL_ERROR "Vulkan SDK not found. Install: brew install molten-vk vulkan-headers")
    endif()
else()
    find_package(Vulkan REQUIRED)
    target_link_libraries(LeirEngine PUBLIC Vulkan::Vulkan)
    target_compile_definitions(LeirEngine PUBLIC LEIR_HAS_VULKAN)
    message(STATUS "Vulkan SDK found")
endif()

# Jolt Physics
if(TARGET Jolt::Jolt)
    target_link_libraries(LeirEngine PRIVATE Jolt::Jolt)
elseif(TARGET Jolt)
    target_link_libraries(LeirEngine PRIVATE Jolt)
endif()

# SoLoud
if(TARGET soloud)
    target_link_libraries(LeirEngine PRIVATE soloud)
endif()

target_compile_definitions(LeirEngine PRIVATE LEIR_ENGINE_BUILD)
set_target_properties(LeirEngine PROPERTIES CXX_VISIBILITY_PRESET hidden)
