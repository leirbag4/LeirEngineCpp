add_library(LeirEngine SHARED
    src/Core/CoreApplication.cpp
    src/Core/Settings.cpp
    src/Core/Log.cpp
    src/Core/CoreObject.cpp
    src/Core/Component.cpp
    src/Core/Transform.cpp
    src/Core/UUID.cpp
    src/ECS/World.cpp
    src/ECS/HierarchyTree.cpp
    src/ECS/TransformSystem.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneManager.cpp
    src/Objects/Object3D.cpp
    src/Objects/Object2D.cpp
    src/Input/InputManager.cpp
    src/Input/EventQueue.cpp
    src/Input/Keyboard.cpp
    src/Input/Mouse.cpp
    src/Input/Touch.cpp
    src/Input/Pointer.cpp
    src/Rendering/VulkanDevice.cpp
    src/RHI/VulkanBackend.cpp
    src/RHI/BackendFactory.cpp
    src/Rendering/Shader.cpp
    src/Rendering/ShaderLayout.cpp

    src/Rendering/Image.cpp
    src/Rendering/Texture2D.cpp
    src/Rendering/Mesh.cpp
    src/Rendering/Material.cpp
    src/Rendering/RenderPipeline.cpp
    src/Rendering/RenderTexture.cpp
    src/Components/MeshRenderer.cpp
    src/Components/SpriteRenderer.cpp
    src/Components/Camera.cpp
    src/Rendering/SpriteSheet.cpp
    src/Components/Light.cpp
    src/UI/UIElement.cpp
    src/UI/UICanvas.cpp
    src/UI/UIImage.cpp
    src/UI/UILabel.cpp
    src/UI/Font.cpp
    src/UI/UIButton.cpp
    src/UI/UIContextMenu.cpp
    src/UI/UIPanel.cpp
    src/UI/UIRenderer.cpp
    src/UI/UITextInput.cpp
    src/UI/UIFloatInput.cpp
    src/UI/UISlider.cpp
    src/UI/UIDebugOverlay.cpp
    src/UI/UIViewportPanel.cpp
    src/UI/UITextArea.cpp
    src/UI/ScrollView.cpp
    src/UI/UIScrollbar.cpp
    src/UI/UITreeView.cpp
    src/UI/UITreeViewItem.cpp
    src/UI/UITextureCache.cpp
    src/UI/Dock/DockNode.cpp
    src/UI/Dock/DockSplitNode.cpp
    src/UI/Dock/DockPane.cpp
    src/UI/Dock/DockTabBar.cpp
    src/UI/Dock/DockSplitter.cpp
    src/UI/Dock/DockDropOverlay.cpp
    src/UI/Dock/DockManager.cpp
    src/Physics/PhysicsWorld.cpp
    src/Physics/Collider.cpp
    src/Physics/RigidBody.cpp
    src/Audio/AudioEngine.cpp
    src/Audio/AudioClip.cpp
    src/Audio/SoLoudBackend.cpp
    src/Audio/SoundPlayer.cpp
    src/Components/AudioSource.cpp
    src/Components/AudioListener.cpp
)

if(WIN32 AND MSVC)
    target_sources(LeirEngine PRIVATE
        src/RHI/D3D12Backend.cpp
        src/RHI/WebGPUBackend.cpp
    )
endif()

# Shader compilation (Slang)
# The shader dirs are compile-time constants the engine needs on every platform
# regardless of whether slangc is installed (CI macOS/Linux runners don't ship
# slangc — only the Vulkan SDK's Windows install provides it).
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")
target_compile_definitions(LeirEngine PUBLIC
    LEIR_SHADER_DIR="${SHADER_OUTPUT_DIR}"
    # .slang sources (editor shader tooling reads them for export / hot-reload)
    LEIR_SHADER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

find_program(SLANGC slangc)
if(SLANGC)
    set(SHADER_SOURCES
        shaders/Basic.vert.slang
        shaders/Basic.frag.slang
        shaders/Sprite.vert.slang
        shaders/Sprite.frag.slang
        shaders/UI.vert.slang
        shaders/UI.frag.slang
        shaders/Grid.vert.slang
        shaders/Grid.frag.slang
        shaders/Gizmo.vert.slang
        shaders/Gizmo.frag.slang
        shaders/GizmoSolid.vert.slang
        shaders/GizmoSolid.frag.slang
    )
    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        # Base output name: strip the trailing .slang (Sprite.vert.slang -> Sprite.vert)
        string(REGEX REPLACE "\\.slang$" "" SHADER_BASE ${SHADER_NAME})
        set(SPV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.spv")
        # Derive stage from the filename (.vert. -> vertex, .frag. -> fragment)
        if(SHADER_NAME MATCHES "\\.vert\\.")
            set(SHADER_STAGE vertex)
        elseif(SHADER_NAME MATCHES "\\.frag\\.")
            set(SHADER_STAGE fragment)
        else()
            set(SHADER_STAGE "")
        endif()
        add_custom_command(
            OUTPUT ${SPV_FILE}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
            COMMAND ${SLANGC} -target spirv -profile spirv_1_3 -stage ${SHADER_STAGE}
                ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${SPV_FILE}
            COMMENT "Compiling ${SHADER_NAME}"
        )
        list(APPEND SHADER_SPVS ${SPV_FILE})

        # DXIL (D3D12) twin of the same shader, only built on MSVC (the D3D12
        # backend is not compiled on the MinGW CI preset).
        if(WIN32 AND MSVC)
            set(DXIL_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.dxil")
            add_custom_command(
                OUTPUT ${DXIL_FILE}
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}
                COMMAND ${SLANGC} -target dxil -profile sm_6_0 -stage ${SHADER_STAGE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER} -o ${DXIL_FILE}
                COMMENT "Compiling ${SHADER_NAME} (DXIL)"
            )
            list(APPEND SHADER_DXILS ${DXIL_FILE})
        endif()
    endforeach()
    add_custom_target(LeirShaders ALL DEPENDS ${SHADER_SPVS})
    add_dependencies(LeirEngine LeirShaders)
    if(WIN32 AND MSVC)
        add_custom_target(LeirShadersDXIL ALL DEPENDS ${SHADER_DXILS})
        add_dependencies(LeirShadersDXIL LeirShaders)
        add_dependencies(LeirEngine LeirShadersDXIL)
    endif()
else()
    message(WARNING "slangc not found — shaders will not be compiled at build time")
endif()

# WebGPU WGSL is GENERATED from the .slang at editor startup (single-source,
# see TODO_WEBGPU_SINGLE_SOURCE.md) — no hand-written .wgsl is copied anymore.
# The web export (.web.wgsl) is generated in the web build (Etapa C).

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
        nlohmann_json::nlohmann_json
    PRIVATE
        cereal::cereal
)

if(WIN32 AND MSVC)
    target_link_libraries(LeirEngine PRIVATE d3d12 dxgi)
    # wgpu-native header (Fase 5) — webgpu.h only; the DLL is loaded at runtime.
    FetchContent_GetProperties(wgpu)
    target_include_directories(LeirEngine PRIVATE ${wgpu_SOURCE_DIR}/include)
endif()

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
