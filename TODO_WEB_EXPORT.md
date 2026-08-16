# TODO_WEB_EXPORT.md — Fase 6: Export Web (Emscripten + WebGPU en navegador)

> Estado: **EN CURSO** — M0 ✅ (2026-08-15), **M1 ✅ verificado en navegador
> (Firefox 153 + Chrome + Opera, 2026-08-15)**, **M2 ✅ verificado en navegador
> (Firefox, 2026-08-16)**, pendiente M3+.

## Objetivo

Portar el motor completo (rendering + física + audio + UI + input) a WebAssembly para
navegador, renderizando por **WebGPU** en el navegador, y empaquetar un **demo completo**
(escena 3D + física + audio + UI) en HTML5.

Decisiones de alcance (usuario, 2026-08-15):
- Demo web = **completo (física + audio también)** — implica implementar **Fase 4 Audio**
  (hoy vacío) primero en desktop y luego en wasm.
- Emscripten = **clon fresco de 6.0.6** (`C:\programs_dev\emsdk6`).
- Shaders = **`--preload-file`** a la ruta virtual (`/shaders`).

## Contexto técnico (hallazgos verificados)

### Toolchain
- Emscripten **6.0.6** instalado en `C:\programs_dev\emsdk6` (clon fresco, tag `6.0.6`).
  Python 3.13.3 + node 24.19.0 embebidos por el propio emsdk.
- Activación por sesión: `call C:\programs_dev\emsdk6\emsdk_env.bat`.
- Ports disponibles: `--use-port=emdawnwebgpu` y `--use-port=contrib.glfw3` (verificado).

### emdawnwebgpu (Dawn para Emscripten) — reemplaza `-sUSE_WEBGPU` (deprecado)
- Provee `<webgpu/webgpu.h>` estándar (webgpu-native). También `webgpu_cpp.h`.
- **Base API idéntica a wgpu-native v29**: `wgpuCreateInstance`,
  `wgpuInstanceRequestAdapter/RequestDevice` (→ `WGPUFuture`),
  `wgpuInstanceWaitAny(instance, count, futures, timeoutNS)` → `WGPUWaitStatus`,
  `wgpuInstanceCreateSurface(instance, desc)` → `WGPUSurface` (retorno directo, OLD),
  `wgpuSurfaceGetCapabilities` → `WGPUStatus`, `wgpuQueueSubmit(queue, n, cmds)`.
  El diff desktop↔web es SOLO en extensiones nativas (no existentes en web).
- `wgpuGetProcAddress` **aborta** en web → los símbolos se linkean estáticamente.
- `emwgpuWaitAny` (glue de `wgpuInstanceWaitAny`) requiere **`-sASYNCIFY=1`**
  (`#if ASYNCIFY`, aborta sin él) → el init síncrono adapter/device usa ASYNCIFY.
  (El Chrome-doc oficial usa exactamente este patrón: `CallbackMode::WaitAnyOnly` +
  `WaitAny(f, UINT64_MAX)`.)
- `emscripten_webgpu_get_device()` existe pero es deprecado y depende de
  `Module['preinitializedWebGPUDevice']` (no se preinicializa solo) → **NO usar**.
- **LIMITACIÓN CRÍTICA**: emdawnwebgpu NO soporta arrays de texturas/samplers en bind
  groups (`library_webgpu.js:1540-1546`: `bindingArraySize` "not specced, not implemented",
  assert `bindingArraySize ≤ 1`; `makeEntry` liga UN solo recurso por entrada).
  El motor usa arquitectura **bindless** (`binding_array<texture_2d<f32>, 16>` en WGSL,
  ver `engine/shaders/*.wgsl`) → la glue C no puede ligar las 16 views + 16 samplers.
  - Solución aplicada: **vendor + parchear emdawnwebgpu** — añadir
    `WGPUSType_BindGroupEntryExtras` + `WGPUBindGroupEntryExtras`
    (`{chain, buffers, bufferCount, samplers, samplerCount, textureViews, textureViewCount}`)
    a `webgpu.h` y a `library_webgpu.js::makeEntry` (recurso = array), MIRRORING wgpu-native
    `<webgpu/wgpu.h>` para que el C++ del backend sea 100% compartido, y
    `wgpuDeviceCreateBindGroupLayout` pasa `entry.bindingArraySize` (offset 16, u32) al
    dict JS — el navegador **NO infiere** el tamaño del array desde el WGSL.
- **Descubierto en verificación (navegador, 2026-08-15)** — lo que el plan original no
  previó:
  - **Firefox (naga web) NO puede compilar `binding_array` en absoluto**: el parser exige
    `enable wgpu_binding_array;` y esa extensión es **native-only** en naga
    (`naga/src/front/wgsl/parse/directive/enable_extension.rs` → `WgpuBindingArray`
    native only; el enable en web = "the `wgpu_binding_array` extension is not supported
    in the current environment"). Conclusión: en **web el bindless por arrays es
    imposible** → el backend degrada la tabla compartida a **recurso único** y los
    shaders web usan variantes `*.web.wgsl` (ver "Verificación M1 en navegador").
- Port local: `--use-port=<pkg>/emdawnwebgpu.port.py` (el port.py resuelve `_pkg_dir`
  relativo al archivo). Paquete ~861 KB → se vendea en `dependencies/emdawnwebgpu_pkg/`.

### contrib.glfw3 (emscripten-glfw, GLFW 3.4 completo en C++)
- GLFW oficial NO tiene plataforma Emscripten. `contrib.glfw3` = port C++ de pongasoft.
- El motor usa GLFW SOLO en 5 archivos: `CoreApplication.cpp` (25 funciones), `InputManager.cpp`
  (8), y backends nativos (excluidos en web: `glfwGetWin32Window`/`glfwGetCurrentContext`).
  NO usa `glfwSwapBuffers`/`glfwMakeContextCurrent`/`glfwGetKey`/`glfwSetInputMode`.
  Semántica de monitor/maximize/pos = stubs razonables en web.
- **Peligro**: `#define GLFW_INCLUDE_VULKAN` en `CoreApplication.cpp:6` y `InputManager.cpp:8`
  arrastra `<vulkan/vulkan.h>` a TUs del core → gatear con `#if !defined(__EMSCRIPTEN__)`.
- `CoreApplication::Run()` usa `glfwPollEvents()` + bucle `while (!glfwWindowShouldClose)`.
  En web hay que convertir el loop a `emscripten_set_main_loop` (o mantener el while con
  `emscripten_sleep`/`-sASYNCIFY`). Decisión M2.

### Vulkan → exclusión mínima en web
- Solo `VulkanDevice.cpp` + `VulkanBackend.cpp` + `find_package(Vulkan REQUIRED)`.
  Toda la capa render/UI/asset usa `RHI::RenderBackend*` (auditado, sin fugas).
- **BackendFactory vive en `VulkanBackend.cpp`** → extraer a `RHI/BackendFactory.cpp` neutro
  (M1) para que el build web tenga dispatch `"webgpu"`.

### Física (M3)
- `PhysicsWorld.cpp:13,40-45`: swap `JobSystemThreadPool` → `JobSystemSingleThreaded` bajo
  `__EMSCRIPTEN__` (sin pthreads → sin SharedArrayBuffer/COOP-COEP → cualquier server estático).
- Jolt a wasm: flags `JPH_CPU_ARCH`/SIMD a resolver en el build.

### File IO en web (M2)
- `Settings.cpp` (config dir + `create_directories`) → no-op por `#ifdef __EMSCRIPTEN__`.
- `LEIR_SHADER_DIR` es ruta host absoluta → en web definir `/shaders` (virtual) +
  `--preload-file <host shaders>@/shaders`. `ShaderLayout` necesita `.reflect.json` sidecars
  → **los 6 sidecars están commiteados** en `engine/shaders/` (cargados en web vía el fix de
  `SidecarPathFor` para `.web.wgsl`, M2/D).
- Font: `Font.cpp` lee TTF con `fopen` → en web el FS virtual (preload-file) funciona tal cual;
  el demo usa `/assets/Roboto-Regular.ttf` (commiteado en `examples/WebEngineDemo/assets/`).
- `UIDebugOverlay.cpp:53` lee `/proc/self/status` (falla limpio en web).

## Plan de fases

- [ ] **M0 — Toolchain** — emsdk 6.0.6 clon fresco + install/activate + validar ports. ✅ (2026-08-15)
- [ ] **M1 — WebGPUBackend web + render mínimo** (de-risking)
  - [x] Vendor + parchear emdawnwebgpu (arrays en bind groups) — `dependencies/emdawnwebgpu/`
  - [x] `BackendFactory` → `RHI/BackendFactory.cpp` neutro
  - [x] Split `#if __EMSCRIPTEN__` en `WebGPUBackend.cpp` (device vía WaitAny+ASYNCIFY,
        superficie canvas `WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector`, sin native ext,
        bindless table pequeña)
  - [x] Sidecars `.reflect.json` en `engine/shaders/` (copiados; commit pendiente) + shaders
        WGSL web ya en `engine/shaders/`
  - [x] `examples/WebDemo` mínimo (cubo + cámara, RHI directo) — standalone CMake, sin shell
        custom (el default de emcc ya tiene `#canvas`)
  - [x] Preset CMake `emscripten` (standalone, `examples/WebDemo/CMakePresets.json`) +
        preload-file `engine/shaders@/shaders`
  - [x] **Verificar render en navegador** — ✅ Firefox 153 + Chrome + Opera (2026-08-15),
        cubo checker rotando, log limpio

## Estado M1 (build, 2026-08-15)

- **Patches emdawnwebgpu** (`dependencies/emdawnwebgpu/`):
  - `webgpu.h` vendado: `WGPUSType_BindGroupEntryExtras = 0x00030005`,
    `WGPUSType_BindGroupLayoutEntryExtras = 0x00030006`, structs
    `WGPUBindGroupEntryExtras {chain, buffers, bufferCount, samplers, samplerCount,
    textureViews, textureViewCount}` + `WGPUBindGroupLayoutEntryExtras {chain, count}`
    + macros INIT (offsets wasm32 verificados: chain=0, buffers=8, bufferCount=12,
    samplers=16, samplerCount=20, textureViews=24, textureViewCount=28, size 32).
  - `library_webgpu.js`: `wgpuDeviceCreateBindGroup` → `makeEntry` lee los extras
    (sType 0x00030005) por walk manual de la chain y liga el recurso como **array**
    (textureViews o samplers; la rama de buffers se quitó); assert de
    `bindingArraySize` relajado (`==0 || >=1`); `iterateExtensions` salta sTypes sin
    handler (antes crasheaba).
- **`BackendFactory.cpp`** (nuevo): dispatch `"d3d12"/"webgpu"/"vulkan"` + guards por
  plataforma (`__EMSCRIPTEN__` → webgpu; vulkan `nullptr`). `LEIR_BACKEND_WEBGPU 3`
  en `RHI.h`. El archivo se añadió al `engine/CMakeLists.txt` y se compila también en
  el WebDemo.
- **Split `WebGPUBackend.cpp`** (guard `(WIN32&&MSVC) || __EMSCRIPTEN__`):
  - Ctor web: los ~57 proc pointers se asignan a símbolos directos (emdawn),
    `DevicePoll=nullptr`; `FreeLibrary`/`LoadProc`/`hwnd` quedan desktop-only.
  - `Init()` web: instancia plana + surface `#canvas`
    (`WGPUEmscriptenSurfaceSourceCanvasHTMLSelector`), adapter/device por
    `WaitAnyOnly` + `WaitAny(UINT64_MAX)` (necesita `-sASYNCIFY=1`), **sin
    requiredFeatures/requiredLimits** (Chrome habilita binding arrays por defecto;
    los límites default 16/16 coinciden con `kBindlessMax`; la tabla
    `WebGPU.FeatureName` de la glue no mapea la feature nativa → pedirla daría
    `[undefined]`).
  - `WaitIdle` web = no-op (wgpuDevicePoll no existe en emdawn).
  - `glfwGetFramebufferSize` (ctor y `RecreateSwapchainInternal`) usa la ventana GLFW
    real → el WebDemo debe crear la ventana y pasarla al backend.
- **`examples/WebDemo`** (standalone, RHI directo, sin librería engine):
  - Compila `WebGPUBackend.cpp` + `BackendFactory.cpp` + `Core/Log.cpp` en
    `WebDemoCore` (static) + `main.cpp`.
  - Renderiza un **cubo checker rotando con cámara auto-orbit** usando
    `Basic.vert/frag.wgsl` (precargados en `/shaders`): UBO set0 (viewProjection),
    bindless set1 (slot 0 = textura checker 2×2 sRGB registrada), push emulado set2
    (grupo = nº de setLayouts), pass-less graph en el main pass de la swapchain
    (`BeginFrame(false)` + `CmdExecuteGraph` + `EndFrame`).
  - `emscripten_set_main_loop_arg` como loop; GLFW solo para crear la ventana
    (`GLFW_CLIENT_API=NO_API`) → la superficie WebGPU es el `#canvas` que crea el port.
  - **Preset `emscripten`** en `examples/WebDemo/CMakePresets.json` (toolchain de emsdk,
    Ninja, `-sASYNCIFY=1`, `--use-port=emdawnwebgpu.port.py` + `contrib.glfw3`,
    `-sALLOW_MEMORY_GROWTH=1`, `--preload-file engine/shaders@/shaders`).
- **Build verificado (2026-08-15)**: `cmake -S examples/WebDemo --preset emscripten` +
  `cmake --build examples/WebDemo/build/emscripten-webdemo` → `LeirEngineWebDemo.js`
  (391 KB) + `.wasm` (9.2 MB, -O0) + `.data` (13,933 B = los 18 ficheros de
  `engine/shaders`). Glue emdawn presente (`wgpuInstanceRequestAdapter`,
  `wgpuDeviceCreateBindGroup`, `emwgpuWaitAny`, `CanvasHTMLSelector`,
  `WebGPU.FeatureName`); sin macros plantilla residuales (`{{{`/`#if`).
- **Verificado en navegador (2026-08-15)**: servido con `python -m http.server` y
  **Firefox 153 / Chrome / Opera** renderizan el **cubo checker rotando en 3D** con
  cámara auto-orbit. Log de consola limpio (solo `favicon.ico` 404, inocuo). Artefactos
  finales: `.wasm` 10.39 MB, `.js` 426 KB, `.data` 15,209 B.
- [ ] **M2 — Motor completo a wasm** (static lib, GLFW port, CoreApplication loop, Settings no-op, input)
  - [x] **Fase A** — multi-textura web: per-texture bind groups + push-slot pool (ver "Estado M2")
  - [x] **Fase B** — `LeirEngineCore` static lib web-safe (`engine/CMakeLists.web.txt`, 45 sources) + `PhysicsWorld.web.cpp`
  - [x] **Fase C** — `CoreApplication::Run()` → `emscripten_set_main_loop_arg` (Frame/FrameThunk)
  - [x] **Fase D** — shaders `*.web.wgsl` (6) + sidecars web + `WebEngineDemo` (Scene completa + UI + Font)
  - [x] **Verificar render en navegador** — ✅ Firefox (2026-08-16), 2 cubos checker + cámara órbita + UI + fuente Roboto
  - [x] **Regresión M1** — rebuild WebDemo tras el rename `Basic.frag.web.wgsl` ✅
- [ ] **M3 — Física** (Jolt wasm + JobSystemSingleThreaded)
- [ ] **M4 — Fase 4 Audio** (desktop primero, luego wasm WebAudio)
- [ ] **M5 — CI** (job ubuntu setup-emsdk 6.0.6, compile-only)
- [ ] **M6 — Docs + commit + tag**

## Verificación M1 en navegador — bugs encontrados y fixes (2026-08-15)

La primera prueba en Firefox dio **pantalla negra** con tres errores en cascada. Se
resolvieron en orden:

1. **Shader `binding_array` + pipeline inválido + `createBindGroup: Missing required
   'buffer' member of GPUBufferBinding`** (cascada):
   - El error de bind group era **consecuencia del shader roto**: Firefox devuelve el
     pipeline aunque el módulo tenga errores de compilación (el error real se reporta
     como uncaptured), así que el demo seguía y creaba bind groups; el texto del error
     ("Missing required 'buffer'") venía de la WebIDL union al intentar convertir un
     resource de array a `GPUBufferBinding`. Desapareció solo al arreglar el shader —
     **no era un bug de la glue**.
   - Primer intento fallido: prepend de `enable wgpu_binding_array;\n` en
     `WebGPUBackend::CreateShaderModule` (web-only) → **rompe Firefox** ("the
     `wgpu_binding_array` extension is not supported in the current environment",
     naga native-only) → **revertido**.
2. **Solución definitiva al shader (web no-bindless)** — naga web no puede compilar
   `binding_array` en absoluto:
   - `engine/shaders/Basic.web.frag.wgsl` (nuevo): textura/sampler **únicos** en
     `@group(1) @binding(0/1)` (sin `binding_array`); el vert `Basic.vert.wgsl` ya no
     usaba arrays → compila igual en Firefox.
   - `WebGPUBackend.cpp` bajo `__EMSCRIPTEN__`: el **layout bindless compartido** se
     crea **sin** `bindingArraySize`/extras (2 entradas: texture + sampler de recurso
     único), y `RebuildBindlessBindGroup` liga la **textura+sampler del slot más bajo
     registrado** (o el dummy) en vez de los arrays de 16. En nativos (wgpu-native/
     Vulkan/D3D12) el código array queda intacto (`#if !defined(__EMSCRIPTEN__)`).
   - `examples/WebDemo/main.cpp`: carga `Basic.web.frag.wgsl` cuando `__EMSCRIPTEN__`
     (el vert se comparte). El push `textureIndex` queda inerte en web (el shader web
     no indexa).
3. **Stencil ops en `Depth32Float`** — WebGPU valida:
   "Stencil `LoadOp`/`StoreOp` ... must be `None` for attachments (`Depth32Float`)
   without stencil aspect". Se **quitaron** `depth.stencilLoadOp`/`stencilStoreOp`
   (quedan en 0 = `Undefined` → la glue pasa `None`) en el pass principal de
   `BeginFrame` y en `CmdBeginRenderPass` (ambos usan `Depth32Float`). El error
   impedía el `CmdBeginRenderPass` → sin clear ni draw.
4. **`bindingArraySize` en la glue del layout** (fix correcto, se mantiene): el navegador
   NO infiere el tamaño del array desde el WGSL → `wgpuDeviceCreateBindGroupLayout`
   (`library_webgpu.js`) lee `entry.bindingArraySize` (offset 16) y lo emite al dict JS.
   En el camino web actual (recurso único) no se usa; se necesita si algún día web usa
   arrays (Chrome los acepta vía Tint; solo naga/Firefox los rechaza).
5. **Diagnóstico temporal retirado**: durante el debug se añadió try/catch + summary en
   `wgpuDeviceCreateBindGroup` (`[LeirBG] createBindGroup FAILED label=... entries=[...]`);
   se eliminó tras la verificación (relink limpio, 0 ocurrencias en el JS).

Lección: **naga web no soporta `binding_array`** (ni con ni sin enable) → en el navegador
la tabla bindless se degrada a recurso único y los shaders web usan variantes `*.web.wgsl`.
Chrome/Opera (Tint) sí aceptarían `binding_array` core, pero se priorizó la variante única
para que funcione en los tres navegadores.

## Verificación M1 (objetivo) — ✅ CUMPLIDO (2026-08-15)

- `emcmake cmake` + build del WebDemo → `WebDemo.html/.js/.wasm` + `webgpu` preload.
- Servir con `python -m http.server` y abrir en **Firefox / Chrome / Opera** → cubo
  iluminado (checker 2×2 sRGB) con cámara orbitando. Consola sin errores.
- Parity visual contra el desktop WebGPU (wgpu-native) no aplica pixel-a-pixel en navegador;
  validado que el render (cubo + clear + textura) se ve correcto.

## Estado M2 (build + navegador, 2026-08-16)

Motor completo (Scene/Object3D/Camera/Light/MeshRenderer/Material/Texture2D/RenderTexture/
RenderPipeline/UICanvas/UIRenderer/Font/Input) corriendo en navegador por WebGPU. Build:
`examples/WebEngineDemo` standalone (preset `emscripten`, binaryDir
`build/emscripten-webengine`) → `WebEngineDemo.html/.js/.wasm/.data`.

- **Fase A — multi-textura web** (WebGPUBackend.cpp): la web no puede usar `binding_array`
  (naga), así que el backend degrada la tabla bindless compartida a **recurso único** y los
  draws ligan su textura con **bind groups per-texture**:
  - `Impl::textureBindGroups` (cache) + `GetTextureBindGroup(index)` (mismo `bindlessLayout`:
    binding 0 = texture, binding 1 = sampler; fallback dummy).
  - `CmdBindDescriptorSets` web marca `im.bindlessSetSlot = slot` (no bindless no toca);
  - el executor (`CmdExecuteGraph`, `__EMSCRIPTEN__`) resetea `bindlessSetSlot=-1`/`pushSlot=0`
    al inicio y, **por draw**, re-liga el slot bindless con
    `GetTextureBindGroup(rec.draw.sampledTextures[0])` (o dummy).
  - **push slot pool**: `CmdPushConstants` crea un UBO de push por draw (`lr->pushBuffers`/
    `pushBindGroups` + `im.pushSlot++`) — cada draw lee su propio bloque (sin last-write-wins).
    Tamaño = `max(pushSize,16)` alineado a 16. `Material::Bind` liga set1 (bindless);
    `RenderPipeline::RenderMeshRenderer` liga set0 (UBO) via shadow `MapMemory`/`UnmapMemory`
    (QueueWriteChunked). Validado en navegador: **3 cubos** (gris/rojo/azul) en el M1, cada
    uno con su propia textura, mismo pipeline.
- **Fase B — `LeirEngineCore` static lib** (`engine/CMakeLists.web.txt`, nuevo): 45 sources
  web-safe (Core, Scene, Objects, Input, RHI WebGPU, Rendering, Components, UI + Dock, Physics
  stub). Include PUBLIC `engine/include` + glm; PRIVATE `engine/src`, stb, emdawnwebgpu.
  Define PUBLIC `LEIR_SHADER_DIR="/shaders"`. Link PUBLIC nlohmann_json. Compile
  `-sASYNCIFY=1 --use-port=contrib.glfw3`. `CXX_VISIBILITY_PRESET hidden`. Robust a
  `include()` vía `LEIR_ROOT = CMAKE_CURRENT_LIST_DIR/..`.
  - **`engine/src/Physics/PhysicsWorld.web.cpp`** (nuevo): stub sin Jolt — misma interfaz,
    `StepPhysics`/`Init`/`Shutdown` no-op; `GetBodyInterface`/`GetPhysicsSystem` devuelven
    refs a punteros null (nunca llamadas en web; refs a tipos incompletos JPH = legal).
    El header `PhysicsWorld.h` forward-declara JPH (no incluye Jolt) → el build web es
    **100% libre de Jolt**.
- **Fase C — main loop web** (`CoreApplication.h/.cpp`): `Run()` bajo `__EMSCRIPTEN__` →
  `m_LastFrameTime = glfwGetTime()` + `emscripten_set_main_loop_arg(&FrameThunk, this, 0, true)`
  (infinite loop; `Run()` nunca retorna en web). `Frame(double)` espeja el bucle desktop:
  `glfwPollEvents` → `EventQueue::Process` → `scene->OnUpdate` → `OnUpdate(deltaTime)` →
  `InputManager::Update()` → `OnRender`. Nuevos `Frame(double)`/`FrameThunk(void*)`/
  `m_LastFrameTime`. `Settings` verificado web-safe sin cambios (branch `#else`: HOME=`/` del
  FS virtual, try/catch protege el fopen). Guards `#if !defined(__EMSCRIPTEN__)` en los
  `#define GLFW_INCLUDE_VULKAN` de `CoreApplication.cpp`/`InputManager.cpp`.
- **Fase D — shaders web + sidecars + demo**:
  - `engine/shaders/*.web.wgsl` (6): `Basic.vert/frag`, `Sprite.vert/frag`, `UI.vert/frag`
    (textura/sampler **únicos**; `Basic.frag.web.wgsl` **renombrado** desde el viejo
    `Basic.web.frag.wgsl`). `Sprite.frag.web.wgsl` y `UI.frag.web.wgsl` son nuevos (single
    texture; `UI.frag` sin `fragTexIndex`).
  - `WebGPUBackend.h::GetShaderFileExtension()` → `".web.wgsl"` bajo `__EMSCRIPTEN__` → el
    motor carga las variantes web automáticamente.
  - `ShaderLayout.cpp::SidecarPathFor` ahora recorta `.web.wgsl`/`.dxil`/`.spv` → los **6
    sidecars `.reflect.json` commiteados** cargan en web y dan los push sizes exactos
    (Basic 144, Sprite 112, UI 8). **Crítico**: `sizeof(PushConstants)` C++ = 132 < 144 del
    shader std430 → sin sidecar el push UBO web quedaría corto (validation error).
  - **`examples/WebEngineDemo/main.cpp`** (nuevo): subclase de `CoreApplication`, backend
    `"webgpu"`, Scene con Camera orbital (matemática EditorCamera: `Euler(pitch,yaw,0)`,
    pos = `dist·(cos·sin, -sin, cos·cos)` → mira al origen), Light directional, 2 cubos con
    **checkers 256×256 distintos** (gris y rojo) rotando, `RenderTexture` fullscreen−30
    (físico = lógico×dpr) + `RenderPipeline::Render`, `UIViewportPanel` (Stretch, offset
    `{0,0,0,-30}`) + barra inferior con `UILabel` (el texto va FUERA del viewport: la capa UI
    normal dibuja debajo de los viewports), `Font` desde `/assets/Roboto-Regular.ttf`
    (preload). `OnRender` = `BeginFrame(true)` → graph del RT → `CmdExecuteGraph` →
    `BeginSwapchainOverlay` → graph UI → `EndFrame`.
  - CMake del demo: `add_executable(WebEngineDemo)` linka `LeirEngineCore` + `--use-port=
    ${LEIR_ROOT}/dependencies/emdawnwebgpu/emdawnwebgpu.port.py` **en el LINK** (provee la
    glue JS de wgpu → sin él, undefined symbols `wgpuCreateInstance`/`wgpuInstanceRequestAdapter`
    …), `contrib.glfw3`, `-sALLOW_MEMORY_GROWTH=1`, preload `engine/shaders@/shaders` y
    `assets@/assets`. El include de emdawnwebgpu es PRIVATE en la lib; el exe no lo necesita
    (no incluye `webgpu.h` directamente).
  - Fuente: **Roboto-Regular.ttf** (Apache-2.0, release `googlefonts/roboto` v2.138) commiteada
    en `examples/WebEngineDemo/assets/` (el repo no tiene TTF compilados; `roboto-3-classic`
    y `google/fonts` ya no los alojan).
- **Verificado en navegador (2026-08-16)**: Firefox — 2 cubos checker nítidos (256×256)
  girando + cámara en órbita, barra inferior con título en **texto Roboto**, y **los logs del
  motor (XConsole) visibles en la consola del navegador**. Artefactos: `.wasm` 22.5 MB,
  `.js` 412 KB, `.data` 369,582 B (shaders + sidecars + fuente). Servidor de prueba:
  `python -m http.server 8001` desde `examples/WebEngineDemo/build/emscripten-webengine`.
- **Regresión M1 (2026-08-16)**: rebuild del WebDemo tras el rename `Basic.frag.web.wgsl`
  (main.cpp ya apuntaba a `/shaders/Basic.frag.web.wgsl`) → link OK, render OK (3 cubos
  gris/rojo/azul con sus checkers 2×2 propios + órbita). El aspecto de "4 cuadrados por cara
  difuminados" es el **diseño M1** (texturas 2×2 con sampler Linear), no una regresión.

## Riesgos abiertos
- ASYNCIFY overhead (global). Aceptable para demo; alternativas a revisar si duele.
- Feature `texture-array-non-uniform-indexing` (UI.frag indexa no-uniforme): **resuelto en
  M1 degradando a recurso único en web**; el UI real (M2) deberá decidir entre variantes
  `*.web.wgsl` por shader o un pipeline UI sin non-uniform indexing.
- `contrib.glfw3` cobertura de `glfwGetMonitorWorkarea`/`glfwCreateStandardCursor` (M2).
- Jolt/SoLoud bajo Emscripten (M3/M4).
- Tamaño wasm + `ALLOW_MEMORY_GROWTH` + preload de fuentes/audio.