# LeirEngine — TODO List

> **Estado actual:** Core, Renderer (Vulkan), Physics (Jolt), UI system propio y Editor base están
> implementados. Pendiente: Audio (SoLoud), Serialización & Assets, Animaciones, Documentación,
> build WSL/Linux y MoltenVK. Detalle de lo ya hecho en cada fase abajo.

## Fase 0 — Setup Inicial
- [x] Crear estructura de directorios del proyecto
- [x] `dependencies/CMakeLists.txt` con FetchContent (GLFW, GLM, Vulkan, stb, Jolt, SoLoud, nlohmann_json, cereal)
- [x] `CMakeLists.txt` raíz con superbuild
- [x] `CMakePresets.json` (windows-debug, windows-release, linux-debug)
- [x] `engine/CMakeLists.txt` → compila `LeirEngine.dll` / `LeirEngine.so`
- [x] `editor/CMakeLists.txt` → compila `LeirEngineEditor.exe`
- [x] `engine/include/LeirEngine/Core/Export.h` (declspec y visibility)
- [x] `engine/src/Core/CoreApplication.cpp` + `editor/src/main.cpp` (GLFW window)
- [x] `.gitignore`
- [x] Verificar build en Windows ✓ (`LeirEngine.dll` + `LeirEngineEditor.exe`)
- [ ] Verificar build en WSL/Linux

## Fase 1 — Core Engine
- [x] `CoreObject` (name, uuid, active, Transform, parent/children)
- [x] `Transform` (position, rotation, scale, local/world, dirty flag, matrices, parent/child chain)
- [x] `Object3D` (hereda de CoreObject, world transform 3D, bounding box)
- [x] `Object2D` (hereda de CoreObject, transform 2D, sorting layer)
- [x] `Component` base (onAwake, onStart, onUpdate, onDestroy, getOwner, getScene)
- [x] `Scene` (name, objects, createObject3D/2D, destroyObject, onUpdate, onRender)
- [x] `SceneManager` (load, unload, getActiveScene)
- [x] Sistema de componentes: `addComponent<T>`, `getComponent<T>`, `hasComponent<T>`, `removeComponent<T>`
- [x] `Application` (main loop, deltaTime, init/shutdown, SceneManager + InputManager integrados)
- [x] `InputManager` (wrap GLFW: keyboard, mouse, gamepad, edge detection)
- [x] Logging propio `XConsole` (Core/Log.h + Log.cpp) — reemplaza spdlog: formatter runtime, ring buffer 1000, stdout/stderr. Ver `TODO_LOG_SYSTEM.md`

## Fase 2 — Renderer (Vulkan)
- [x] `RHI` (Render Hardware Interface): Device, Swapchain, CommandBuffer
- [x] Pipeline creation (Vertex, Fragment shaders)
- [x] Compilación de shaders GLSL→SPIR-V en build time (glslc)
- [x] `Mesh`, `VertexBuffer`, `IndexBuffer`, `UniformBuffer`
- [x] `Texture2D` (carga con stb_image)
- [x] `Image` (CPU-side, SetPixel/GetPixel/SavePNG, separado de Texture2D)
- [x] `Material` (shader + parameters + textures)
- [x] `MeshRenderer` component
- [x] Forward renderer básico (diffuse + specular lighting)
- [x] `Camera` component (view/projection, frustum culling)
- [x] `Light` component (directional, point, spot)
- [x] Sprite 2D overlay system (pipeline, SpriteRenderer, SpriteSheet, push constants con uvRect)
- [ ] MoltenVK integration para macOS/iOS
- [x] Limpiar recursos en teardown: `VUID-vkDestroyDevice-device-05137` (recursos no destruidos antes de `vkDestroyDevice`). Verificado: 2 corridas limpias con validation layers (dock ops + resize de ventana/swapchain, cerrar app → sin VUID). Fixes: `Material::RecreatePipeline` destruye el `m_UBOSetLayout` viejo antes de recrear (fuga latente de `VkDescriptorSetLayout`); el editor ahora libera todos los subtrees de contenido del dock (`DeleteUiSubtree`) en `OnShutdown` (antes `hierarchy`/`inspector`/paneles debug quedaban huérfanos y nunca se borraban).

## Fase 3 — Physics (Jolt)
- [x] Integrar Jolt Physics via CMake FetchContent
- [x] `PhysicsWorld` wrapper (step, gravity, debug draw)
- [x] `RigidBody` component (mass, gravity, kinematic/static)
- [x] `Collider` component (box, sphere, capsule, mesh)
- [x] Sync transform → Jolt → transform cada frame
- [x] Raycast / Shape overlap queries
- [ ] `CharacterController` component
- [x] Configurar Jolt multithreading
- [x] Configuración por capas (NON_MOVING/MOVING, tablas BroadPhase/ObjectLayer)

## Fase 4 — Audio (SoLoud)
- [ ] `AudioSystem` wrapper
- [ ] `AudioSource` component (3D position, pitch, volume, looping, WAV/OGG)
- [ ] `AudioListener` component (atado a Camera)
- [ ] DSP filters (echo, reverb, biquad)
- [ ] Gestión de memoria de sonidos

## Fase 5 — UI System (Propio)
- [x] `Canvas` (screen space overlay, sorting) — `UICanvas` (event hooks, hit-test, hover, focus, pointer capture, wheel→`OnScroll` dispatch)
- [x] `UIWidget` base — `UIElement` (rect/anchor/offset, Free/Row/Column layout, min size, SizePolicy)
- [x] `UILabel` (texto con fuente, vertical centering)
- [x] `UIButton` (click, hover, pressed states)
- [x] `UIImage` (textura)
- [x] `UIPanel` (contenedor)
- [x] `UIScrollView`
- [x] `UIScrollbar` (scrollbars en ScrollView — ver `TODO_UI_SCROLLBARS.md`; UITextArea scroll offset pendiente en `TODO_UI_INPUT.md` F3.1/F3.3)
- [x] Bug ScrollView arreglado (track del scrollbar no cubría el alto + drag/thumb/rueda invertidos): `SyncScrollbar` con coordenadas absolutas + contenido en `cr - scrollOffset` + drag touch-style — ver `TODO_UI_SCROLLBARS.md`
- [x] Bug consola: flash al resize de docksplitters (rebuild de líneas corría tras el layout → 1 frame culled) — `ConsolePanel::Refresh()` movido antes de `UpdateLayout()`; log "Viewport Resized" debounced (0 msgs por drag, 1 al soltar) — ver `TODO_UI_CONSOLE.md`
- [x] `UITextInput` (caret, click-to-position, drag selection, double-click word, Ctrl+A, Ctrl+arrow)
- [x] `UITextArea` (multiline: líneas lógicas, selección multi-línea, navegación Up/Down)
- [x] `UIFloatInput` (input numérico que filtra `[0-9+-.]`, commit en Enter/Blur)
- [x] `UISlider`
- [ ] `UICheckbox`
- [x] Layout system (Free/Row/Column con anchors, offsets, propagación de posición al padre)
- [ ] Style system (colores, borders, fonts, margins)
- [x] Text rendering (stb_truetype + FreeType) — `Font`
- [x] `UIRenderer` (batcher de quads, 3 capas: UI regular → viewports → debug overlay; **clipping por scissor** por nodo con `SetClip` — ver `TODO_UI_SCROLLBARS.md`)
- [x] `UIViewportPanel` (RenderTexture dentro de la UI)
- [x] `UIDebugOverlay` (FPS, frame time, DrawCalls actual+avg, memoria del proceso, mouse, teclas, hover, eventos)
- [x] Optimización UI (core): cache del tamaño natural de `UILabel` (layout O(1) por label) + batch de draw calls en `UIRenderer` (agrupa por textura+scissor, con vértices degenerados para el `TRIANGLE_STRIP`). Verificado: 60 FPS con la consola llena, ~127 drawcalls avg. Ver `TODO_UI_OPTIMIZATIONS.md`
- [x] RenderTexture (offscreen color+depth, muestreo en UI)

## Fase 6 — Editor (LeirEngineEditor)
- [x] `EditorApp` (subclase de CoreApplication)
- [ ] `HierarchyPanel` (árbol de objetos en Scene) — hoy es solo un panel estático con título
- [x] `InspectorPanel` — `InspectorTransformPanel` (transform en vivo; falta listado de componentes)
- [x] `SceneViewPanel` (viewport 3D con `UIViewportPanel` + RenderTexture + EditorCamera)
- [x] `ConsolePanel` (output de `XConsole` en tiempo real, dockeable, filtros Info/Warn/Error, Clear, auto-follow — ver `TODO_UI_CONSOLE.md`)
- [x] Bug UIEvent flood arreglado (consola se scrolleaba sola + glitches + FPS 60→20): UIEvent→Trace + overflow no destructivo en `Flush()` — ver `TODO_UI_EVENT_FLOOD.md`
- [ ] `ProjectPanel` (explorador de archivos)
- [ ] Gizmos 3D (translate, rotate, scale handles)
- [ ] Selección de objetos con raycast
- [ ] Drag & drop en hierarchy
- [x] `EditorCamera` (free-fly: right-click yaw/pitch, middle-click pan, WASDQE, Shift×3; sync bidireccional con cámara de escena)
- [x] Splitters redimensionables Hierarchy|Viewport|Inspector (`UISplitter`, drag invertido, cursor ResizeEW)
- [x] Persistencia: layout de paneles + ventana (tamaño/posición/maximized) en `settings.json` (guardado al soltar y al cerrar)
- [x] Paneles debug: `UITestPanel`, `CameraTestPanel`, `DebugTextPanel`, `TextAreaDebugPanel` (capa debug overlay)
- [x] `UIDragFloatInput` (drag-to-change sobre label con pointer capture)

## Fase 7 — Serialización & Assets
- [ ] `Scene` serialization a JSON (nlohmann_json)
- [ ] `Scene` serialization binario (cereal)
- [ ] `AssetImporter` (.obj, .fbx, image, audio)
- [ ] `ResourceManager` (caché, reference counting, hot-reload)
- [ ] Editor: Save Scene / Load Scene
- [ ] Editor: drag & drop assets al proyecto

## Fase 8 — Animaciones
- [ ] `AnimationCurve`
- [ ] `Bone` + `Skeleton`
- [ ] `SkinnedMeshRenderer`
- [ ] `AnimationController` (state machine básico)
- [ ] `Animator` component

## Fase 9 — Documentación
- [ ] Configurar Doxygen (Doxyfile)
- [ ] Comentarios API en todos los headers públicos
- [ ] Páginas markdown: architecture overview, getting started, class hierarchy
- [ ] Generar HTML automático con el build
- [ ] Hostear documentación (GitHub Pages o local)

## Extras (futuro, sin prioridad)
- [ ] Scripting con C# (Mono embedding o .NET host)
- [ ] Networking (ENet / RakNet)
- [ ] Particle system
- [ ] Post-processing (bloom, DOF, HDR, tonemapping)
- [ ] Terrain system
- [ ] NavMesh / pathfinding
- [ ] Android/iOS port
- [ ] Editor dark theme UI
