# LeirEngine

A cross-platform C++ game engine built from scratch with Vulkan.

## Tech Stack

| Area | Choice |
|---|---|
| Language | C++20 |
| Build | CMake 3.20+ (superbuild with FetchContent) |
| Windowing/Input | GLFW 3.4 |
| Graphics | Vulkan 1.3 + MoltenVK (macOS/iOS) |
| Math | GLM (header-only) |
| Physics | Jolt Physics |
| Audio | SoLoud |
| Image loading | stb_image, stb_image_write |
| Fonts | stb_truetype + FreeType |
| Serialization | nlohmann/json, cereal (binary) |
| Logging | spdlog |
| Docs | Doxygen → HTML |

## Project Layout

```
LeirEngine/
├── CMakeLists.txt          # superbuild root
├── CMakePresets.json
├── engine/                 # LeirEngine.dll / .so
│   ├── include/LeirEngine/
│   └── src/
├── editor/                 # LeirEngineEditor.exe
├── dependencies/           # FetchContent declarations
└── docs/
```

editors/ → editor/

## Architecture

- `CoreObject` base class with `Transform` (parent/child hierarchy via `addChild`/`getParent`), `addComponent<T>`, `getComponent<T>` (Unity-style component system)
- `Object3D` and `Object2D` inherit from `CoreObject`
- `Scene` owns objects, drives update/render
- Editor links against the engine DLL/SO and uses public API only
- Editor UI built with custom UI system (own widget library, not Dear ImGui/Qt)

## Prerequisites

- **Vulkan SDK** 1.3.296+ from [LunarG](https://vulkan.lunarg.com/) — sets `VULKAN_SDK` env var
- **CMake** 3.20+ (bundled with VS 2022 at `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`)
- **Visual Studio 2022** with "Desktop development with C++" workload
- **glslc** (shader compiler) — included with Vulkan SDK at `$VULKAN_SDK/Bin/glslc.exe`

## Build Commands

```bash
# First configure (run once, or after adding deps)
cmake --preset=windows-debug

# Build engine + editor
cmake --build build/windows-debug

# Outputs:
#   build/windows-debug/engine/Debug/LeirEngine.dll
#   build/windows-debug/editor/Debug/LeirEngineEditor.exe

# Linux via WSL
cmake --preset=linux-debug
cmake --build build/linux-debug
```

## Notable Design Decisions

- All public symbols use `LEIR_API` macro (`__declspec` on Windows, `visibility("default")` on Unix)
- Shaders compiled GLSL→SPIR-V at build time via `glslc`
- Jolt Physics configured with multithreading enabled
- SoLoud for cross-platform audio (zlib license)
- Documentation auto-generated from Doxygen comments in headers

## Jolt Physics Integration

### Init order (required)
PhysicsWorld::Init() must call in this exact order:
1. `RegisterDefaultAllocator()`
2. `Factory::sInstance = new Factory()`
3. `RegisterTypes()` — without this, CollisionDispatch is uninitialized → AV on first contact

### On Shutdown
1. Destroy all Scene objects (RigidBody dtors clean up Jolt bodies via RemoveBody + DestroyBody)
2. Call `PhysicsWorld::Shutdown()` which calls `UnregisterTypes()`, deletes Factory
3. Use scoped scene (`{ Scene s; ... }`) before Shutdown to ensure bodies are removed while PhysicsSystem is still alive

### Layer configuration
Use table-based classes (BroadPhaseLayerInterfaceTable, ObjectLayerPairFilterTable, ObjectVsBroadPhaseLayerFilterTable) with 2 layers:
- `PhysicsLayers::NON_MOVING (0)` for Static bodies
- `PhysicsLayers::MOVING (1)` for Dynamic/Kinematic bodies

### Public headers must not expose Jolt types
Conversion helpers live in internal `PhysicsConversions.h`, not in public includes.

## Class Hierarchy

```
CoreObject (Transform, components, parent/children, name, uuid, active)
├── Object3D  (world transform 3D, bounding box)
└── Object2D  (transform 2D, sorting layer)

Component (virtual onAwake/onStart/onUpdate/onDestroy)
├── MeshRenderer, Camera, Light
├── RigidBody, Collider, CharacterController
├── AudioSource, AudioListener
├── CanvasRenderer
└── ScriptComponent (future)

Scene (owns objects, physics world, render queue)
```

## Conventions

- `PascalCase` for classes, structs, namespaces, methods
- `camelCase` for variables, parameters
- Headers in `include/LeirEngine/<Module>/`, sources in `src/`
- Each public header includes `Core/Export.h` for `LEIR_API`
