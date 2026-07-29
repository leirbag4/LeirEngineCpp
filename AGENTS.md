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

## Settings System

- `LeirSettings` singleton (`Core/Settings.h`) reads `leir_settings.json` at executable level
- JSON via nlohmann/json (already a dependency)
- Sections:
  - `window`: `width`, `height`, `fullscreen`, `vsync`
  - `debug`: `ui_outlines` (toggles green UI bounding-box outlines), `show_overlay` (toggles UIDebugOverlay)
- If file doesn't exist, written with defaults on first `Load()` call
- `LeirSettings::Get().Load()` called in `main()` before app creation
- Editor reads settings for window size / fullscreen mode

## Input System

### Architecture

```
GLFW callbacks (key, char, mouse, cursor, scroll)
     ↓
EventQueue (thread-safe, std::mutex)
     ↓
EventQueue::Process()  ← per frame
     ├── Dispatch hooks:
     │   ├── PointerEvent → UICanvas::ProcessPointerEvent (hit-test, hover, click, focus)
     │   ├── KeyEvent     → UICanvas::SendKeyDown         (focused element)
     │   ├── CharEvent    → UICanvas::SendTextInput       (focused element)
     │   └── ScrollEvent  → Mouse scroll state updated
     │
     └── Update polling state:
         ├── Keyboard (current/previous arrays)
         ├── Mouse    (position, delta, scroll, button arrays)
         ├── Touch    (finger array, edge flags)
         └── Pointer  (unified button state from Mouse + Touch + Pen)
```

### File Structure

```
engine/include/LeirEngine/Input/
├── InputManager.h      → GLFW bridge, registers callbacks, pushes to EventQueue
├── InputEvent.h        → KeyEvent, PointerEvent, CharEvent, ScrollEvent variants
├── EventQueue.h        → Thread-safe queue, Process(), hook registration
├── Key.h               → Key enum + KeyCombo (operator| for flag-style combos)
├── PointerButton.h     → PointerButton enum (uint16_t, flags: Primary/Secondary/Auxiliary + aliases Left/Right/Middle)
├── Keyboard.h          → static polling API
├── Mouse.h             → static polling API (delegates internally)
├── Touch.h             → multi-touch API (desktop: mouse simulates 1 finger)
└── Pointer.h           → unified static API (Mouse + Touch + Pen)

engine/src/Input/
├── InputManager.cpp
├── EventQueue.cpp
├── Keyboard.cpp
├── Mouse.cpp
├── Touch.cpp
└── Pointer.cpp
```

### Frame Lifecycle (CoreApplication::Run)

```
glfwPollEvents()                          → callbacks push to EventQueue
EventQueue::Get().Process()               → dispatch hooks + update poll state
Scene::OnUpdate(dt) / EditorApp::OnUpdate  → reads poll state (delta, scroll, edges)
InputManager::GetInstance().Update()      → ResetFrame (saves current→previous for next frame)
Scene::OnRender()
```
- `EventQueue::Process()` calls `Keyboard::ProcessEvent()`, `Mouse::ProcessEvent()`, `Pointer::ProcessEvent()`, `Touch::ProcessEvent()`, `Mouse::ProcessScroll()` to update polling state per frame.
- `InputManager::Update()` (ResetFrame) runs **after** `OnUpdate()` so `Mouse::GetDelta()`, `GetScrollDelta()`, `WasPressed/Released` are available during update.

### API Reference

#### PointerButton (`PointerButton.h`)

```cpp
enum class PointerButton : uint16_t {
    None      = 0,
    Primary   = 1 << 0,   // mouse left / touch primary / pen tip
    Secondary = 1 << 1,   // mouse right / pen button
    Auxiliary = 1 << 2,   // mouse middle / pen eraser
    Extra1..8 = 1 << 3..10,
    Left   = Primary,     // alias
    Right  = Secondary,   // alias
    Middle = Auxiliary,   // alias
};
// Bitwise operators: |, &, ~, Any(), Has()
```

#### Key + KeyCombo (`Key.h`)

```cpp
enum class Key : int32_t;  // matches GLFW key codes (Space=32, A=65, ..., Escape=256, Left=263, etc.)

struct KeyCombo {
    std::vector<Key> keys;
    KeyCombo(Key k);                            // implicit from single key
    KeyCombo(std::initializer_list<Key> ks);
};
KeyCombo operator|(Key a, Key b);               // Key::A | Key::B → KeyCombo{A, B}
KeyCombo operator|(KeyCombo a, Key b);          // append
```

#### Keyboard (`Keyboard.h`)

```cpp
Keyboard::IsDown(Key key);                          // single key
Keyboard::IsDown(KeyCombo combo);                   // all keys in combo must be down
Keyboard::IsUp(Key key);
Keyboard::WasPressed(Key key);                      // true only on the frame key went down
Keyboard::WasReleased(Key key);                     // true only on the frame key went up
// Usage: Keyboard::IsDown(Key::A | Key::B) → KeyCombo created via operator|
```

#### Mouse (`Mouse.h`)

```cpp
Mouse::IsDown(PointerButton btn = PointerButton::Left);     // default: left
Mouse::IsUp(PointerButton btn = PointerButton::Left);
Mouse::WasPressed(PointerButton btn = PointerButton::Left);
Mouse::WasReleased(PointerButton btn = PointerButton::Left);
Mouse::GetX(), GetY(), GetPos(), GetDelta(), GetScrollDelta();
```

#### Touch (`Touch.h`)

```cpp
struct TouchFinger { int id; glm::vec2 pos; float pressure; bool down/pressed/released; };

Touch::GetFingers(), GetCount();
Touch::IsDown(int fingerId = 0);
Touch::WasPressed(int fingerId), WasReleased(int fingerId);
```
- Desktop: mouse simulates finger 0 (for testing)
- Mobile-ready: real multi-touch backend added later per platform

#### Pointer (`Pointer.h`) — unified

```cpp
Pointer::IsDown(PointerButton btn = PointerButton::Primary);
Pointer::AreDown(PointerButton btns);            // all flags must be set
Pointer::IsUp(PointerButton btn);
Pointer::WasPressed(PointerButton btn);
Pointer::WasReleased(PointerButton btn);
Pointer::GetX(), GetY(), GetPos(), GetDelta();
```
- `Pointer::IsDown(Primary)` = `Mouse::IsDown(Left)` OR `Touch::IsDown(0)`
- Write game code with Pointer for cross-platform input (PC, tablet, phone) in one line

### InputEvent Variant (`InputEvent.h`)

```cpp
enum class EventAction : uint8_t { Press, Release, Repeat, Move, Cancel };
enum class PointerSource : uint8_t { Mouse, Touch, Pen };

struct KeyEvent      { Key key; int scancode; EventAction action; int mods; };
struct PointerEvent  { PointerSource source; int pointerId; glm::vec2 pos/delta; PointerButton button; EventAction action; float pressure; };
struct CharEvent     { uint32_t codepoint; int mods; };
struct ScrollEvent   { glm::vec2 offset; };

using InputEvent = std::variant<KeyEvent, PointerEvent, CharEvent, ScrollEvent>;
```

### UICanvas Integration

- `UICanvas::ConnectToInputSystem()` registers hooks on `EventQueue`:
  - PointerEvent → `ProcessPointerEvent()` (hit-test + hover tracking + click/focus dispatch)
  - CharEvent → `SendTextInput()` (focused element)
  - KeyEvent → `SendKeyDown()` (focused element)
- Called once after canvas creation: `m_Canvas->ConnectToInputSystem()`
- `DisconnectFromInputSystem()` called in destructor (clears hooks)
- `UICanvas::GetHoveredElement()` returns the deepest hovered element for editor viewport hit-testing
- Old `UpdatePointer(pos, down, up)` replaced by `ProcessPointerEvent(const PointerEvent&)` which handles each event individually

### Event Flow Detail

| Action | GLFW Callback | Event | UI Dispatch | Poll State |
|---|---|---|---|---|
| Key press | `KeyCallback(GLFW_PRESS)` | `KeyEvent{Press}` | `canvas->SendKeyDown(key)` | `Keyboard::current[key]=true` |
| Key release | `KeyCallback(GLFW_RELEASE)` | `KeyEvent{Release}` | — | `Keyboard::current[key]=false` |
| Char input | `CharCallback` | `CharEvent{codepoint}` | `canvas->SendTextInput(cp)` | — |
| Mouse move | `CursorPosCallback` | `PointerEvent{Move}` | `canvas->ProcessPointerEvent(e)` | `Mouse::position/delta` |
| Mouse press | `MouseButtonCallback(GLFW_PRESS)` | `PointerEvent{Press}` | `canvas->ProcessPointerEvent(e)` | `Mouse::current[btn]=true` |
| Mouse release | `MouseButtonCallback(GLFW_RELEASE)` | `PointerEvent{Release}` | `canvas->ProcessPointerEvent(e)` | `Mouse::current[btn]=false` |
| Scroll | `ScrollCallback` | `ScrollEvent{offset}` | — | `Mouse::scrollDelta` |

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

## UIRenderer Draw Layers

The UI renderer draws in 3 phases (bottom to top):

1. **Regular UI** — canvas background, panels (Hierarchy, Inspector), buttons, sliders, inputs, labels, debug outlines
2. **Viewports** — `ViewportDraw` quads from `UIViewportPanel` (offscreen `RenderTexture`)
3. **Debug overlay** — `UIDebugOverlay` panel + all children (FPS, mouse, buttons, keys, hover, event labels)

Detection: elements whose `Name()` starts with `"Debug"` are routed to `BuildBatchDebug()` (separate `m_DebugVertices`/`m_DebugQuadTextures`). Everything else goes to `BuildBatch()`.

Vertex buffer layout: `[regular UI vertices] [viewport vertices] [debug vertices]`

```
void UIRenderer::Flush(VkCommandBuffer cmd) {
    // Layout: [regular UI] [viewport] [debug overlay]
    // Draw:   1. regular quads (bottom)
    //         2. viewport quads (middle)
    //         3. debug quads (top)
}
```

## RenderTexture System

- `RenderTexture` class (`Rendering/RenderTexture.h/.cpp`) creates an offscreen render target with color + depth VkImage/VkImageView/VkFramebuffer/VkSampler.
- Uses its own render pass compatible with swapchain 3D pass (both B8G8R8A8_SRGB + D32_SFLOAT), so existing pipelines work.
- `BeginRender(VkCommandBuffer, VkClearValue, float depth)` — transitions images, begins pass
- `EndRender(VkCommandBuffer)` — transitions back to shader-read optimal for sampling
- `GetDescriptorInfo()` — for use as a sampled texture in UI

```cpp
m_ViewportRT = std::make_unique<RenderTexture>(device, width, height);
m_Material->RecreatePipeline(m_ViewportRT->GetRenderPass());

// Frame:
m_ViewportRT->BeginRender(cmd, clearColor, 1.0f);
m_RenderPipeline->Render(cmd, scene);
m_ViewportRT->EndRender(cmd);
```

## UIViewportPanel

- `UIViewportPanel` (`UI/UIViewportPanel.h/.cpp`) — UI element that holds a `RenderTexture*`
- Inherits from `UIElement` (not `UIPanel`), so it does NOT draw a background quad
- `SetRenderTexture(RenderTexture*)` / `GetRenderTexture()`
- `ScreenToViewport(float x, float y)` converts screen coords to viewport-local UV coords
- Detected in `UIRenderer::Render()` via `dynamic_cast<UIViewportPanel*>`, creates a `ViewportDraw` entry

## VulkanDevice Additions

```cpp
// In VulkanDevice:
bool BeginFrame(bool skipRenderPass = false);   // skipRenderPass: don't start 3D render pass
void BeginSwapchainOverlay();                     // transition swapchain + begin overlay pass
```

- `BeginFrame(true)` used by editor to skip the 3D scene render pass (renders to offscreen RT instead)
- `BeginSwapchainOverlay()` transitions swapchain image and begins the overlay render pass — called after viewport rendering is complete, before UI rendering

## Editor Layout

```
Canvas
  └── root (UIPanel "EditorRoot", full screen, alpha 1.0)
        ├── UIViewportPanel ("Viewport", anchor 0,0–1,1 offset 200,0–-220,-30)
        ├── Hierarchy (UIPanel, anchor 0,0–0,1 offset 0,0–200,-30)
        │     └── HierarchyTitle (UILabel)
        ├── Inspector (UIPanel, anchor 1,0–1,1 offset -220,0–0,-30)
        │     └── InspectorTitle (UILabel)
        ├── BottomBar (UIImage, anchor 0,1–1,1 offset 0,-30–0,0)
        └── StatusLabel (UILabel, anchor 0,1–0,1 offset 8,-28–600,0)
[DebugOverlay panel added as sibling of root]
```

### EditorCamera

```cpp
struct EditorCamera {
    float yaw = 0.0f;
    float pitch = -20.0f;
    float distance = 8.0f;
    glm::vec3 target = {0.0f, 0.0f, 0.0f};

    glm::vec3 GetPosition() const;  // spherical → cartesian
    void Orbit(float dx, float dy); // left drag
    void Pan(float dx, float dy);   // middle drag
    void Zoom(float delta);         // scroll
};
```

- Scene camera position/rotation synced from EditorCamera each frame
- Only responds when hovered element is inside the ViewportPanel

## OnRender Flow (Editor)

```cpp
void OnRender() override {
    m_VulkanDevice->BeginFrame(true);             // skip 3D pass
    m_ViewportRT->BeginRender(cmd, clear, 1.0f);  // offscreen render
    m_RenderPipeline->Render(cmd, scene);
    m_ViewportRT->EndRender(cmd);
    m_VulkanDevice->BeginSwapchainOverlay();       // start overlay pass
    m_UIRenderer->Render(cmd, m_Canvas.get());     // UI (3 layers)
    m_VulkanDevice->EndFrame();
}
```

## Previous Changes Summary

- `Settings.h`: added `debug.show_overlay` field
- `UICanvas::GetHoveredElement()` added for editor camera viewport detection
- `Keyboard.h`: added `#include <string>` for `GetPressedKeysString()`
- `EventQueue::Process()` now calls `Keyboard/Mouse/Pointer/Touch::ProcessEvent()` to update polling state each frame
- `UIDebugOverlay`: event display duration increased from 1 frame to 120 frames (~2s)
- `CoreApplication::Run()`: `InputManager::Update()` (ResetFrame) moved **after** `OnUpdate()` so delta/scroll/edge detection work during update
- `UIRenderer`: `ViewportDraw` struct, `m_ViewportDraws`/`m_VpDescCache`, `BuildBatchDebug()` for 3-layer flush
- `engine/CMakeLists.txt`: added `RenderTexture.cpp`, `UIViewportPanel.cpp`
