# LeirEngine

A cross-platform C++ game engine built from scratch with Vulkan.

## Tech Stack

| Area | Choice |
|---|---|
| Language | C++20 |
| Build | CMake 3.20+ (superbuild with FetchContent) |
| Windowing/Input | GLFW 3.4 |
| Graphics | Vulkan 1.3 (backend RHI) + D3D12 (backend RHI) + WebGPU (wgpu-native, Fase 5 — 2026-08-15); MoltenVK (macOS/iOS, futuro) |
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

### Web export (WebGPU demos)

One command builds + serves + opens the browser. `scripts/export_web.py` is the
cross-platform core; the wrappers pick the demo:

- `scripts/export_web_engine.bat` (double-click) → **WebEngineDemo** (full engine),
  serves on **8001**
- `scripts/export_web_demo.bat` (double-click) → **WebDemo** (M1 raw RHI), 8000
- `scripts/export_web_engine.sh` / `export_web_demo.sh` → same on Linux/macOS

Flags: `--demo {demo,engine}`, `--no-serve` (build only), `--port N`,
`--no-open`, `--emsdk <path>`. Fixed emsdk default: `C:\programs_dev\emsdk6`
(Windows) / `~/emsdk` (POSIX), overridable via `--emsdk`. The EMSDK *env var*
is intentionally ignored (a stale one pointed at an old emsdk with Python 3.9
broke emscripten's ≥3.10 requirement); use `--emsdk` instead.

Windows notes: CreateProcess resolves bare exe names against the *parent*
PATH, so the script passes an absolute `cmake.exe` path; `EMSDK_PYTHON` is set
to the emsdk-bundled Python for em++.

**Web physics (M3) needs a bigger wasm stack**: Jolt's `ProcessBodyPair` at
`-O0` (Debug build) has ~1500+ wasm locals; with ASYNCIFY + the deep call chain
(`MainLoop → … → JobFindCollisions → ProcessBodyPair`) it overflowed
emscripten's default 5 MB stack → `index out of bounds` at the function entry
on the FIRST real contact (looked like a physics bug, was a stack overflow).
Fix: `-sSTACK_SIZE=16777216` (16 MB) in the demo's `target_link_options`
(`examples/WebEngineDemo/CMakeLists.txt`). No physics settings were changed.
A future `-O2` release build shrinks the frames; the flag is harmless either way.

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

## Crash Diagnostics

Editor crash/failure reporting isolated in `editor/src/CrashDiagnostics.h/.cpp`
(see `CRASH_DIAGNOSTICS.md`). Single portable entry point
`CrashDiagnostics::Init()` called once from `editor/src/main.cpp`. Per-platform
branches:

- **Windows**: `std::set_terminate` (dumps exception type+what) + CRT
  invalid-parameter handler + debug CRT alloc hook (>512 MB → DbgHelp symbolized
  stack walk to `crash_diagnostics.log`).
- **macOS/Linux**: terminate handler only; skeletons ready with TODO comments.

**CI note**: `dbghelp` is linked via CMake in `editor/CMakeLists.txt`
(`if(WIN32) ... PRIVATE dbghelp`) because the `#pragma comment(lib,...)` in the
.cpp is MSVC-only — MinGW (the old GitHub Actions `windows-ci-debug` compiler)
ignores it and would otherwise fail to link `SymInitialize`/`SymFromAddr`.
Keep the CMake link as source of truth. Platform code stays OUT of `main.cpp`.
**Windows CI now builds with MSVC (`cl`) since 2026-08-14** — the runner image's
MinGW was never able to RUN the test exes (they hung silently before `main()`);
`ilammy/msvc-dev-cmd@v1` + `-DCMAKE_CXX_COMPILER=cl` makes CI Windows match the
local `windows-debug` preset exactly.

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
- **Fonts**: the font atlas is rasterized at `fontSize × dpr` with all metrics in logical units
  (atlas px ÷ dpr) so each texel maps 1:1 to a physical pixel — crisp text at any DPI in both
  backends (see `Font::Font(..., contentScale)` / `Font::SetContentScale()`; fixed BUG02,
  2026-08-13).
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

### Web (wasm/Emscripten)
- `JobSystemSingleThreaded` instead of `JobSystemThreadPool` under `__EMSCRIPTEN__`
  (no pthreads on the static web build; jobs run synchronously, ASYNCIFY-compatible).
- `StepPhysics` uses a **fixed 1/60 step** with an accumulator (dt clamped to 0.25 s)
  — a variable dt caused tunneling through the 1 m floor at 25-40 fps.
- Jolt's `ProcessBodyPair` at `-O0` needs ~16 MB of wasm stack (see "Web export").

### Web threading: single-thread is a deliberate, permanent decision (2026-08-17)
- Browser threads = Emscripten `-pthread` → Web Workers + **`SharedArrayBuffer`**, which
  requires **cross-origin isolation**: COOP (`Cross-Origin-Opener-Policy: same-origin`) +
  COEP (`Cross-Origin-Embedder-Policy: require-corp`) headers from the **server**, HTTPS or
  localhost, and every cross-origin embedded asset (images/audio/fonts) CORS/CORP-compliant.
  A static server (`python -m http.server`, GitHub Pages without headers, etc.) then breaks,
  and the "works from any hosting" property is lost. This is a browser security constraint,
  NOT an Emscripten limitation (Emscripten threads work fine) and NOT a leftover bug.
- **Professional engines do the same**: single-threaded web builds by default, multithreading
  as an opt-in/experimental feature that requires the COOP/COEP headers — Unity WebGL
  (single default; experimental threads since 2022.x/Unity 6), Godot 4 (single default;
  "Thread Support" export option), Rapier (single-threaded wasm), Ammo.js (single/worker).
  Even threaded engines keep the core game loop + render on one thread; workers serve
  specialized systems (physics, jobs).
- **Decision: LeirEngine web stays single-threaded.** Jolt's `JobSystemSingleThreaded` is
  correct and scales for demo-scale scenes; the desktop engine keeps `JobSystemThreadPool`
  (multithreaded) in every backend (Vulkan/D3D12/WebGPU native). Revisit only if a web game
  ever needs hundreds of bodies → then build `-pthread` + serve with COOP/COEP.

## Audio System

See `TODO_AUDIO_SYSTEM.md` for the full concept, design and gotchas.

- **SoLoud** (zlib) is the backend library. The abstraction follows the RHI/Physics
  pattern: **public headers NEVER expose SoLoud types** — all handles are opaque
  `uint32_t` (`SoundId`/`ClipId`), positions use `Leir::Vector3`.
- **Files**: public headers in `engine/include/LeirEngine/Audio/` (`AudioTypes.h`,
  `AudioBackend.h`, `AudioEngine.h`, `AudioClip.h`, `SoundPlayer.h`) + sources in
  `engine/src/Audio/` (`SoLoudBackend.cpp`, `AudioEngine.cpp`, `AudioClip.cpp`,
  `SoundPlayer.cpp`) + components `AudioSource`/`AudioListener` in
  `engine/include/LeirEngine/Components/` + `engine/src/Components/`.
- **API**: `AudioEngine` singleton (init/shutdown/update/WakeUp, clip cache by path),
  `SoundPlayer` static quick-play API (`Play` with 8 overloads incl. 3D `Vector3`
  position, `PlayMusic`, `Stop/Pause/Resume/Seek/SetLoop/SetVolume/SetPitch/
  SetPosition/FadeOut/FadeTo`, `GetState/GetTime/GetDuration`, global
  `SetMasterVolume/StopAll/PauseAll/ResumeAll/FadeAll`), `AudioSource` component
  (Unity-style: clip, loop, volume/pitch, `SetSpatial3D`, `SetMinDistance/
  SetMaxDistance` falloff, play on awake), `AudioListener` (feeds
  `SetListener3D` from the transform each frame). Music = one active channel;
  `StopMusic()`/`PauseMusic()` without an id act on the current one.
- **SoLoudBackend**: 1 `SoundId` = 1 voice. `Play` = stop + `play()`/`play3d()` +
  `setLooping`; `Pause/Resume` = `setPause`; `Seek` = `seek`; `GetTime` = `getStreamTime`;
  3D via `play3d` + `set3dSourcePosition` + `update3dAudio`; listener
  `set3dListenerParameters(pos, at=pos+fwd, up)`; master `setGlobalVolume`; fades
  `setFadeVolume`. `Update()` calls `soloud.update()` + `update3dAudio()`.
- **Backends**: Windows desktop = `soloud_wasapi` (links `ole32`+`avrt`); Linux/macOS/CI
  = `soloud_null` (no audio device); web = `soloud_miniaudio` → WebAudio
  (`ma_backend_webaudio`), the TU compiled with `-std=gnu++20` (miniaudio forbids
  `-std=c*`/`-ansi`, an Emscripten constraint). Built as a **custom `soloud` STATIC
  target** from `${soloud_SOURCE_DIR}` in `cmake/AudioTooling.cmake` (SoLoud has no root
  CMakeLists; `FetchContent_MakeAvailable` alone only populates it and the old
  `if(TARGET soloud)` was silently false). Pinned to `e82fd32c`.
- **Web autoplay policy**: the `AudioContext` is born suspended; browsers require a user
  gesture to `resume()`. `SoLoudBackend::Init()` does **NOT** call `soloud.init()` on web;
  `AudioEngine::WakeUp()` starts the device on the first gesture (called by the demo from
  the first `OnPointerDown`). Desktop: `Init()` starts immediately, `WakeUp()` is a no-op.
- **Guards**: methods needing a running device (`Play`, `SetListener3D`, `GetState`,
  `GetTime`, `SetMasterVolume`, `GetMasterVolume`, `DestroySource`) no-op while
  `m_DeviceStarted` is false; `LoadClip`/`CreateSource` keep working on `m_Initialized`
  only (so the click handler can load and prepare before the first gesture).
- **Web build gotcha (EM_ASM exports)**: miniaudio's WebAudio backend runs EM_ASM JS that
  accesses `Module.*`. Emscripten 4.x only copies `EXPORTED_FUNCTIONS` symbols to
  `Module[name]` and `EXPORTED_RUNTIME_METHODS` defaults to `[]`, so three exports were
  required in `examples/WebEngineDemo/CMakeLists.txt`:
  1. `-sEXPORTED_FUNCTIONS=_malloc,_free,_main` → fixes `Module._malloc is not a function`
     (miniaudio reserves the interop buffer with `Module._malloc`/`_free`);
  2. `-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF64`
     → fixes `Aborted('HEAPF32' was not exported ...)` (EM_ASM builds the interop view as
     `new Float32Array(Module.HEAPF32.buffer, ...)`);
  3. add `ccall` to `EXPORTED_RUNTIME_METHODS` → fixes `ccall is not a function` thrown in
     `onaudioprocess` (the ScriptProcessorNode calls the C
     `ma_device_process_pcm_frames_playback__webaudio` via `ccall(...)`).
  Rules: if you set `EXPORTED_FUNCTIONS` you must keep `_main` (else reactor mode, `main`
  never runs); auto-generated exports (KEEPALIVE `ma_device_process_pcm_frames_*`,
  `emwgpuOn*`, dynCall, stack) are preserved separately and not broken by the explicit list.
- **Assets** (CC0, generated with ffmpeg): `assets/audio/beep.wav` (880 Hz 0.15 s),
  `pop.wav` (descending chirp), `music_loop.ogg` (C4/E4/G4/C5 pad loop). Preloaded on web
  by the existing `--preload-file assets@/assets`. Logging follows the Trace/Debug rule.

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
- **Hit-testing mirrors the clip** (fixed 2026-08-08): `UICanvas::HitTestRecursive`
  now takes the active clip rect and applies the **same intersect/fast-reject logic
  as `UIRenderer::RenderElement`** (`IsClipEnabled` → intersect with parent clip,
  empty intersection or fully-outside → subtree skipped; pointer outside `effClip` →
  skip). Before this, hit-testing only checked computed rects, so scrolled content
  kept stealing clicks/hover from elements its clipped rect had scrolled **over**
  (e.g. console lines sliding under the ConsoleHeader's Info/Warn/Error/Clear buttons
  captured the events, and wide content could cover a scrollbar strip).
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
- Registers once in the backend's **bindless texture table** (`RegisterBindlessTexture` in ctor); `Resize()` rewrites the descriptor **in place** (`UpdateBindlessTexture` — the SRV/sampler heaps never grow), and the dtor unregisters. UI renders the viewport by sampling `textures[GetBindlessIndex()]`. The old per-texture descriptor set (`GetDescriptorSet`/`m_DescPool`/`m_DescSetLayout`) is gone (see `TODO_RHI_SLANG.md` §5 Plan B step 3; legacy docs in `TODO_DESCRIPTORS_VIEWPORT.md`).

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

## UI Subtree Teardown & Ownership (double-free rule)

**Rule (learned from the 5s-close bug, 2026-08-08)**: composite UI widgets that build
their own internal children (`ScrollView` → viewport + 2 `UIScrollbar`;
`UITextArea` → 2 `UIScrollbar`; `UIScrollbar` → thumb) delete those children in
their **own destructors** (`RemoveChild` + `delete`). The editor frees dock
content subtrees with a recursive `DeleteUiSubtree(std::move next)`. A naive
walker that deletes *every* child first and then the widget causes a **double
free** (crash `0xC0000005` in `LeirEngine.dll` → WER sits ~5 s) — and deleting a
child out from under its parent leaves a stale `m_Children` entry that the
parent dtor dereferences.

- `UIElement::OwnsChild(const UIElement*)` virtual, default `false`. Composite
  widgets override it for their internal children:
  - `ScrollView`: `child == m_Viewport || m_VScrollbar || m_HScrollbar`
  - `UITextArea`: `child == m_VScrollbar || m_HScrollbar`
  - `UIScrollbar`: `child == m_Thumb`
- Both teardown helpers honor it: the editor's `DeleteUiSubtree` (main.cpp) and
  the `ConsolePanel` line-column walk. When `parent->OwnsChild(c)` is true the
  walker **skips** `c` (the widget dtor owns it) but still frees `c`'s
  *non-owned* descendants (`DeleteNonOwnedSubtree` / the inner recursion) so
  e.g. a `ScrollView`'s editor-owned content column still gets deleted. When
  false it does `RemoveChild(c)` **before** deleting so the parent dtor never
  sees a dangling pointer.
- The `ConsolePanel` internal copy of `DeleteUiSubtree` follows the same rule
  (used every rebuild).
- `DockTabBar`/`DockSplitNode` dtors already use the remove-then-delete pattern
  and were fine.

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

- **Grid del editor terminado + WebGPU single-source para el grid** (2026-08-21, ver `TODO_GRID_LOD_DISTANCE_FADE.md` + `TODO_WEBGPU_SINGLE_SOURCE.md`): (1) **Fog by depth** — el fade de distancia se movió al fragment shader (por pixel, con la profundidad interpolada `depth = lerp(clipS.w, clipE.w, cornerX)`), cada línea es 1 quad (se eliminó la subdivisión CPU `kSeg=16` que multiplicaba el count ~16× y stutteaba; el grid pasó de ~3091 a ~300-600 líneas). GridVertex gana `spacing` (loc 6, stride 64), GridPushConstants pasa a 8 floats (scale/fadeStart/fadeEnd/horizonStart/horizonEnd/overrideDensity), push mask `Vertex|Fragment`. (2) **Paridad de backends (Fix 10)**: Vulkan `dstAlphaBlendFactor` `ZERO`→`ONE_MINUS_SRC_ALPHA` (el UI compone el viewport con el alpha del RT; `ZERO` dejaba el RT semi-transparente en las líneas tenues → gris); ejes opacos (`spacing==0`) ordenados últimos (sort ascendente los tapaba). (3) **`GridPanel`** (tab "Grid"): knobs px/unit, fadeStart/End, thickWidth, horizonStart/End + toggle **Manual/Auto** (auto = knobs grisados, horizon fade **automático por altura de cámara** con interpolación lineal por tramos sobre breakpoints tuneables, y los valores auto se escriben en los inputs). El toggle arranca en Auto. (4) **WebGPU single-source (grid)**: `ShaderExporter::WriteRuntimeWebGpuShaders` genera `Grid.vert/frag.wgsl` desde el `.slang` al arrancar el editor (post-procesado: entry `vs_main`/`ps_main`, push `@group(1)@binding(0)`, locations de inputs 0..6 — Slang las asigna por semantic, no por `vk::location`; y **`FixWgslMatrixMultiply`** — Slang emite `vector*matrix` porque su WGSL guarda matrices transpuestas, con UBO GLM column-major eso daba `M^T·v` → el grid salía como rectángulos sólidos; se intercambia a `matrix*vector`). Se borraron los `.wgsl` hand-written del grid y su copia del CMake. Verificado en wgpu-native. Pendiente: extender a Basic/Sprite/UI/Gizmo con `#ifdef LEIR_BINDLESS` (native bindless / web single-texture — naga no compila `binding_array`).
- **Fase 6 — M4 COMPLETO, subsistema de audio SoLoud→WebAudio, sonido en el navegador** (2026-08-17, ver `TODO_AUDIO_SYSTEM.md`): el `WebEngineDemo` reproduce música loop + beep 2D + pop 3D al hacer click (verificado por el usuario en Firefox). **Arquitectura** (patrón RHI/Physics, la biblioteca queda aislada): headers públicos en `engine/include/LeirEngine/Audio/` (`AudioTypes.h` = `SoundId`/`ClipId`/`SoundState`, `AudioBackend.h` = interfaz `IAudioBackend`, `AudioEngine.h` singleton + caché de clips, `AudioClip.h` asset, `SoundPlayer.h` API estática) + `engine/src/Audio/` (`SoLoudBackend.cpp` real, `AudioEngine.cpp`, `AudioClip.cpp`, `SoundPlayer.cpp`) + componentes `AudioSource`/`AudioListener` (`engine/include/LeirEngine/Components/` + `src/Components/`). **Regla**: los headers públicos NUNCA exponen tipos de SoLoud; todo handle es `uint32_t`; las posiciones usan `Leir::Vector3`. **SoLoudBackend**: 1 `SoundId` = 1 voz (Stop/Pause/Resume/Seek/GetTime/loop), 3D vía `play3d`/`set3dSourcePosition` + `update3dAudio`, listener `set3dListenerParameters`, master `setGlobalVolume`, fades `setFadeVolume`. **Build**: target `soloud` STATIC propio creado desde `${soloud_SOURCE_DIR}` en `cmake/AudioTooling.cmake` (SoLoud NO tiene CMakeLists en la raíz — el `FetchContent_MakeAvailable` solo lo poblaba y el `if(TARGET soloud)` era falso = dead weight; diagnóstico 2026-08-17). Core (19 fuentes `src/core/`) + `wav`/`wavstream`/`dr_impl`/`stb_vorbis.c` + backend por plataforma: `wasapi` (Windows desktop, link `ole32`+`avrt`), `null` (Linux/macOS/CI — no hay device), `miniaudio` (`__EMSCRIPTEN__`, compilado con `-std=gnu++20` — miniaudio.h prohíbe `-std=c*`/`-ansi`, constraint de Emscripten). Pin a `e82fd32c`. Desktop: engine linka `PRIVATE soloud` + las 6 fuentes de audio. Web: `LeirEngineCore` + `examples/WebEngineDemo` linkan `soloud`, `--preload-file assets@/assets` cubre `assets/audio/`. **Autoplay policy resuelta con init lazy**: `AudioEngine::WakeUp()` inicia el device en el primer gesto; el demo llama `WakeUp()` + `PlayMusic(1, ...)` + `Play(...)` 2D + `Play(2, false, 0.8f, ..., posCubeA)` 3D en el primer `OnPointerDown`. `SoLoudBackend::Init()` en web NO llama `soloud.init()` (el `AudioContext` nace suspendido); `WakeUp()` lo hace. **Guards `m_DeviceStarted`** en `SoLoudBackend.cpp`: `Play`/`SetListener3D`/`GetState`/`GetTime`/`SetMasterVolume`/`GetMasterVolume`/`DestroySource` no-op sin device iniciado; `LoadClip`/`CreateSource` siguen funcionando (mantienen `m_Initialized`). **Los 3 fixes del build web** (el backend webaudio de miniaudio ejecuta EM_ASM JS que accede a `Module.*`; en Emscripten 4.x solo los símbolos de `EXPORTED_FUNCTIONS` reciben copia `Module[name]` y `EXPORTED_RUNTIME_METHODS` default es `[]`): (1) `Uncaught TypeError: Module._malloc is not a function` → `-sEXPORTED_FUNCTIONS=_malloc,_free,_main` (miniaudio reserva el buffer interop con `Module._malloc`/`_free`); (2) `Aborted('HEAPF32' was not exported...)` → `-sEXPORTED_RUNTIME_METHODS=HEAPF32,HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF64` (el mismo EM_ASM construye `new Float32Array(Module.HEAPF32.buffer, ...)`); (3) `Uncaught TypeError: ccall is not a function` en `onaudioprocess` → agregar `ccall` a `EXPORTED_RUNTIME_METHODS` (el ScriptProcessorNode invoca el C `ma_device_process_pcm_frames_playback__webaudio` vía `ccall(...)`). Reglas aprendidas: si fijás `EXPORTED_FUNCTIONS` incluí `_main` (si no → reactor mode, main no corre); los exports auto (KEEPALIVE `ma_device_process_pcm_frames_*`, `emwgpuOn*`, dynCall, stack) se conservan aparte y NO se rompen. **Assets CC0 generados con ffmpeg**: `assets/audio/{beep.wav 880Hz 0.15s, pop.wav chirp descendente, music_loop.ogg pad C4/E4/G4/C5 loop}` — verificados por el usuario localmente. Resultado: wasm ~68.5 MB (Debug), `.data` 493 KB (audio embebido), CI pendiente del commit sin `[skip ci]` (job Emscripten + 3 OS).
- **WebGPU nativo: bug de push constants (un solo cubo) + PhysicsDemo con backend seleccionable** (2026-08-17): `PhysicsDemo` con `settings.graphics.backend="webgpu"` dibujaba **1 solo cubo** en vez de los 9. Causa raíz: en el backend nativo (`WebGPUBackend`, wgpu-native) `CmdPushConstants` escribía en **UN solo UBO de push** (slot 0) — `queue.writeBuffer` es **last-write-wins**, así que todo el render pass leía el push del ÚLTIMO draw → los 9 cubos (misma malla/material, transform solo en push) se dibujaban todos en la transform del último → "1 cubo". El **push slot pool** (un UBO por draw, `lr->pushBuffers`/`pushBindGroups`) existía solo bajo `__EMSCRIPTEN__`; ahora se activa en **todas las plataformas**: `CmdExecuteGraph` resetea `im.pushSlot = 0` al inicio y lo incrementa por push sin guard (se quitaron los `#if __EMSCRIPTEN__`). El editor no lo disparó nunca: su escena 3D tiene 1 cubo y la UI comparte un push constante (solo screenSize), así que el overwrite no se notaba. De paso, `PhysicsDemo/main.cpp` ahora: **(1)** llama `LeirSettings::Get().Load()` en `main()` (faltaba — por eso siempre corría Vulkan aunque settings dijera webgpu), **(2)** pasa `LeirSettings::Get().graphics.backend` a `BackendFactory::Create` (antes hardcode `""`), **(3)** usa `GetShaderFileExtension()` para los shaders (`.wgsl` con webgpu, `.spv` con vulkan — antes hardcode `.spv`), **(4)** null-check del backend y se sacó el include muerto de `VulkanBackend.h`. Verificado por el usuario (2026-08-17): los 9 cubos rojos caen y se asientan con el backend WebGPU nativo, igual que con Vulkan/D3D12. La simulación Jolt es idéntica (backend-independiente); solo cambia el render.
- **Fase 6 — Export Web M3 COMPLETO, física Jolt en navegador** (2026-08-17, see `TODO_WEB_EXPORT.md` "Estado M3"): `WebEngineDemo` ahora corre física real — piso estático + 2 cubos dinámicos que caen y **se asientan** sobre el piso checker (verificado en Firefox). Cambios: **(1) `JobSystemSingleThreaded`** en `PhysicsWorld::Init` bajo `__EMSCRIPTEN__` (sin pthreads → sin SharedArrayBuffer/COOP-COEP → cualquier server estático; los jobs corren síncronos en el main thread, compatible con ASYNCIFY). **(2) Timestep fijo 1/60 con acumulador** (`m_Accumulator`, dt clamp 0.25 s) en `StepPhysics` — arregla el **tunneling** en web (a 60 Hz el cubo cae 0.16 m/paso; con dt variable de 25-40 fps atravesaba el piso de 1 m). **(3) Web build usa el `PhysicsWorld.cpp` real** + `RigidBody.cpp`/`Collider.cpp` (linka `Jolt` PRIVATE); el stub `PhysicsWorld.web.cpp` de M2 fue **eliminado**. **(4) CRASH resuelto — causa raíz: desbordamiento de pila wasm, NO física**: `Uncaught RuntimeError: index out of bounds` en `ProcessBodyPair` al PRIMER contacto real (cubo A vs piso, ~1.3 s), siempre antes del primer `printf` de la función. Diagnóstico con un **repro mínimo** (Jolt directo + `libJolt.a` real, mismos flags `-O0 -sASYNCIFY=1 -sALLOW_MEMORY_GROWTH=1`, corriendo bajo **node**) que reproducía el crash 1:1 (`memory access out of bounds`, mismos `[JoltAddHit] A=1 B=0`/`[JoltReadQ]` previos; el par (1,0) era **válido** — sin BodyID basura ni broadphase corrupto). Fix: **`-sSTACK_SIZE=16777216` (16 MB)** en las flags de LINK del demo — el frame gigante de `ProcessBodyPair` a `-O0` (~1500+ locals) + ASYNCIFY + cadena profunda (`MainLoop → … → JobFindCollisions → ProcessBodyPair`) agotaba la pila default de 5 MB en la entrada misma de la función (por eso "crasheaba al tocar el piso": la función solo se llama cuando existe un par real). **No se bajó calidad de física** (misma gravedad/step/settings, mismo commit Jolt `2e28006`); la pila es solo memoria de trabajo. Flag solo del build web (desktop usa pila nativa); un futuro `-O2` encoge los frames. Limpieza: revertido el `PhysicsSystem.cpp` instrumentado en `_deps` (gitignored) y eliminado el `[PhysDiag]` de `main.cpp`. Verificado (2026-08-17): cubos asientan sin rebotar/atravesar, log XConsole limpio; wasm 65.1 MB (Debug).
- **Fase 6 — Export Web M2 COMPLETO, motor completo (Scene+UI+Font) en navegador** (2026-08-16, see `TODO_WEB_EXPORT.md` "Estado M2"): `examples/WebEngineDemo` (2 cubos checker 256×256 + cámara órbita + RenderTexture viewport + barra con texto Roboto) corriendo el engine completo por WebGPU. Fases (todas verificadas): **(A) multi-textura web** — naga no compila `binding_array` → el backend degrada la tabla bindless a recurso único y liga **bind groups per-texture** (`Impl::textureBindGroups` + `GetTextureBindGroup(index)`, mismo `bindlessLayout`; el executor `CmdExecuteGraph` web resetea `bindlessSetSlot=-1`/`pushSlot=0` y por draw re-liga `GetTextureBindGroup(sampledTextures[0])`); **push slot pool** (`lr->pushBuffers`/`pushBindGroups` + `im.pushSlot++`, UBO de push por draw alineado a 16 — sin last-write-wins). **(B) `LeirEngineCore` static lib** (`engine/CMakeLists.web.txt`, nuevo, 45 sources web-safe, PUBLIC engine/include+glm, `LEIR_SHADER_DIR="/shaders"` PUBLIC, robusto a `include()` vía `LEIR_ROOT`) + **`engine/src/Physics/PhysicsWorld.web.cpp`** (stub sin Jolt: `StepPhysics` no-op, refs a punteros null — el header forward-declara JPH, el build web es 100% libre de Jolt). **(C) main loop web** — `Run()` → `emscripten_set_main_loop_arg(&FrameThunk, this, 0, true)` (infinite loop); `Frame(double)` espeja el bucle desktop (poll → EventQueue → scene/OnUpdate → InputManager.Update → OnRender); `m_LastFrameTime`; guards `GLFW_INCLUDE_VULKAN` con `#if !defined(__EMSCRIPTEN__)` en `CoreApplication.cpp`/`InputManager.cpp`. **(D) shaders `*.web.wgsl` (6)** — `Basic.frag.web.wgsl` **renombrado** desde `Basic.web.frag.wgsl` (D), `Sprite.frag.web.wgsl`/`UI.frag.web.wgsl` nuevos (texture única); `WebGPUBackend.h::GetShaderFileExtension()` → `".web.wgsl"` en web; **`ShaderLayout.cpp::SidecarPathFor` recorta `.web.wgsl`** → los 6 sidecars `.reflect.json` cargan en web y dan push sizes 144/112/8 (**crítico**: `sizeof(PushConstants)`=132 < 144 shader std430). Demo: `RenderPipeline`/`RenderTexture`/`UIRenderer`/`Font` desde `/assets/Roboto-Regular.ttf` (commiteado, release `googlefonts/roboto` v2.138 — el repo fuente no tiene TTF). Link: el exe agrega `--use-port=emdawnwebgpu.port.py` (glue JS wgpu; sin él undefined symbols `wgpuCreateInstance`/…). **Verificado (2026-08-16)**: Firefox — 2 cubos checker girando + órbita + título Roboto + logs XConsole en consola; wasm 22.5 MB / js 412 KB / data 370 KB. **Regresión M1 OK** tras el rename (3 cubos gris/rojo/azul con sus checkers 2×2 — el aspecto "4 cuadrados difuminados" es diseño M1, no regresión). Pendiente: M3 física, M4 audio, M5 CI (job emscripten compile-only), M6 commit/tag.
- **Fase 6 — Export Web (Emscripten + WebGPU en navegador) M1 COMPLETO, WebDemo renderiza en Firefox/Chrome/Opera** (2026-08-15, see `TODO_WEB_EXPORT.md`): standalone `examples/WebDemo` (cubo checker rotando + cámara auto-orbit) linkando los sources web-safe del motor (WebGPUBackend + BackendFactory + XConsole) vía `--use-port=emdawnwebgpu.port.py` + `contrib.glfw3`, preset CMake `emscripten` con `-sASYNCIFY=1` + `--preload-file engine/shaders@/shaders`. **`BackendFactory` extraído** a `RHI/BackendFactory.cpp` neutro (dispatch `"vulkan"/"d3d12"/"webgpu"`, `LEIR_BACKEND_WEBGPU=3` en `RHI.h`) — vivía en `VulkanBackend.cpp`. **emdawnwebgpu vendored** en `dependencies/emdawnwebgpu/` (webgpu.h con `WGPUBindGroupEntryExtras`/`WGPUBindGroupLayoutEntryExtras`, sTypes 0x00030005/6, mirroring wgpu-native) y **glue parcheada**: `wgpuDeviceCreateBindGroup::makeEntry` liga arrays vía extras; `wgpuDeviceCreateBindGroupLayout` pasa `bindingArraySize` (offset 16) al dict JS — el navegador NO lo infiere del WGSL. **Split `#if __EMSCRIPTEN__` en `WebGPUBackend.cpp`**: device vía `WaitAnyOnly` + `WaitAny(UINT64_MAX)` (necesita ASYNCIFY), superficie canvas (`WGPUEmscriptenSurfaceSourceCanvasHTMLSelector`), `WaitIdle` no-op, proc pointers estáticos (sin `LoadProc`), sin requiredFeatures/requiredLimits. **Dos descubrimientos de verificación**: (1) **naga web NO puede compilar `binding_array`** — el parser exige `enable wgpu_binding_array;` que es **native-only** (`naga/src/front/wgsl/parse/directive/enable_extension.rs`); el prepend del enable (primer intento) rompía Firefox ("the `wgpu_binding_array` extension is not supported in the current environment") → **revertido**. Fix definitivo: en `__EMSCRIPTEN__` el layout bindless compartido se crea **sin** arrays (recurso único: texture binding 0 + sampler binding 1) y `RebuildBindlessBindGroup` liga la textura+sampler del slot más bajo (o dummy); shaders web = variantes `*.web.wgsl` (`Basic.web.frag.wgsl`, textura/sampler únicos en `@group(1)`), que el demo carga bajo `__EMSCRIPTEN__`. Los nativos (wgpu-native/Vulkan/D3D12) conservan los arrays vía `#if !defined(__EMSCRIPTEN__)`. El error `createBindGroup: Missing required 'buffer' member of GPUBufferBinding` era **cascada** del shader roto (Firefox devuelve el pipeline con módulo inválido + uncaptured; la WebIDL union intenta GPUBufferBinding sobre un resource de array) — desapareció al arreglar el shader. (2) **Stencil ops en `Depth32Float`**: WebGPU valida que `stencilLoadOp`/`stencilStoreOp` sean `None` en formatos sin aspecto de stencil → se quitaron esas asignaciones en el pass principal de `BeginFrame` y en `CmdBeginRenderPass` (quedan `Undefined`=0 → la glue pasa `None`). **Verificado (2026-08-15)**: cubo texturizado rotando, log limpio, en **Firefox 153 + Chrome + Opera**; wasm 10.39 MB / js 426 KB / data 15,209 B. Pendiente: M2 motor completo, M3 física (Jolt + JobSystemSingleThreaded), M4 audio, M5 CI, M6 commit/tag.
- **Fase 5 — Backend WebGPU (wgpu-native v29) COMPLETO, renderiza con parity vs Vulkan** (2026-08-15, see `TODO_RHI_SLANG.md` "Fase 5 (WebGPU)"): `WebGPUBackend.cpp` inicializa y renderiza por completo sobre wgpu-native **v29** con backend **DX12 forzado**. **Causa raíz del bloqueo Vulkan**: wgpu-hal `adapter.rs` (`PhysicalDeviceFeatures::to_wgpu`) concede `TEXTURE_BINDING_ARRAY` solo si los 6 sub-features de descriptor indexing están activos incl. non-uniform indexing; Intel Gen9 (UHD 620) no lo tiene (Gen11+) → se fuerza DX12 vía `WGPUInstanceExtras.backends = WGPUInstanceBackend_DX12` encadenado a `WGPUInstanceDescriptor.nextInChain` (DX12 tier 3 sí: binding arrays + non-uniform). **Quirks v29 resueltos en secuencia** (todos verificados en el header): (1) bind group = **1 entry por binding** con `WGPUBindGroupEntryExtras` (arrays `textureViews`/`samplers`), no N entries sueltas; (2) tamaños de array del layout en la chain `WGPUBindGroupLayoutEntryExtras.count` (el campo plano `bindingArraySize` se ignora); (3) `requiredLimits` es `WGPULimits const*` directo (v29 no tiene `WGPURequiredLimits`) con `WGPU_LIMITS_INIT` (nunca `{}` → "min_storage_buffer_offset_alignment value 0") + chain `WGPUNativeLimits`: `maxBindingArrayElementsPerShaderStage=32`, `maxBindingArraySamplerElementsPerShaderStage=16`, `maxNonSamplerBindings=WGPU_LIMIT_U32_UNDEFINED` (0 → heap DX12 de 0 descriptores → device lost); (4) 2 features nativos: `TextureBindingArray` + `SampledTextureAndStorageBufferArrayNonUniformIndexing` (naga lo exige: UI.frag indexa con varying); (5) entry points `vs_main`/`ps_main` (no `main`); (6) `color.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED` en los 3 color attachments (v29 lo movió al attachment de color, no depth). **Dos fixes de diseño**: (7) **masking del viewport RT** — wgpu prohíbe una textura que es color target presente en un bind group ligado en el mismo pass → `CmdBeginRenderPass` enmascara los slots bindless que son attachments (fallback a dummy view) + rebuild del bind group, y `CmdEndRenderPass` restaura + rebuild; (8) **clamp del scissor** contra `passW/passH` (wgpu valida estrictamente: scissor UI 992 > target 991 por el ceil de DPI). **Convención NDC**: viewport **positivo (Y-up, estilo D3D12)**, sin flip propio — wgpu-native traduce el flip a su backend subyacente y sale la parity vs el Vulkan negativo. Notas de vida: el adapter debe vivir toda la vida del backend (`wgpuSurfaceGetCapabilities` panica si se suelta); `wgpuInstanceWaitAny` **panica** (init bloqueante = spin `InstanceProcessEvents`); `WaitIdle` = `DevicePoll(device, true, nullptr)` cargado dinámicamente (ordinal 93, fuera del header); `maxAnisotropy` ≥ 1. **Verificado (2026-08-15)**: render completo (escena cubo iluminado + UI panels/tabs/status + sprites), **parity vs Vulkan 1.59%** (≈ baseline 1.4% = overlay FPS/cursor), ctest 2/2, STDERR vacío (sin uncaptured errors wgpu), close limpio.
- **Plan B — Fase 4: `GCommandGraph` (command recording per frame) en Vulkan + D3D12** (2026-08-14, see `TODO_RHI_SLANG.md` §3.2 + §5 "Plan B" step 4): per-frame `RenderPassRecord` + `DrawRecord` are recorded into a backend-neutral graph; each backend translates to native commands and **generates image-layout transitions by last-use tracking**, replacing the manual `CmdTransitionImageLayout`/`CmdBarrier` calls that used to live in `RenderTexture`, `RenderPipeline`, `Mesh`, `Material` and `UIRenderer`. **`GCommandGraph.h`** (new, header-only, `engine/include/LeirEngine/RHI/`): `GRecordType{BeginRenderPass,EndRenderPass,Draw}`; `GAttachment{image,isDepth}`; `GRenderPassRecord{passTemplate, framebuffer, attachments}`; `GDrawRecord` (pipeline, layout, setBindings, vertex/index buffers, push, viewport/scissor, `sampledTextures` = bindless indices sampled by the draw). Recording API is stateful (`m_Current` snapshotted on each Draw/DrawIndexed — repeated state only appears in the first record; native state persists between records like immediate mode). **`RenderBackend::CmdExecuteGraph(cmd, graph)`** pure virtual. **Vulkan** (`VulkanBackend`): `bindlessImages` (index→VkImage) + `imageLayouts` (VkImage→layout) + `GetLayout`; registrations seed textures as `ShaderReadOnly` (the one-shot upload leaves them there); executor transitions attachments at pass-begin, tracks color→`ShaderReadOnly` after the pass, and transitions sampled textures before each draw. **D3D12** (`D3D12Backend`): `bindlessImages` (index→`ImageRec*`) populated in `UpdateBindlessTexture`, reuses `CmdTransitionImageLayout` with the resource's **real** `state` (no seeding), and does an **explicit** color→`PIXEL_SHADER_RESOURCE` transition at EndRenderPass (D3D12 has no implicit final layout). **Design decision (two graphs per frame)**: the editor's scene graph owns the `RenderTexture` pass (records Begin/EndRenderPass); the UI graph and `PhysicsDemo`'s graph are **pass-less** (draws only, inside the native swapchain overlay pass / `BeginFrame(false)` 3D pass) — `VulkanDevice` is barely touched. `Texture2D::GetDescriptorInfo`/`RenderTexture::GetDescriptorInfo` now fill `info.image` (`RHIDescriptorImageInfo.image` added). **Bug found & fixed during verification**: VUID `vkCmdDraw-None-09600` (a sampled texture expected in `SHADER_READ_ONLY` while its actual layout was `UNDEFINED`) — a freshly created/resized `RenderTexture` color image is actually `UNDEFINED`, but registration seeded the tracker as `ShaderReadOnly`, so the executor emitted an `SRO→ColorAttachment` barrier with a false `oldLayout`. Fix: in `CmdExecuteGraph`'s `BeginRenderPass`, color attachments are **always transitioned from `UNDEFINED`** (the pass clears them with `loadOp=CLEAR`, so the discard is safe and `UNDEFINED` is always a legal `oldLayout`); UNDEFINED depth images are left to the render pass (initialLayout UNDEFINED). **Verified (2026-08-14)**: ctest 2/2; editor Vulkan + D3D12 both clean (0 VUIDs / 0 debug-layer errors, empty stderr) with clean closes; `PhysicsDemo` (pass-less graph) clean; cross-backend pixel parity 1.0% ≈ baseline 1.4%.
- **Plan B — Fase 3: bindless-first (descriptor indexing) en Vulkan + D3D12** (2026-08-14, see `TODO_RHI_SLANG.md` §5 "Plan B" step 3): the per-texture single-sampler descriptor sets are gone — each backend owns one **bindless texture table** indexed directly from shaders, and the documented D3D12 SRV-heap leak on RT resize is eliminated structurally. **API** (`RHI.h`): `RHIDescriptorBinding.bindless` (runtime array; `count=UINT32_MAX` = unbounded, replaced by the backend bound), `RHIDescriptorWrite.dstArrayElement`, `Format::R32_SFLOAT`, `RHIVertexAttribute.semanticIndex`. `RenderBackend`: `RegisterBindlessTexture`/`UpdateBindlessTexture`/`UnregisterBindlessTexture` (free-list of indices)/`GetBindlessDescriptorSet`/`GetBindlessMaxTextures`. **Vulkan**: descriptor indexing with **update-after-bind** — required because the Intel UHD iGPU has `maxPerStageDescriptorSamplers=64`/`maxPerStageResources=200`, so a 200-descriptor CIS binding violated `VUID-VkPipelineLayoutCreateInfo-descriptorType-03016` + `VUID-VkGraphicsPipelineCreateInfo-layout-01688`; layout = per-binding `VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT` + set-level `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT`, pool with `VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT`, table sized by the update-after-bind limits → **1,048,576 textures**. SDK 1.4.357 header gotchas: the set-level `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PARTIALLY_BOUND_BIT` was **removed** (per-binding flags via `VkDescriptorSetLayoutBindingFlagsCreateInfo` now), the generic `descriptorBindingUpdateAfterBind` feature was **removed** (use `descriptorBindingSampledImageUpdateAfterBind`), and `maxPerStageDescriptorUpdateAfterBindResources` was renamed `maxPerStageUpdateAfterBindResources`. **D3D12**: SRV heap (4096) + sampler heap enlarged 64 → **2048** (`kBindless`); bindless root signature = SRV table `space1` + sampler table `space2` (both `NumDescriptors=kBindless`, PIXEL) — DXIL splits `Sampler2D textures[]` into `t0,space1unbounded`+`s0,space2unbounded`; `CreateSampler` no longer allocates a slot (keeps the `D3D12_SAMPLER_DESC`); `UpdateBindlessTexture` rewrites SRV+sampler **in-place** at `SrvCpu(i)`/`SamplerCpu(i)` → **viewport-RT resize never grows the heaps**; the legacy `AllocSrv`/`AllocSampler`/`samplerCache`/`srvSlot` path is now dead (the only remaining `WriteDescriptorSets` is the UBO). **Shaders** (6): `Basic.vert/frag` + `Sprite.vert/frag` gained `uint textureIndex` at the end of the push constants (same offsets in C++: 128/96; shader std430 144/112; both stages declare the identical struct → single merged range) plus a bindless `Sampler2D textures[]` (`[[vk::binding(0,1)]]` Basic / `(0,0)` Sprite) sampled as `textures[push.textureIndex]` (uniform per draw → no NonUniform). `UI.vert` gained the loc3 attribute `float fragTexIndex : TEXCOORD1` (`UIVertex.textureIndex`, `R32_SFLOAT`, D3D12 semantic TEXCOORD/1); `UI.frag` uses `textures[NonUniformResourceIndex((uint)input.fragTexIndex)]` (a UI draw mixes several textures) but **only on targets that support it** — `NonUniformResourceIndex` doesn't compile on WGSL/Metal/GLSL, so it's emitted via `__target_switch` (`case hlsl: case spirv:` → NonUniform, `default:` → plain index); verified the DXIL index lands at DWORD 32 (Basic) / 24 (Sprite), matching the C++ structs. **Engine**: `Texture2D` (registers in `CreateFromData`, unregisters in dtor, `GetBindlessIndex`), `RenderTexture` (registers in ctor, `UpdateBindlessTexture` in-place in `Resize`, unregisters in dtor; `m_DescSetLayout/m_DescPool/m_DescriptorSet` removed), `Material` (no pool/set; `Bind` binds the global bindless set at set 1; legacy fallback layout bindless), `RenderPipeline` (pushes `textureIndex` in `RenderMeshRenderer`; `RenderSprite` binds the bindless set at set 0 and drops `descSetCache`/`descPool`), `UIRenderer` (dropped `GetOrCreateDesc`/`m_DescCache`/`m_DescPool`; binds the bindless set **once** in `Flush`; batching by `texIndex`+scissor; viewports use `GetBindlessIndex()`). **Verified (2026-08-14)**: ctest 2/2; editor Vulkan + D3D12 both clean (0 VUIDs / 0 debug-layer errors, previously a VUID storm on the 200-CIS binding), clean closes; Vulkan bindless table `1,048,576 (update-after-bind)`; cross-backend pixel parity 1.52% ≈ baseline 1.4%; SRV/sampler heaps no longer grow on RT resize (no allocation path left).
- **Plan B — Fase 2: `GPipeline`/layouts derivados de la reflection por sidecar offline** (2026-08-14, see `TODO_RHI_SLANG.md` §5 "Plan B" step 2): all hand-written descriptor layouts replaced by layouts derived from a canonical **reflection sidecar** emitted by the editor shader tooling — `<name>.reflect.json` = `{stage, bindings:[{name,set,binding,type,count,stage}], pushConstants:[{stage,offset,size}]}` — loaded by the engine at runtime (engine stays 100% slang-free; identical flow for SPIR-V and DXIL). New `engine/.../Rendering/ShaderLayout.h/.cpp` (`LEIR_API`): `LoadShaderReflectionFromSidecars`, `CreateSetLayoutsFromReflection` (ascending set order), `CreatePipelineLayoutFromReflection` (validates each set + **merges overlapping push ranges by offset+size with combined stage flags**, required by Vulkan), `ValidateSetLayoutAgainstReflection`. Migrated: `Material` (set0 UBO + set1 sampler), `UIRenderer` (set0 sampler, push 8B vertex), `RenderPipeline::Sprite` (set0 sampler, push 96B); each keeps a hand-written legacy fallback when the sidecar is missing (engine standalone). Editor: `ShaderExporter::WriteRuntimeSidecars` generates sidecars into `LEIR_SHADER_DIR` in `OnInit` **before** `m_Shader` creation (compiler moved earlier; the old hot-reload block no longer recreates it), `ExportAll` emits them in the export root too (SlangExportTest now asserts 6 `.reflect.json`), and `ShaderHotReloader` regenerates the sidecar on every reload (`reflect=true`). Two reflection fixes found during implementation: (1) Slang reports SPIR-V resources as `DescriptorTableSlot`/`Mixed` categories — classify via the `TypeLayoutReflection` **binding ranges** (`getBindingRangeType` → `BindingType::PushConstant`/`ConstantBuffer`/`CombinedTextureSampler`); (2) push-constant size = `leaf->getElementTypeLayout()->getSize()` (bytes), not `getSize(PushConstantBuffer)` (returns 1). `nlohmann_json` became PUBLIC on LeirEngine (header-only; the editor needs it to serialize sidecars). **Verified (2026-08-14)**: ctest 2/2; editor Vulkan + D3D12 both clean (0 Vulkan VUIDs / D3D errors — previously a storm of `vkCreateDescriptorSetLayout` duplicate-binding, push-range-size and set-mismatch errors); pixel parity vs Fase 1 (Vulkan 0.39%, D3D12 0.25% = cursor/FPS noise) and cross-backend 1.33% ≈ baseline 1.4%; clean closes, empty stderr.
- **Plan B — Fase 1: `GCaps` + `GPassTemplate` (RHI completo §3, first increment)** (2026-08-14, see `TODO_RHI_SLANG.md` §5 "Plan B"): (1) **`GCaps`** — new struct in `RHI.h` (max textures/UBOs/samplers/SSBOs per table, push-constants size, MRT/maxRT/maxTex, min UBO offset alignment, features: bindless/MRT/instancing/compute/storage/sRGB/wireframe/aniso) + `RenderBackend::GetCaps()`. Vulkan fills from `VkPhysicalDeviceProperties` + `VkPhysicalDeviceFeatures2` (descriptor-indexing query) — note: this SDK 1.4.357 header **drops `computeShader` from `VkPhysicalDeviceFeatures`** (struct trimmed), so compute is set `true` unconditionally (it's core Vulkan 1.0); D3D12 fills from `D3D12_FEATURE_DATA_D3D12_OPTIONS` (binding tier) + heap limits (1M CBV/SRV/UAV, 2048 samplers, 256B root constants, 8 MRT, 16384 tex). Editor prints `[GCaps]` per backend. Verified: Vulkan `textures=200 ubo=200 samplers=64 ssbo=200 push=256B bindless=true` (Intel UHD iGPU, descriptor indexing present), D3D12 `1M/1M/2048/1M bindless=true` (tier 3). (2) **`GPassTemplate`** — `RHIPassTemplateDesc`/`RHIPassTemplate` + `CreatePassTemplate`/`DestroyPassTemplate`; `CmdBeginRenderPass` signature changed to take a template (was `(cmd, renderPass, framebuffer, clearValues, w, h)`, only `RenderTexture` called it). The backend precomputes clears + viewport + scissor once: Vulkan stores `VkClearValue`s, the Y-flip viewport, renderArea (from scissor); D3D12 stores clears + `D3D12_VIEWPORT`/`D3D12_RECT`. `RenderTexture` uses a persistent template rebuilt only on resize or when the requested clears change (`BeginRender` compares `clearColor.color`/`depthClear` to the baked ones). **Bug found & fixed during verification**: `Resize` called `DestroyPassTemplate` explicitly AND `BuildPassTemplate` destroyed it again → double `delete` of `PassTemplateRec` → `std::vector::_Orphan_all` on freed memory → `0xC0000005` caught by `CrashDiagnostics`; removed the explicit destroy in `Resize` and made `BuildPassTemplate` reset the handle after destroy. **Verified**: parity vs baseline (Vulkan 0.3% / D3D12 0.2% pixel diff = cursor+FPS overlay noise, cross-backend 1.5% ≈ baseline 1.4%), validation/debug layers clean (0 VUID), closes 219-321 ms.
- **Plan A / Fase 3 complete — editor shader tooling with Slang** (2026-08-14, see `TODO_RHI_SLANG.md` §5): public `IShaderCompiler` interface (`engine/include/LeirEngine/RHI/IShaderCompiler.h`, **no `LEIR_API`** — header-only inline dispatch; exporting it produced LNK2019/LLVM-mangling breakage on MSVC) + `SlangShaderCompiler` impl in `editor/src/Shaders/` + multi-format `ShaderExporter` (SPIR-V/DXIL/MSL/WGSL/GLSL-450) + `ShaderHotReloader` (polls `engine/shaders/*.slang` per frame, recompiles to `.spv`/`.dxil`, reloads pipelines) + DebugPanel Export/Reload buttons. Engine DLL stays slang-free (isolation). Three findings with fixes: (1) **`LEIR_API` must NOT be used on the header-only `IShaderCompiler`** (removed). (2) **`slang_createGlobalSession` (C API simple) leaves `enableGLSL=false`** → `error[E38201]: 'glsl' module not available`; fix: `SlangGlobalSessionDesc desc = {}; desc.apiVersion = SLANG_API_VERSION; desc.enableGLSL = true; slang_createGlobalSession2(&desc, &session)`. (3) **`loadModuleFromSource` (in-memory modules) breaks capability validation**: any global `cbuffer` requires `Std140DataLayout`, *unavailable* on DXIL/Metal/WGSL → `error[E36107]` on `getEntryPointCode` (same `.slang` compiles fine via `slangc`); not `[[vk::binding]]`, matrix layout, language version, or profile (bisected with a harness). Fix: load from the **file system** (`ISession::loadModule`) instead; `CompileFromSource` stages to a temp `.slang` file. **Verified (2026-08-14)**: export 6/6 on all 5 targets (30 files in `shaders_export/`, SPIR-V 2092 B / DXIL 5412 B / MSL 2362 B / WGSL 3666 B / GLSL 2029 B per Basic.vert); **hot-reload verified live** — modified `Basic.vert.slang` while the editor ran → `[HotReload] ... -> Basic.vert.dxil (5412 bytes)`, stderr empty (no VUIDs), clean exit. libslang now comes **vendored** in `editor/vendor/slang/` (v2026.14.1, modern `slang-compiler` naming, no deprecated `slang.dll` proxy — see `cmake/SlangTooling.cmake` + `TODO_RHI_SLANG.md` §5 "Estado Plan A").
- **Plan A / Fase 3 vendoring + CI fix — libslang vendored in `editor/vendor/slang/` v2026.14.1** (2026-08-14, CI green on all 3 platforms): the editor tooling no longer depends on `$VULKAN_SDK`; prebuilt libslang from the shader-slang release is committed in-repo (`windows/slang-compiler.lib`+4 DLLs, `linux/libslang-compiler.so.0.2026.14.1`+3, `macos/libslang-compiler.0.2026.14.1.dylib`+3, `include/`, `LICENSE.txt` — no `slang.dll` proxy, no symlinks, no `slang-llvm`). Editor gate opened to **all platforms** (was `WIN32 AND MSVC AND DEFINED ENV{VULKAN_SDK}`). Shared CMake helper `cmake/SlangTooling.cmake` → `leir_setup_slang_target(<target>)`: vendored include + per-OS link + POST_BUILD copy of the runtime libs next to the exe + `BUILD_RPATH` (`$ORIGIN` Linux / `@loader_path` macOS, both + `${CMAKE_BINARY_DIR}/engine`). New CTest smoke test **`SlangExportTest`** (`tests/SlangExportTest.cpp`, wired in `tests/CMakeLists.txt`): runs `ShaderExporter::ExportAll` (6 shaders × 5 targets) via the vendored compiler → validates link + dynamic loading (dlopen/dyld) + codegen on every CI platform. **CI had been red since `3dfc232`** (Slang migration, hidden by `[skip ci]` commits): (1) `LEIR_SHADER_DIR`/`LEIR_SHADER_SOURCE_DIR` were defined only inside `if(SLANGC)` → macOS/Linux runners (no `slangc`) failed with "undeclared identifier LEIR_SHADER_DIR" in `RenderPipeline.cpp`; now **always defined** in `engine/CMakeLists.txt` (shader compile still gated to `slangc` best-effort). (2) root `CMakeLists.txt` was missing **`enable_testing()`** → `ctest` never registered/ran any test ("No tests were found"); `PhysicsTest` + `SlangExportTest` now actually run. (3) `TempSlangFilePath()` falls back `TEMP`→`TMPDIR`→`TMP`→`.` (was `TEMP`-only). **Verified (2026-08-14, Windows/MSVC)**: full build clean, `SlangExportTest` passes (slang 2026.14.1, `[Export]` 6/6 × SPIR-V/DXIL/Metal/WGSL/GLSL450 → 30 files), `PhysicsTest` passes, editor launches against vendored DLLs (`[Slang] global session created (2026.14.1)`, `Shader compiler ready: 2026.14.1`), clean close, empty stderr. **CI green on all 3 platforms (2026-08-14)** — Windows CI now builds with **MSVC (`cl`)** instead of the runner image's MinGW (MinGW-built exes hung silently before `main()`, so neither test could run): `ilammy/msvc-dev-cmd@v1` + `-DCMAKE_CXX_COMPILER=cl` makes `windows-ci-debug` match the local `windows-debug` preset; `PhysicsTest` + `SlangExportTest` pass there (DXIL shaders compile too, slangc found), and macOS arm64 dylibs (`@loader_path`) / Linux `.so` (`$ORIGIN`) verified green. `ctest --timeout 120` kept as a guard against silent hangs.
- **D3D12 render parity with Vulkan completed** (BUG01/BUG02/BUG03, see `TODO_RHI_SLANG.md` "Checkboxes — Paridad de render D3D12 vs Vulkan"): (1) **BUG03 cube/camera inverted** — removed `m_ProjectionMatrix(1,1) *= -1.0f` from `Camera::SetPerspective` (it was Vulkan's y-down NDC compensation that broke D3D12's y-up NDC = GLM) and moved the flip to the Vulkan backend via negative-height viewports in the 3D pass (`VulkanDevice::BeginFrame` swapchain + `VulkanBackend::CmdBeginRenderPass` for the RenderTexture). Rule learned: NDC convention is per-backend (D3D12/Metal/WebGL y-up → positive viewport; Vulkan y-down → negative viewport); shared code stays pure GLM and front-face is CCW in all backends. The WebGPU backend (wgpu-native, 2026-08-15) uses **positive** viewport (Y-up, D3D12-style) — wgpu-native translates the flip to its underlying backend, so it renders in parity with the negative-flipped Vulkan output. (2) **BUG01 darker colors in D3D12** — the swapchain was `B8G8R8A8_UNORM` (Vulkan: `_SRGB`). The straightforward fix (swapchain + ResizeBuffers + colorFormats → `UNORM_SRGB`) **device-removes on the Intel UHD driver** (`DXGI_ERROR_DEVICE_REMOVED` 0x887A0001 at CreateSwapChainForHwnd). Final equivalent fix: the backbuffer **resource** stays `UNORM`, but the **RTVs are created as `B8G8R8A8_UNORM_SRGB`** (`InitBackBuffers`, `D3D12_RENDER_TARGET_VIEW_DESC` `TEXTURE2D`) with main/overlay pass `colorFormats` = `UNORM_SRGB` — the sRGB encode happens at the RTV store, identical to an sRGB swapchain. (3) **BUG02 pixelated UI at `hidpi:true`** — pixel-level diagnosis (DPI-aware captures, cross-correlation) proved D3D12 and Vulkan renders are pixel-aligned and nearly identical; the real cause was the **font atlas rasterized at `fontSize` logical px and upscaled 1.25× with a `Nearest` sampler** (chunky strokes). Universal fix: **rasterize the atlas at `fontSize × contentScale`** with all metrics kept in **logical units** (atlas px ÷ scale), so each texel maps 1:1 to a physical pixel — crisp text at any DPI in both backends (`Font` ctor gains `float contentScale`; `Font::SetContentScale()` re-rasterizes in place so all `Font*` holders stay valid; editor passes `GetContentScale()` and calls it in `OnContentScaleChanged`). All three verified by the user (2026-08-13). Known D3D12 limitations (documented, deferred to the full RHI): descriptor SRV/sampler slots are never freed (no `DestroyDescriptorSet` in the RHI interface — every viewport-RT resize leaks 1 SRV slot from a 4096 heap); `cmdList4` is unused; `mainRenderPass`/`overlayRenderPass` are never freed in `~Impl`. See TODO_RHI_SLANG.md "Limitaciones conocidas del backend D3D12".
- **D3D12 teardown crash (slow close) fixed + SEH diagnostics added** (2026-08-13): closing the editor in D3D12 hung for several seconds then died. `CrashDiagnostics` only caught C++ exceptions (`set_terminate`) and an AV/SEH fell straight through to WER (which collects a dump = the "several seconds") with no log. (1) Added `SetUnhandledExceptionFilter` (`CrashDiagnostics::OnUnhandledException` → `LogStackWalk`, then `EXCEPTION_EXECUTE_HANDLER` = no WER, no dialog) and refactored the stack-walk helper to take a header string (reused by the alloc hook). This would also have caught the old 5s-close double-free. (2) The SEH dump pinpointed the crash: `RenderTexture::~RenderTexture` → `DestroyResources` → `DestroyMemory` → releasing an `ID3D12Resource` while the GPU still referenced it — the D3D12 **debug layer** (`D3D12SDKLayers`) raised `0x87d` mid-release. Root cause: the dtor destroyed the color/depth resources **without a `WaitIdle`**, while `Resize()` does wait. Fix: `m_Device->WaitIdle()` at the top of `RenderTexture::~RenderTexture` (mirrors `Resize`; the editor destroys the viewport RT in `OnShutdown` before the backend's own WaitIdle). Verified: close now 162-186 ms with a clean, complete teardown (no crash log, no new WER reports, stderr empty) across 3 runs.
- **GitHub Actions CI green again on all 3 platforms** (2026-08-08): (1) the editor's Windows-only crash-diagnostics block was extracted from `main.cpp` into its own `editor/src/CrashDiagnostics.h/.cpp` (single portable entry `CrashDiagnostics::Init()`, one `#ifdef` per platform, macOS/Linux skeletons ready — see `CRASH_DIAGNOSTICS.md`); `dbghelp` is now linked via CMake (`if(WIN32) PRIVATE dbghelp`) because the old `#pragma comment(lib, "dbghelp.lib")` is MSVC-only and MinGW (CI `windows-ci-debug` preset) ignored it → `undefined reference to __imp_SymInitialize/SymFromAddr`. (2) Linux runner failed on missing `<cstring>` in `engine/src/Rendering/Texture2D.cpp` and `engine/src/UI/Font.cpp` (they call `memcpy` but only MSVC pulls `<cstring>` transitively) — added the include to both. Verified green: windows-latest + macos-latest + ubuntu-latest.
- **Hit-testing mirrors the clip** (console buttons fix, 2026-08-08): `UICanvas::HitTestRecursive` now carries the active clip rect and applies the same intersect/fast-reject logic as `UIRenderer::RenderElement` (see the "UI Clipping, Scrollbars & Wheel" section). Before, scrolled console lines extended over the ConsoleHeader area in absolute coords, and since hit-testing ignored clips, the `ConsoleLine` labels captured hover/click meant for the Info/Warn/Error/Clear buttons.
- **`TextAreaDebugPanel` read-only area removed** (2026-08-08): the 40-line `SetEditable(false)` test area added during the UITextArea scroll work was deleted on request — the "Text Area" tab keeps only the editable UITextArea + status label. The `SetEditable` feature itself stays in the engine.
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
- **UITextArea full scroll (Fase 2, see `TODO_UI_TEXTAREA.md`)**: `UITextArea` now has its own `m_ScrollOffset` + `SetClip(true)` in its ctor. `GetContentSize()` (max line width × line count) vs `GetViewportSize()` (widget rect minus enabled scrollbar strips, default 10px `m_ScrollbarWidth`) drive `GetMaxScrollY/X()`. It owns vertical + horizontal `UIScrollbar` children created in the ctor, positioned in `OnLayoutComputed`/`SyncScrollbars` with full-rect TopLeft + absolute offsets (same convention as `ScrollView::SyncScrollbar`), `SetActive(overflow)`, `SetRange`/`SetValue` synced both ways. `OnScroll` = wheel vertical, Shift+wheel horizontal (`delta × lineHeight`). `EnsureCaretVisible()` auto-follows after every cursor mutation (keys, click/drag, typing, Enter, selection) with a one-line margin. `UIRenderer` now subtracts the offset from text baseline, caret, and each multi-line selection rect; textarea text is **no longer word-wrapped** (`LayoutText` maxWidth=0) so long lines overflow into the horizontal scroll space. Pointer hit-testing adds `m_ScrollOffset` to local coords so clicks land on the right line/col when scrolled. (`TextAreaDebugPanel` briefly had a second read-only area of 40 long lines for testing — verified and then removed on request, keeping only the editable area + status.)
- **`UITextInput::SetEditable` (read-only text fields)**: `bool m_Editable` (default true) + `SetEditable()`/`IsEditable()`. When `false` the control is read-only but still scrollable: `OnPointerDown` returns false (no caret placement or focus), `OnKeyDown`/`OnTextInput`/`InsertChar`/`OnPointerMove` guard editable, `OnFocus` only marks focused when editable, `IsCaretVisible()` requires editable, and `SetEditable(false)` clears focus/capture/selection via the canvas. `SetText`/`SetPlaceholder`/colors unaffected.
- **UITextArea perf fix — O(N²) → O(N)** (see `TODO_UI_TEXTAREA.md` Fase 3, verified by user): with the 40-line read-only area visible FPS fell 60 → 10. Root cause: `UITextArea::GetContentSize()` was **O(N²)** — per line it called `GetLineEnd(line)` (rescan from start) + `GetCursorXAt(GetLineEnd(line))` (rescan from start again), ~40 rescans per call, and it runs ~3-4× per frame (`SetScrollOffset` + `SyncScrollbars` in `OnLayoutComputed`) ≈ 90M char-ops/frame. Fix: `GetContentSize()` rewritten as a **single-pass O(N)** sweep over `m_Text` — one walk decoding each codepoint (same UTF-8/space/advance logic as `GetCursorXAt`), accumulating current line width + max line width, counting lines via the `\n`. No per-line `GetLineEnd`/`GetCursorXAt`. FPS stable at 60.
- **UITextArea word wrap (Fase 4, see `TODO_UI_TEXTAREA.md`)**: optional per-instance `SetWordWrap(bool)`/`IsWordWrapEnabled()`. When on, a lazily-rebuilt visual-row model (`m_VisualRows` of `{startByte, endByte, width}`) makes visual rows ≠ logical lines: `\n` closes a row (empty rows included), and long runs wrap at `wrapLimit = max(0, cr.z - vstrip - 8)` — with a previous space in the row the break lands at the space (word stays intact), without one it hard-breaks. The model is invalidated via `m_ModelGen` (bumped by `SetText`/`SetFont`/`OnTextMutated`/`SetWordWrap`) and rebuilt lazily in `EnsureVisualRows()` when the gen **or** the computed wrap width changes (so resizing the widget re-wraps). `GetLineCount/GetLineStart/GetLineEnd/GetCursorLine/GetCursorCol/GetCursorXAt` now operate on visual rows (wrap off → identical to previous logical-line behavior). `Home`/`End` move to the active *visual* row start/end; Up/Down keep `m_TargetX` and scan the target row with correct space-width. `GetContentSize()` with wrap ON returns `{viewport.x, rows*lineH + 8}` so the horizontal scrollbar hides automatically (no hscroll when wrapping); wrap OFF keeps the single-pass O(N) max-width of all rows + hscroll. UIRenderer draws each wrapped row via `LayoutText(rowSubstr, 0)` shifted by `line * lineH − scrollY`; caret/selection render against the visual row. Editor: new `TextAreaWrapPanel` ("Text Area Wrap", tab in `kDebugIds`) with a toggle button + live status (wrap, logical vs visual lines, cursor).

