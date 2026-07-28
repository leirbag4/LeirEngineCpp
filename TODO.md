# LeirEngine — TODO List

## Fase 0 — Setup Inicial
- [x] Crear estructura de directorios del proyecto
- [x] `dependencies/CMakeLists.txt` con FetchContent (GLFW, GLM, Vulkan, stb, Jolt, SoLoud, spdlog, nlohmann_json, cereal)
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
- [x] Logging con spdlog integrado

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

## Fase 3 — Physics (Jolt)
- [x] Integrar Jolt Physics via CMake FetchContent
- [x] `PhysicsWorld` wrapper (step, gravity, debug draw)
- [x] `RigidBody` component (mass, gravity, kinematic/static)
- [x] `Collider` component (box, sphere, capsule, mesh)
- [x] Sync transform → Jolt → transform cada frame
- [x] Raycast / Shape overlap queries
- [ ] `CharacterController` component
- [x] Configurar Jolt multithreading

## Fase 4 — Audio (SoLoud)
- [ ] `AudioSystem` wrapper
- [ ] `AudioSource` component (3D position, pitch, volume, looping, WAV/OGG)
- [ ] `AudioListener` component (atado a Camera)
- [ ] DSP filters (echo, reverb, biquad)
- [ ] Gestión de memoria de sonidos

## Fase 5 — UI System (Propio)
- [ ] `Canvas` (screen space overlay, sorting)
- [ ] `UIWidget` base (rect, transform, padding, color, visible)
- [ ] `UILabel` (texto con fuente)
- [ ] `UIButton` (click, hover, pressed states)
- [ ] `UIImage` (textura)
- [ ] `UIPanel` (contenedor)
- [ ] `UIScrollbar`, `UIScrollView`
- [ ] `UITextInput`, `UISlider`, `UICheckbox`
- [ ] Layout system (vertical, horizontal, absolute)
- [ ] Style system (colores, borders, fonts, margins)
- [ ] Text rendering (stb_truetype + FreeType)

## Fase 6 — Editor (LeirEngineEditor)
- [ ] `EditorApp` (Application subclass)
- [ ] `HierarchyPanel` (árbol de objetos en Scene)
- [ ] `InspectorPanel` (propiedades del objeto + sus componentes)
- [ ] `SceneViewPanel` (viewport 3D con EditorCamera)
- [ ] `ConsolePanel` (output de spdlog en tiempo real)
- [ ] `ProjectPanel` (explorador de archivos)
- [ ] Gizmos 3D (translate, rotate, scale handles)
- [ ] Selección de objetos con raycast
- [ ] Drag & drop en hierarchy
- [ ] EditorCamera (orbit alt+click, pan, zoom scroll)

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
