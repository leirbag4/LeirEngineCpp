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
| Logging | XConsole (propio, sin deps) |
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

- `LeirSettings` singleton (`Core/Settings.h`) reads/writes `settings.json` in the platform config dir:
  - Windows: `%APPDATA%\LeirEngine\settings.json`
  - macOS: `~/Library/Application Support/LeirEngine/settings.json`
  - Linux: `$XDG_CONFIG_HOME/LeirEngine/settings.json` (falls back to `~/.config/LeirEngine/`)
- JSON via nlohmann/json (already a dependency)
- Sections:
  - `window`: `width`, `height`, `pos_x`, `pos_y` (`INT_MIN` = unset → centered on first run), `fullscreen`, `maximized`, `vsync`, `hidpi` (uses system DPI scale; `false` = fixed 1× UI)
  - `debug`: `ui_outlines` (toggles green UI bounding-box outlines), `show_overlay` (toggles UIDebugOverlay)
  - `layout`: `hierarchy_width`, `inspector_width` (legacy, unused since the dock system)
  - `dock`: `layout` (serialized dock tree as JSON string; empty = default layout)
- `Save()` creates the config directory if missing (`create_directories`)
- If file doesn't exist, written with defaults on first `Load()` call
- `LeirSettings::Get().Load()` called in `main()` before app creation
- Editor reads settings for window size / fullscreen / position / maximized and the dock layout; saves on splitter drag end, panel close, and shutdown (`OnShutdown`)
- Window placement persistence: `CoreApplication` tracks the "normal rect" (size/pos only when not maximized and not fullscreen) via GLFW size/pos callbacks, so exiting maximized or fullscreen never corrupts the saved windowed rect. Position is restored via `glfwSetWindowPos` (centered if unset), maximized via `glfwMaximizeWindow`, both ignored in fullscreen

## XConsole Logging

Own logging system (`Core/Log.h` + `Core/Log.cpp`), no external dependency (replaced spdlog).

- `enum class LogLevel { Trace, Debug, Info, Warning, Error }` — levels are a verbosity *filter*: messages below the current level are discarded at the source (Trace/Debug are "silent by default").
- API: `XConsole::Println` (Info), `PrintWarning`, `PrintError` (Error → **stderr**), `Trace`, `Debug`, `SetLevel`, `GetLevel`, `GetMessages()`, `GetVersion()`, `Clear()`. All are `Leir::XConsole::`.
- Formatting: `{}` + specs `{:.Nf}`, `{:Nd}`, `{:0Nd}` via a runtime variadic formatter (`std::any` + `std::ostringstream`). Format strings may be non-literal.
- Output: `[HH:MM:SS.mmm] [level] message` → stdout (info/warn/debug/trace) and stderr (error). **Only Info/Warning/Error are retained** in the 1000-entry ring buffer (`GetMessages`) that feeds the editor `ConsolePanel` — Trace/Debug are debug-only and would evict useful messages. `LogMessage` = `{ level, text, time }` (`time` = `HH:MM:SS.mmm`).
- **RULE (learned from `TODO_UI_EVENT_FLOOD.md`)**: any UI/renderer-internal or debug log must be `Trace`/`Debug`, NEVER `Println`/`PrintWarning`/`PrintError` inside per-frame paths — Info/Warning/Error enter the ring buffer → bump `GetVersion()` → `ConsolePanel::Refresh()` rebuilds all lines every frame → FPS churn, and per-frame `Warning` from e.g. `Flush()` overflow creates a feedback loop. Only real system messages (device created, shader compiled, load errors) should be Info+.
- `GetVersion()` returns a monotonic counter bumped on every retained message and on `Clear()` — lets the ConsolePanel detect new messages without snapshotting the buffer each frame.
- Thread-safe (std::mutex; Jolt runs multithreaded). State lives in function-local statics in `Log.cpp`.
- `ConsolePanel` (dockeable, filter buttons Info/Warn/Error, Clear, timestamps, wheel + scrollbar, auto-follow) — **implemented**, see `TODO_UI_CONSOLE.md`.

## HiDPI (DPI awareness)

- The engine runs **per-monitor DPI-aware** (GLFW calls `SetProcessDpiAwarenessContext` on init).
  On Windows `glfwGetWindowSize == glfwGetFramebufferSize` = **physical pixels**; logical =
  `framebuffer ÷ contentScale`. On macOS/Linux window size is logical, framebuffer physical.
- `CoreApplication` works in **logical units**: `GetWidth()/GetHeight()` (logical, derived from
  `framebuffer ÷ GetContentScale()`), `GetFramebufferWidth/Height()` (physical),
  `GetContentScale()` (system scale, or 1.0 when HiDPI disabled via `SetHidpiEnabled`).
- The window is created at `logical × scale` physical pixels when HiDPI is enabled, so the
  windowed area stays the same at any DPI. `CenterWindow` uses native units (no conversion).
- **Input**: GLFW cursor is physical on Windows; `InputManager::ToLogical()` divides by the
  effective scale there (no-op on macOS/Linux). Set via `InputManager::SetContentScale()` by
  `CoreApplication` before `Init` and on `glfwSetWindowContentScaleCallback`
  (`OnContentScaleChanged()` virtual).
- **Rendering**: the swapchain extent is physical (Vulkan surface caps). `UIRenderer` push-constant
  `screenSize` = **logical canvas size** (vertices are logical). The editor viewport `RenderTexture`
  is created/resized at `logical × dpr`. Viewport/scissor stay physical.
- Toggle: `settings.window.hidpi` (default `true`); `false` = old "fixed 1×" behavior.
  See `TODO_HIDPI.md` for full concept and platform notes.

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

## UI Clipping, Scrollbars & Wheel

See `TODO_UI_SCROLLBARS.md` for the full concept and implementation notes.

- **Clipping**: `UIElement::SetClip(bool)` makes the element's computed rect a clip
  region for its descendants. `UIRenderer::Render` uses a **recursive** walk
  (`RenderElement(elem, clip, isDebug)`) that intersects the active clip with each
  clip-enabled rect; fully-outside subtrees are culled on the CPU. `Flush` applies
  `vkCmdSetScissor` per draw (each quad is its own `vkCmdDraw`), tracking
  `lastScissor` across all 3 layers. Clip rects are logical; scissor is physical →
  `UIRenderer::SetContentScale(float)` (editor sets it in `OnInit` +
  `OnContentScaleChanged`). The UI pipeline already used dynamic viewport/scissor.
- **Wheel**: `UIElement::OnScroll(float delta)` virtual (return true to consume).
  `UICanvas` registers the existing `EventQueue` `ScrollHook` and dispatches the
  `ScrollEvent` to the hovered element, propagating up the parent chain.
- **`UIScrollbar`** (`UI/UIScrollbar.h/.cpp`): track (own background) + thumb (child
  `UIPanel`), normalized value [0,1], `SetRange(viewport, content)`, drag with
  `CapturePointer` (click track = jump, click thumb = grab). Orientation H/V.
  No UIRenderer branch needed (composed of primitives).
- **`ScrollView`**: sets `SetClip(true)`, positions content at **absolute** coords
  (`cr - scrollOffset` — negative offset moves content UP, matching the header doc
  "positive = content moved up"; **was `cr + scrollOffset`, which inverted drag /
  wheel / thumb direction and made the top lock wrong**), clamps to `[0, maxScroll]`
  (`maxScrollY = contentSize.y - viewport.h`, content size via `GetContentSize()`),
  scrolls on wheel (`delta × lineHeight`) and drag (captured, touch-style:
  content follows the finger via `off.y = m_ScrollStart.y - delta.y`), and owns a
  built-in **vertical `UIScrollbar`** (visible only on overflow, synced both ways
  in `OnLayoutComputed`).
- **`ScrollView::SyncScrollbar` positions the scrollbar with `AnchorSet::TopLeft` +
  ABSOLUTE offsets** (`{cr.x + cr.z - w - 2, cr.y + 2, cr.x + cr.z - 2, cr.y + cr.w - 2}`),
  same convention as the content. **Do NOT use relative anchors here** — the old
  `{1,0,1,1}` anchor + relative offsets clobbered the parent-propagated absolute
  position, so the track was drawn misaligned and clipped by the ScrollView's own
  clip → the visible track didn't cover the container height and shrank during
  resize. (Fixed 2026-08-06, user-verified.)

## RenderTexture System

- `RenderTexture` class (`Rendering/RenderTexture.h/.cpp`) creates an offscreen render target with color + depth VkImage/VkImageView/VkFramebuffer/VkSampler.
- Uses its own render pass compatible with swapchain 3D pass (both B8G8R8A8_SRGB + D32_SFLOAT), so existing pipelines work.
- `BeginRender(VkCommandBuffer, VkClearValue, float depth)` — transitions images, begins pass
- `EndRender(VkCommandBuffer)` — transitions back to shader-read optimal for sampling
- Owns its own UI descriptor set: `GetDescriptorSet()` (combined image sampler, layout structurally identical to UIRenderer's). Created in ctor, re-written in `Resize()` (`UpdateDescriptor`), destroyed in dtor with its own layout + pool. UI renders the viewport by binding `rt->GetDescriptorSet()` — no manual invalidation needed (see `TODO_DESCRIPTORS_VIEWPORT.md`).

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

## Editor Layout (dock system)

The editor's layout is built with the dock system (see "Dock System" below). The old
fixed-width panels (`ApplyPanelLayout`, `UISplitter`, `hierarchy_width`/`inspector_width`)
are gone; the `DockManager` is the full-screen root and panels are dockable tabs.

```
Canvas
  ├── DockManager ("EditorDock", Stretch, offset 0,0–0,-30)   ← bottom 30px free
  │     └── DockSplitNode H [0.17, 0.66, 0.17]
  │           ├── DockPane: [DockTabBar: Hierarchy] → content "Hierarchy"
  │           ├── DockSplitNode V [0.8, 0.2]
  │           │     ├── DockPane: [DockTabBar: Viewport] → content UIViewportPanel
  │           │     └── DockPane: [DockTabBar: Test | Camera | Debug Text | Text Area | Text Area Wrap]
  │           │                       → UITestPanel / CameraTestPanel / DebugTextPanel / TextAreaDebugPanel / TextAreaWrapPanel
  │           └── DockPane: [DockTabBar: Inspector] → content "Inspector" (+ InspectorTransformPanel)
  ├── BottomBar (UIImage, anchor 0,1–1,1 offset 0,-30–0,0)
  └── StatusLabel (UILabel, anchor 0,1–0,1 offset 8,-28–600,0)
[UIDebugOverlay panel added as sibling of DockManager (explicit overlay layer)]
```

Default ratios: H 0.17/0.66/0.17, inner V 0.8/0.2. Panels Hierarchy/Viewport/Inspector are
not closeable; the four debug panels are. Drag tabs to re-dock (edges = split, center =
tab-merge); drag the 6px splitters to resize; layout persists to
`settings.json → dock.layout` on splitter drag-end, close, and shutdown.

## Dock System

Docking engine in `engine/include/LeirEngine/UI/Dock/` (headers) + `engine/src/UI/Dock/`
(sources). The dock tree IS the UI tree: every node is a `UIElement`/`UIPanel`, so it
inherits layout, hit-test, input and rendering.

- `DockPanel` — registry entry `{ id, title, content: UIElement*, closeable }`. Owned by
  the `DockManager` (`unique_ptr`); the content subtree is owned by the caller.
- `DockNode` → `DockSplitNode` (`orientation` H/V, `ratios[]`, 6px `DockSplitter`s) and
  `DockPane` (Column: `DockTabBar` + active content host).
- `DockTabBar` / `DockTab` — Row of tabs; pointer-down activates the tab and starts a dock
  drag via `DockManager::BeginTabDrag`; rightmost 16px of a closeable tab closes it.
- `DockManager` — root: panel registry, tree ops (`SplitPane`/`MergeIntoPane`/`ClosePanel`),
  drag & drop with `DockDropOverlay` (ghost + highlighted zone, overlay layer), JSON
  serialization via nlohmann, `SetOnLayoutChanged` callback for persistence.
- `DockSplitNode::ComputeLayout` — overrides the virtual `UIElement::ComputeLayout` to
  position children by ratios (absolute TopLeft + per-child `ComputeLayout` calls).
- `DockDropZone` — `Left/Right/Top/Bottom` = split, `Center` = tab-merge (center 50% box).
- **Tab reorder (same pane)**: dragging a tab over its own pane's **tab bar** (when the pane
  has other tabs) reorders it on drop — the tab is repositioned by X against the sibling
  tab centers (`DockPane::ReorderTabTo`; `UIElement::InsertChildAt` +
  `DockTabBar::InsertTab` + `DockPane::InsertTab`). The zone highlight is hidden during
  this gesture so it reads as a reorder, not a split. `OnPointerUp` uses the real drop
  position (the pointer is captured during drag, so there is no per-tab hover).
- **Split on own shared pane**: edge drops (Left/Right/Top/Bottom) on a pane that *also
  hosts the dragged tab* split the pane when it has ≥2 tabs (`SplitPane`'s self-drop guard
  only short-circuits when `GetTabCount() <= 1`). The dragged tab leaves the shared pane
  into the new zone; the siblings stay behind. Center drop on the same pane still just
  focuses the tab.
- **Deferred close**: `DockTab` close clicks call `RequestClosePanel` (the tab must finish
  dispatching before being deleted); `DockManager::Process()` runs it once per frame (editor
  calls it in `OnUpdate`). After any structural mutation the manager calls
  `ClearDanglingPointers()` → `UICanvas::ClearHoverAndFocus()` (clears hover/focus without
  callbacks — safe after element deletion).
- **Ratios**: `DockSplitNode::AddNode` does NOT normalize (keeps the requested fractions
  exact); `DragSplitter`/`RemoveNode` re-normalize. Persisted ratios are stable across
  save/load round-trips.
- Persistence: `LeirSettings::dock.layout` (string JSON). Editor restores on startup
  (`LoadLayout`), falls back to `BuildDefaultLayout` when empty/invalid, and
  `PlaceMissingPanels` re-adds active registered panels not present in the tree.
- Editor teardown order (`OnShutdown`): serialize layout → save settings → remove the dock
  manager from the canvas → delete it (content stays owned by the editor) → destroy the
  viewport RT → destroy the scene.
- Overlay routing in `UIRenderer`: an element (or any ancestor) with `IsOverlayLayer()` is
  drawn in the top batch. The name-prefix `"Debug"` heuristic was removed;
  `UIDebugOverlay` and `DockDropOverlay` set the flag explicitly.

### EditorCamera

`editor/src/Camera/EditorCamera.h/.cpp` — free-fly camera class replacing the old inline struct.

```cpp
class EditorCamera {
public:
    EditorCamera();
    void Update(float deltaTime);
    glm::vec3 GetPosition() const;
    float GetYaw() const;
    float GetPitch() const;
    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::quat GetRotation() const;
    void SetPosition(const glm::vec3& pos);
    void SetYaw(float y);
    void SetPitch(float p);
private:
    glm::vec3 m_Position = {0.0f, 2.0f, 4.0f};
    float m_Yaw = 0.0f;
    float m_Pitch = -20.0f;
};
```

**Controls** (only when viewport is hovered and the respective button is held):

| Input | Action |
|---|---|
| Right-click + drag | Yaw/Pitch |
| Middle-click + drag | Pan (horizontal: camera right, vertical: world Y) |
| W/S | Move forward/backward (camera forward) |
| A/D | Move left/right (camera right) |
| E/Q | Move up/down (world Y) |
| Shift | 3x movement speed |

**Update** reads `Mouse::IsDown(Right/Middle)`, `Mouse::GetDelta()`, and `Keyboard::IsDown(W/A/S/D/Q/E/Shift)` directly.

### Camera Sync (bidirectional)

In `EditorApp::OnUpdate`:

```cpp
if (rightDown || middleDown)
    m_EditorCamera.Update(deltaTime);

if (cameraObj) {
    if (cameraControlled) {
        // EditorCamera → scene camera
        cameraObj->GetTransform().SetLocalPosition(m_EditorCamera.GetPosition());
        cameraObj->GetTransform().SetLocalRotation(m_EditorCamera.GetRotation());
    } else {
        // scene camera → EditorCamera (panel edits flow back)
        auto pos = t.GetLocalPosition();
        auto euler = glm::degrees(glm::eulerAngles(t.GetLocalRotation()));
        m_EditorCamera.SetPosition(pos);
        m_EditorCamera.SetYaw(euler.y);     // Y euler → yaw
        m_EditorCamera.SetPitch(euler.x);   // X euler → pitch
    }
}
```

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

## UI Widgets

### UIFloatInput (`engine/include/LeirEngine/UI/UIFloatInput.h`)

Inherits `UITextInput`, filters input to `[0-9]` `+` `-` `.`, commits on Enter/Blur.

```cpp
class UIFloatInput : public UITextInput {
    void SetValue(float v);
    float GetValue() const;
    void SetOnValueChanged(std::function<void(float)>);
    bool OnTextInput(uint32_t codepoint) override;
    bool OnKeyDown(int key) override;
    void OnFocus() override;
    void OnBlur() override;
};
```

### UIDragFloatInput (`editor/src/UI/UIDragFloatInput.h/.cpp`)

Inherits `UIPanel` (gray background), contains `UILabel` + `UIFloatInput` in a Row layout. Drag-to-change on the label area.

```cpp
class UIDragFloatInput : public UIPanel {
    void SetLabel(const std::string& text);
    void SetValue(float v);
    float GetValue() const;
    void SetOnValueChanged(std::function<void(float)>);
    bool OnPointerDown(const glm::vec2& pos) override;   // starts drag on label hit
    void OnPointerMove(const glm::vec2& pos) override;    // delta → value
    bool OnPointerUp(const glm::vec2& pos) override;      // ends drag
};
```
- Drag captures pointer via `UICanvas::CapturePointer()` (routes all Move/Release to the drag element)
- Sensitivity: 100 pixels = 1.0 unit

### UITestPanel / CameraTestPanel (`editor/src/UI/`)

- `UITestPanel` — floating panel with Position/Rotation/Scale fields bound to Cube transform
- `CameraTestPanel` — floating panel with Position/Rotation fields bound to Camera transform
- Each field has an `OnValueChanged` callback that writes directly to the `Object3D::Transform` (immediate mode)
- `Refresh()` reads from the scene object and updates input values every frame
- Both panels start with `"Debug"` in their name so the whole subtree routes to the debug overlay layer

## Layout System

### Parent position propagation

In `UIElement::ComputeFreeLayout/ComputeRowLayout/ComputeColumnLayout`, the parent's `m_ComputedRect.x/y` is added to each child's `m_Rect.offset` **before** calling `child->ComputeLayout()`. This ensures grandchildren inherit the full absolute position chain — labels and inputs inside nested layouts (e.g., `UIDragFloatInput` inside a Row inside a `UITestPanel`) are positioned correctly.

### Debug overlay detection in UIRenderer

An element is routed to `BuildBatchDebug()` (debug overlay layer) if its own name or any ancestor's name starts with `"Debug"`. This is checked by walking the parent chain in `Render()` before batching.

## UICanvas Event Propagation

### Pointer event propagation

`ProcessPointerEvent` propagates `OnPointerDown`/`OnPointerUp` up the parent chain when the deepest child returns `false`:

```cpp
UIElement* target = hit;
while (target && !target->OnPointerDown(pos))
    target = target->GetParent();
```

### Pointer capture

`CapturePointer(UIElement*)` / `ReleasePointer()` — when an element captures the pointer, all subsequent Move/Release events are routed directly to it without hit-testing. Used by `UIDragFloatInput` during drag.

```cpp
// Capture in OnPointerDown:
Leir::UIElement* e = this;
while (e) {
    auto* c = dynamic_cast<Leir::UICanvas*>(e);
    if (c) { c->CapturePointer(this); break; }
    e = e->GetParent();
}

// In ProcessPointerEvent:
if (m_CaptureElement && e.action != EventAction::Press) {
    if (e.action == EventAction::Move)
        m_CaptureElement->OnPointerMove(pos);
    else if (e.action == EventAction::Release) {
        m_CaptureElement->OnPointerUp(pos);
        m_CaptureElement = nullptr;
    }
    return;
}
```

### Focus management

- `SetFocus(UIElement*)` — calls `OnBlur()` on old focus, `OnFocus()` on new
- `ClearFocus()` — clears focus
- `SendTextInput(uint32_t codepoint)` — forwards char to `m_FocusElement->OnTextInput()`
- `SendKeyDown(int key)` — forwards key to `m_FocusElement->OnKeyDown()`

## UITextInput — Text Input System

### Keyboard navigation & deletion
- `UITextInput::OnKeyDown` handles: Backspace, Delete, Left/Right/Home/End arrows, each with `ResetCaretBlink()`
- `DeleteForward()` — deletes character after cursor (Delete key)
- `UIFloatInput::OnKeyDown` forwards unhandled keys to `UITextInput::OnKeyDown`

### m_Focused fix
- Removed shadow `bool m_Focused` from `UIFloatInput` — now uses inherited `UITextInput::m_Focused`

### Text color API
- `SetTextColor(Vector4)` / `GetTextColor()` on `UITextInput`
- `UIRenderer` uses `input->GetTextColor()` instead of hardcoded white

### Cursor positioning & caret
- `GetCursorX()` — X position of cursor within text (using glyph advances)
- `GetCursorXAt(int charIndex)` — X of any character index
- `GetCharIndexAtX(float localX)` — character index from local X (click-to-position)
- `TickCaret()` called each frame in `UIRenderer::Render`; blink uses `(m_FrameCounter/30)%2`
- Caret rendered as 1px white quad at computed cursor X, vertically centered for single-line, aligned to line for UITextArea
- `OnPointerDown` sets cursor via `GetCharIndexAtX`, `OnPointerMove` updates cursor during drag

### Drag selection & pointer capture
- `m_Dragging` flag: true only while mouse button held after OnPointerDown
- `OnPointerUp` clears `m_Dragging`
- `OnPointerMove` only moves cursor when `m_Dragging` is true
- `CaptureDragPointer()` walks up to `UICanvas` and calls `CapturePointer(this)` — all subsequent Move/Release events go to the input even outside its bounding box (fixes drag-past-border behavior)
- `OnBlur` releases captured pointer and clears `m_Dragging`
- Selection by click-drag: first `OnPointerMove` during drag sets `m_SelectionStart = m_CursorPos` (position before movement)

### Double-click word selection
- Windows-compatible character classification: whitespace (0), word (1: `a-z A-Z 0-9 _`), other (2: everything else)
- `SelectWordAt(int pos)`: expands left/right while same character class
- `FindPrevWordBoundary(from)` / `FindNextWordBoundary(from)` — word jump boundaries (Ctrl+Left/Right)
- Double-click detection: two `OnPointerDown` within ≤15 frames (~250ms) AND `|pos1-pos2| ≤ 3` characters
- Monotonic `m_FrameCounter` (never wraps, never resets by blink) for reliable timing
- `XConsole::Trace` logged on each double-click with framesSinceLast and posDiff

### Ctrl+A select all
- `Keyboard::IsDown(LeftControl|RightControl) && Key::A` → `m_SelectionStart = 0`, `m_CursorPos = len`

### Space width fix
- `GetCursorXAt` and `GetCharIndexAtX` use `m_Font->GetSpaceWidth()` for space characters instead of `g.advance` (which differs from `m_SpaceWidth` used by `Font::LayoutText`), fixing caret misalignment with spaces

### UITextArea (multiline)
- New class in `engine/include/LeirEngine/UI/UITextArea.h` / `engine/src/UI/UITextArea.cpp`
- Inherits `UITextInput`, allows `\n` in `InsertChar`
- `OnKeyDown`: Enter → insert `\n`; Up/Down → navigate between logical lines preserving `m_TargetX`
- `OnPointerDown`: multiline-aware (computes line from Y, column from X)
- `OnPointerMove`: Y-aware override (line from Y, column from X within line) for correct drag selection across lines
- `GetCursorLine()` / `GetCursorCol()` / `GetLineStart/End()` — logical line helpers
- `SetCustomMinSize(Vector2)` — overrides the default min size for specific instances
- `UIRenderer`: renders text with `baselineY = cr.y + 4 + ascender`, caret at `cr.y + 4 + cursorLine * lineH`
- Selection rendering: multi-line selection draws one rect per selected line (via `GetLineStart`/`GetLineEnd` intersection)
- `GetMinSize()` default: 200×100

### DebugTextPanel (editor)
- New editor panel in `editor/src/UI/DebugTextPanel.h/.cpp`
- Contains: `UITextInput` (single-line), `UITextArea` (multiline), `UIFloatInput`
- `Refresh()` per frame shows live cursor pos, line/col, selection state, float value
- Name starts with "Debug" → renders in debug overlay layer
- Integrated in `main.cpp` (OnInit creation, OnUpdate Refresh)

## Previous Changes Summary

- **Console flash on rebuild + "Viewport Resized" log flood fixed** (see `TODO_UI_CONSOLE.md`): user bug — dragging the horizontal splitter above the console caused a flash (text vanished one frame, right as a "Settings Saved" message arrived on release); dragging any vertical splitter made the console text invisible until release, flickering. Root cause: `ConsolePanel::Refresh()` (→ `RebuildLines`) ran **after** `m_Canvas->UpdateLayout()` in `EditorApp::OnUpdate`, so the freshly created `newColumn` + labels had `m_ComputedRect = {0,0,0,0}` (never laid out that frame) → the `ScrollView`'s clip (SetClip) culled them (intersection zero → `return` in `RenderElement`) → **exactly 1 empty frame per rebuild**. The splitter cases amplified it: "Settings Saved" is logged at **Info** on every splitter-drag release (`Settings.cpp Save()`), and "Viewport Resized" was logged at **Info on every frame of a vertical splitter drag** (`UpdateViewportRenderTarget`) → `GetVersion()` bumped per frame → rebuild every frame → permanent blank until release. Fixes: (1) `main.cpp OnUpdate` — moved `m_ConsolePanel->Refresh()` **before** `m_Canvas->UpdateLayout()` so rebuilt labels get laid out in the same frame (offset still re-clamped by `ScrollView::OnLayoutComputed`). (2) `UpdateViewportRenderTarget()` — the "Viewport Resized" log is now **debounced**: it records `m_PendingW/H` while the size keeps changing and only prints once the size settles for a frame (`m_LastLoggedW/H` guard), so a whole drag emits 0 messages and exactly 1 on release. Verified by user.
- **ScrollView scroll direction + scrollbar track fixed** (see `TODO_UI_SCROLLBARS.md`): user bug in the console's left column — (1) the scrollbar track height didn't match the container and kept shrinking on resize; (2) drag-to-scroll was inverted (empty space appeared at top + top lock, instead of the first line clipping away and the lock at the bottom); (3) the thumb moved the text the wrong way (thumb down → text down). All three shared root causes in `engine/src/UI/ScrollView.cpp`: (a) `SyncScrollbar()` positioned the scrollbar with a `{1,0,1,1}` anchor + *relative* offsets that clobbered the parent-propagated absolute position → the track was drawn misaligned and got clipped by the ScrollView's own clip (wrong height, shrank on resize). Fix: position the scrollbar with `AnchorSet::TopLeft` + absolute offsets like the content. (b) Content positioned at `cr + scrollOffset` but the header documents "positive = content moved up" → sign inverted (drag/wheel/thumb all moved the text the wrong way). Fix: `cr - scrollOffset` in both `OnLayoutComputed` blocks, and drag `off.y = m_ScrollStart.y - delta.y` (touch-style, content follows the finger). The thumb inversion was fixed automatically by the content sign (thumb down → value↑ → offset↑ → content up). Wheel already correct once content sign fixed. Verified by user.
- **Scrollbar scissor truncation + pixel snapping** (see `TODO_UI_SCROLLBARS.md`, 2026-08-07): after scoping the viewport clip (Model A, `m_Viewport` node), the user still saw the horizontal scrollbar track as 9px (thumb 6px, 2px track above / 1px below) while the vertical one was 10px; with `ui_outlines: true` the **track's bottom outline row wasn't drawn** → root cause was the **scissor truncation**, not the geometry (track/thumb were already exact `10.0`/`6.0` floats). `ScrollView` has `SetClip(true)` so a scrollbar's clip ≈ its own **fractional** rect (e.g. `bottom=1615.806`); `UIRenderer` computed `scissor.offset=(int32_t)(y*scale)` and `extent.height=(uint32_t)(w*scale)` — truncation toward zero dropped the last pixel row of anything whose edge fell on a fractional pixel. Fix: single helper `ScissorFromLogicalClip` in `UIRenderer.cpp` (floor for offset, ceil for the opposite edge, clamped to the framebuffer) used in both `pushQuad` and `ApplyScissor` — conservative clipping never drops a touched pixel and the quad geometry stamps any excess. Kept (complementary, not the root cause): `std::round` pixel-snapping of the thumb edges (`UIScrollbar::OnLayoutComputed`) and track/viewport rects (`ScrollView::SyncScrollbar`/`OnLayoutComputed`). Removed the temporary `ScrollbarDebugLog` (`%TEMP%\leir_scrollbar_geom.txt`) from `UIScrollbar.cpp`. Verified by user: track 10px / thumb 6px centered in both orientations, bottom outline drawn.
- **UIEvent flood → console rebuild loop + UIRenderer overflow glitches fixed** (see `TODO_UI_EVENT_FLOOD.md`): user bug — moving the mouse made the console auto-scroll, red/green glitches across the screen, FPS dropped 60→20 permanently, slow close. Root cause (confirmed by log with `ui_event_log: true`): every mouse move emitted `[UIEvent] hover -> 'X'` at **Info** level → entered the ring buffer → bumped `GetVersion()` → `ConsolePanel::Refresh()` rebuilt all lines **every frame** (destroy/recreate of up to 300 labels → FPS churn) + auto-follow scrolled down. The rebuild kept the line count growing → `UIRenderer` `Flush()` overflow (m_MaxVertices was 8192) did clear-all + `return`, but the overlay pass is `LOAD_OP_LOAD` + UNDEFINED layout (`BeginSwapchainOverlay`) → swapchain showed uninitialized memory = the glitches. Each overflow warning (level **Warning** = retained) bumped `GetVersion()` again → feedback loop (permanent low FPS + slow close). Fixes: (1) `UICanvas.cpp` — all 10 `[UIEvent]` banners changed from `Println` (Info) to `XConsole::Trace` (Debug); the `[Canvas]` banners were already Trace. UI/debug logs must be Trace/Debug by rule (only Info/Warn/Error are retained for the console). (2) `UIRenderer.cpp Flush()` — overflow now **truncates non-destructively** (drops regular quads from the end, then debug-overlay quads last, always keeping the viewport) instead of clear+return; warning is `XConsole::Debug` (not retained → no loop). (3) `m_MaxVertices` raised 8192 → 65536. Verified with mouse-sweep script (`SetCursorPos`, ~2.2s sweep over the window with `ui_event_log: true`): `[Console] rebuilt` went from ~1/pointer-event to **1 total**; `[info] [UIEvent]` 0 (63 trace); `overflow` 0; stderr/VUID empty. Editing rule learned: **renderer-internal / UI logs must be Trace/Debug, never Info/Warn/Error** (Info+ is the console channel and any Info/Warning emitted inside per-frame paths creates feedback loops).
- **UI Clipping + Scrollbars + Console panel** (see `TODO_UI_SCROLLBARS.md` + `TODO_UI_CONSOLE.md`): `UIElement::SetClip(bool)`; `UIRenderer` recursive `RenderElement` walk with clip-stack intersection + CPU cull, per-draw `vkCmdSetScissor` (parallel `m_QuadClips`/`m_DebugQuadClips`, `ViewportDraw.clip`), `SetContentScale(float)` (logical clip → physical scissor). `UIElement::OnScroll(float)` virtual; `UICanvas::ProcessScrollEvent` dispatches the `ScrollEvent` to the hovered element (propagates up parents). New `UIScrollbar` (engine, track+thumb composition, normalized value, drag with pointer capture). `ScrollView` rework: `SetClip(true)`, absolute content positioning (fixed relative-offset bug), clamps to `[0,maxScroll]`, wheel + drag scrolling, built-in vertical `UIScrollbar` synced in `OnLayoutComputed`. `LogMessage` gains `time` (`HH:MM:SS.mmm`); `XConsole::GetVersion()` monotonic counter; ring buffer now **retains only Info/Warning/Error** (Trace/Debug are debug-only and were evicting useful messages). Editor: new `ConsolePanel` (docked, filter buttons Info/Warn/Error + Clear, colored lines with timestamp, wheel/scrollbar/auto-follow, lazy rebuild via `GetVersion()`), registered as `"ConsolePanel"`/`"Console"` (closeable), added to `kDebugIds` in `BuildDefaultLayout`, `DeleteUiSubtree` in `OnShutdown`; `UIRenderer::SetContentScale` in `OnInit`/`OnContentScaleChanged`.
- `XConsole` logging system (`Core/Log.h` + `Core/Log.cpp`): own logger replacing spdlog. `LogLevel` enum (Trace/Debug/Info/Warning/Error, verbosity filter), API `Println`/`PrintWarning`/`PrintError`/`Trace`/`Debug`/`SetLevel`/`GetLevel`/`GetMessages`/`Clear`, runtime `{}` formatter (`std::any` + `ostringstream`; specs `{:.Nf}`/`{:Nd}`/`{:0Nd}`), 1000-entry thread-safe ring buffer, stdout (info/warn/debug/trace) + stderr (error). All state in function-local statics (no static-init-order issues).
- spdlog removed: deleted FetchContent from `dependencies/CMakeLists.txt`, `spdlog::spdlog` from `engine/CMakeLists.txt` (added `Log.cpp`), migrated 94 call-sites in 20 files (`info`→`Println`, `warn`→`PrintWarning`, `error`/`critical`→`PrintError`, `trace`→`Trace`; `set_level`→`SetLevel`), editor/example qualified as `Leir::XConsole::`. Re-added `<exception>`/`<stdexcept>` where spdlog provided them transitively (Settings, Shader, Texture2D, DockManager). New doc `TODO_LOG_SYSTEM.md` (concepts, API, migration, future `ConsolePanel`).
- `Settings.h`: added `debug.show_overlay` field
- `UICanvas::GetHoveredElement()` added for editor camera viewport detection
- `Keyboard.h`: added `#include <string>` for `GetPressedKeysString()`
- `EventQueue::Process()` now calls `Keyboard/Mouse/Pointer/Touch::ProcessEvent()` to update polling state each frame
- `UIDebugOverlay`: event display duration increased from 1 frame to 120 frames (~2s)
- `CoreApplication::Run()`: `InputManager::Update()` (ResetFrame) moved **after** `OnUpdate()` so delta/scroll/edge detection work during update
- `UIRenderer`: `ViewportDraw` struct, `m_ViewportDraws`/`m_VpDescCache`, `BuildBatchDebug()` for 3-layer flush
- `engine/CMakeLists.txt`: added `RenderTexture.cpp`, `UIViewportPanel.cpp`
- `UIRenderer` draw layers: changed from 2-layer (regular UI → viewports) to 3-layer (regular UI → viewports → debug overlay). Debug detection walks parent chain for `"Debug"` prefix name
- Layout system: parent `m_ComputedRect` position now added to child offset **before** `child->ComputeLayout()` in all 3 layout modes (Free/Row/Column), fixing nested layouts where grandchildren (e.g. labels inside `UIDragFloatInput` inside `UITestPanel`) had wrong positions
- `UICanvas::ProcessPointerEvent`: event propagates up parent chain when deepest child returns `false`; pointer capture via `CapturePointer(UIElement*)` / `ReleasePointer()`
- `UILabel`: vertical centering — text now centered in label's computed rect using `(cr.w - blockH) * 0.5f + ascender` instead of just `ascender`
- `UIFloatInput`: new engine widget inheriting `UITextInput`, filters `[0-9]` `+` `-` `.`, commits on Enter/Blur
- `UIDragFloatInput`: new editor widget (UIPanel + UILabel + UIFloatInput), drag-to-change on label with pointer capture
- `UITestPanel` / `CameraTestPanel`: floating editor panels bound to Cube and Camera transforms, with OnValueChanged callbacks for immediate transform writes and Refresh() per frame
- `EditorCamera`: new class in `editor/src/Camera/EditorCamera.h/.cpp`, free-fly camera with right-click yaw/pitch, middle-click pan, WASDQE movement. Replaces old inline struct
- Bidirectional camera sync: EditorCamera → scene camera during right-click/middle-click control; scene camera → EditorCamera during panel edits
- Camera initial position: `{0.0f, 2.0f, 4.0f}`
- `GetCursorXAt` and `GetCharIndexAtX` in `UITextInput.cpp`: `\n` resets x=0 (instead of adding fallback glyph advance), fixing caret staircase in multiline text
- `UITextArea::OnPointerDown` in `UITextArea.cpp`: same `\n` + space-width fix in column-finding loop
- `UITextArea::OnPointerMove` override: Y-aware drag selection (line from Y, column from X within line)
- `UIRenderer.cpp` selection batch: multi-line selection renders one rect per selected line (via `GetLineStart`/`GetLineEnd` intersection) instead of a single rect
- `UITextArea.h/.cpp`: added `SetCustomMinSize(Vector2)` + `m_HasCustomMinSize` flag for instance-level min-size override
- `UIElement.cpp` Column/Row layout: Fill children now clamp to `std::max(fillTotal, GetMinSize())` — prevents squeezing below minimum
- `DebugTextPanel.cpp`: `\n`/`\r`/`\t` escaped in MultiInput status label text, preventing UILabel multi-line expansion with each Enter
- `editor/src/UI/TextAreaDebugPanel.h/.cpp`: new isolated debug panel for UITextArea (top-right, above CameraTestPanel)
- `editor/src/main.cpp`: TextAreaDebugPanel integrated (create + Refresh per frame)
- `editor/CMakeLists.txt`: added `TextAreaDebugPanel.cpp`
- `LeirSettings` (Settings.h/.cpp): settings moved to platform config dir (`%APPDATA%\LeirEngine\settings.json` on Windows, `~/Library/Application Support/LeirEngine/` macOS, `$XDG_CONFIG_HOME/LeirEngine/` Linux); added `layout` section (`hierarchy_width`/`inspector_width`); `Save()` creates the config dir; legacy `leir_settings.json` migration + file removed (no backward compat)
- `InputManager.h/.cpp`: new `CursorStyle` enum + static `SetCursorStyle()` using cached `glfwCreateStandardCursor` (no-op on no change)
- `UISplitter`: new editor widget (`editor/src/UI/UISplitter.h/.cpp`) — 6px draggable divider between panels, pointer capture, `clamp(startWidth+dx, min, max)`, `SetDragInverted(true)` for right-docked panels, `ResizeEW` cursor on hover/drag, saves on drag end
- `EditorApp` (editor/src/main.cpp): replaced `kHierarchyWidth`/`kInspectorWidth` constants with mutable `m_HierarchyWidth`/`m_InspectorWidth` state; new `ApplyPanelLayout()` applies widths to panel offsets (called in OnInit + each frame before layout); splitters between Hierarchy|Viewport and Viewport|Inspector; save on splitter drag end + on `OnShutdown`
- Window placement persistence: `CoreApplication` (CoreApplication.h/.cpp) extended ctor with `posX/posY/maximized` (restored via `glfwSetWindowPos`/`glfwMaximizeWindow`, centered if `INT_MIN`); new `glfwSetWindowSizeCallback`/`glfwSetWindowPosCallback` track the normal (non-maximized, non-fullscreen) rect; getters `GetWindowPosition`, `GetWindowSize`, `IsMaximized`, `GetNormalWindowRect`. `LeirSettings.window` added `pos_x`, `pos_y` (default `INT_MIN`), `maximized`. Editor saves windowed rect + maximized flag on `OnShutdown` (skips when fullscreen)
- HiDPI base (see `TODO_HIDPI.md`): `CoreApplication` ctor takes `hidpi` and creates the window at `logical × scale` physical px; `GetWidth/GetHeight` are now **logical** (`framebuffer ÷ GetContentScale()`), `GetFramebufferWidth/Height` physical, `GetContentScale()` (1.0 when HiDPI disabled); `OnContentScaleChanged()` virtual via `glfwSetWindowContentScaleCallback`. `InputManager::ToLogical()` divides cursor pos by the effective scale on Windows (physical there for DPI-aware processes, no-op on mac/linux), fed by `SetContentScale()`. `UIRenderer` push-constant `screenSize` = logical canvas size. Editor viewport `RenderTexture` sized at `logical × dpr`. `LeirSettings.window` added `hidpi` (default `true`). Diagnostic logs: DPI awareness context + surface currentExtent
- Maximized-start fix (`CoreApplication` ctor): removed the `if (maximized) { m_Width = width; m_Height = height; }` overwrite — it clobbered the logical canvas size with the saved windowed size while `UpdateNormalRect` ignores the maximized state anyway. Starting maximized left the UI stretched (~1.49×) and mouse misaligned until a resize event fired; now `m_Width/m_Height` keep the real maximized logical size.
- `RenderTexture` descriptor ownership (see `TODO_DESCRIPTORS_VIEWPORT.md`): the viewport descriptor set moved from `UIRenderer` (lazy cache `m_VpDescCache` + manual `InvalidateViewportDescriptor`) into `RenderTexture` — created in ctor, re-written in `Resize()`, destroyed in dtor, with its own `VkDescriptorSetLayout` + pool. UI binds `rt->GetDescriptorSet()` directly; stale-descriptor-by-forgetfulness is now impossible. Removed `UIRenderer::GetOrCreateVpDescSet`, `InvalidateViewportDescriptor`, `m_VpDescCache`, and `FREE_DESCRIPTOR_SET_BIT` from the UI texture pool (now textures-only). Editor no longer invalidates on viewport resize.
- `UIElement::InsertChildAt(child, index)`: new public API — inserts a child at a given position in `m_Children` (clamped, auto-reparents like `AddChild`). Used to reorder tabs in a dock tab bar.
- `DockPane::InsertTab(panel, index)` + `DockTabBar::InsertTab(panel, index)`: insert a tab at a specific index in both the tab list and the tab bar row.
- `DockPane::ReorderTabTo(panel, pos)`: same-pane **tab reorder** — computes the insertion index from `pos.x` against sibling tab centers (adjusting `-1` when moving right so removal shifts the remaining tabs), then `RemoveTab` + `InsertTab` + `SetActivePanel`. Returns whether the order changed.
- `DockPane::GetTabBar()`: accessor added so `DockManager` can test drop position against the tab-bar strip.
- `DockManager::OnPointerUp` (DockManager.cpp): uses the real drop position (was discarded via `(void)pos`). Same-pane drops now: over the tab bar with ≥2 tabs → `ReorderTabTo`; Center → focus; edges → split. Other panes keep merge/split. `OnPointerMove` hides the zone highlight during a reorder gesture (own pane, ≥2 tabs, over tab bar) so only the ghost is shown.
- `DockManager::SplitPane` self-drop guard relaxed: was `if (target->Contains(panel))` (any drop on the pane hosting the tab did nothing); now only short-circuits a pure self-drop when `target->GetTabCount() <= 1`. Edge drops on a shared pane (≥2 tabs) now split the pane and move the dragged tab into the new zone.
- Vulkan teardown fix (`VUID-vkDestroyDevice-device-05137`): `Material::RecreatePipeline` now destroys the old `m_UBOSetLayout` before recreating (was leaking a `VkDescriptorSetLayout` per call). Editor `OnShutdown` frees all dock content subtrees via a new `DeleteUiSubtree()` helper — `hierarchy`/`inspector`/`viewport` content and the debug panels were orphaned (UIElement dtor only nulls child parents) and never deleted. Verified: 2 clean runs with validation layers (dock ops + window/swapchain resizes, close → no VUID).
- **UITextArea full scroll (Fase 2, see `TODO_UI_TEXTAREA.md`)**: `UITextArea` now has its own `m_ScrollOffset` + `SetClip(true)` in its ctor. `GetContentSize()` (max line width × line count) vs `GetViewportSize()` (widget rect minus enabled scrollbar strips, default 10px `m_ScrollbarWidth`) drive `GetMaxScrollY/X()`. It owns vertical + horizontal `UIScrollbar` children created in the ctor, positioned in `OnLayoutComputed`/`SyncScrollbars` with full-rect TopLeft + absolute offsets (same convention as `ScrollView::SyncScrollbar`), `SetActive(overflow)`, `SetRange`/`SetValue` synced both ways. `OnScroll` = wheel vertical, Shift+wheel horizontal (`delta × lineHeight`). `EnsureCaretVisible()` auto-follows after every cursor mutation (keys, click/drag, typing, Enter, selection) with a one-line margin. `UIRenderer` now subtracts the offset from text baseline, caret, and each multi-line selection rect; textarea text is **no longer word-wrapped** (`LayoutText` maxWidth=0) so long lines overflow into the horizontal scroll space. Pointer hit-testing adds `m_ScrollOffset` to local coords so clicks land on the right line/col when scrolled. `TextAreaDebugPanel` gained a second read-only area (40 long lines).
- **`UITextInput::SetEditable` (read-only text fields)**: `bool m_Editable` (default true) + `SetEditable()`/`IsEditable()`. When `false` the control is read-only but still scrollable: `OnPointerDown` returns false (no caret placement or focus), `OnKeyDown`/`OnTextInput`/`InsertChar`/`OnPointerMove` guard editable, `OnFocus` only marks focused when editable, `IsCaretVisible()` requires editable, and `SetEditable(false)` clears focus/capture/selection via the canvas. `SetText`/`SetPlaceholder`/colors unaffected.
- **UITextArea perf fix — O(N²) → O(N)** (see `TODO_UI_TEXTAREA.md` Fase 3, verified by user): with the 40-line read-only area visible FPS fell 60 → 10. Root cause: `UITextArea::GetContentSize()` was **O(N²)** — per line it called `GetLineEnd(line)` (rescan from start) + `GetCursorXAt(GetLineEnd(line))` (rescan from start again), ~40 rescans per call, and it runs ~3-4× per frame (`SetScrollOffset` + `SyncScrollbars` in `OnLayoutComputed`) ≈ 90M char-ops/frame. Fix: `GetContentSize()` rewritten as a **single-pass O(N)** sweep over `m_Text` — one walk decoding each codepoint (same UTF-8/space/advance logic as `GetCursorXAt`), accumulating current line width + max line width, counting lines via the `\n`. No per-line `GetLineEnd`/`GetCursorXAt`. FPS stable at 60.
- **UITextArea word wrap (Fase 4, see `TODO_UI_TEXTAREA.md`)**: optional per-instance `SetWordWrap(bool)`/`IsWordWrapEnabled()`. When on, a lazily-rebuilt visual-row model (`m_VisualRows` of `{startByte, endByte, width}`) makes visual rows ≠ logical lines: `\n` closes a row (empty rows included), and long runs wrap at `wrapLimit = max(0, cr.z - vstrip - 8)` — with a previous space in the row the break lands at the space (word stays intact), without one it hard-breaks. The model is invalidated via `m_ModelGen` (bumped by `SetText`/`SetFont`/`OnTextMutated`/`SetWordWrap`) and rebuilt lazily in `EnsureVisualRows()` when the gen **or** the computed wrap width changes (so resizing the widget re-wraps). `GetLineCount/GetLineStart/GetLineEnd/GetCursorLine/GetCursorCol/GetCursorXAt` now operate on visual rows (wrap off → identical to previous logical-line behavior). `Home`/`End` move to the active *visual* row start/end; Up/Down keep `m_TargetX` and scan the target row with correct space-width. `GetContentSize()` with wrap ON returns `{viewport.x, rows*lineH + 8}` so the horizontal scrollbar hides automatically (no hscroll when wrapping); wrap OFF keeps the single-pass O(N) max-width of all rows + hscroll. UIRenderer draws each wrapped row via `LayoutText(rowSubstr, 0)` shifted by `line * lineH − scrollY`; caret/selection render against the visual row. Editor: new `TextAreaWrapPanel` ("Text Area Wrap", tab in `kDebugIds`) with a toggle button + live status (wrap, logical vs visual lines, cursor).

