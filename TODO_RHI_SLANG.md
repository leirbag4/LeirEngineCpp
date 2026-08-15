# TODO_RHI_SLANG.md — RHI propia + Slang (multiplataforma)

Documento de decisión y plan. Fecha: 2026-08-10.

## 1. Decisión y contexto

- **Objetivo**: editor en Windows/macOS/Linux; runtime del motor en Windows, macOS, Linux,
  Android, iOS y Web (WebGPU, con fallback WebGL2).
- **Shaders**: **Slang** como fuente única. Slang es un **superconjunto de HLSL**. Los shaders se
  escriben en un **subconjunto HLSL-vanilla** (HLSL es el lenguaje de la industria 3D; GLSL quedó
  obsoleto), usando **solo** las adiciones de Slang imprescindibles para la compilación
  multi-backend. Objetivo: si a futuro se cambia Slang por HLSL+DXC puro, **no reescribir los
  shaders** — solo la capa de compilación (ya aislada en `IShaderCompiler`). Ver §2.1b.
- **Integración**: **libslang linkeada en el editor** (compilar shaders al vuelo vía
  `IShaderCompiler`); `slangc` como tool en CMake para el build dev del engine. **Runtime sin
  saber nada de Slang** — carga solo el binario precompilado.
- **RHI**: **propia y desacoplada** de cualquier tecnología concreta. Hoy se implementa sobre
  Slang, pero el diseño aísla la compilación de shaders detrás de una interfaz nuestra para
  poder migrar de tecnología si Slang desaparece o algo mejor aparece (Sección 6). Inspirada en
  el modelo WebGPU (bind groups / render pass / pipeline objects) pero **no una copia** — es un
  diseño original pensado para superarla (Sección 3).

## 2. Pipeline de shaders

### 2.1 Fuente única
Todo shader vive como `.slang` en `engine/shaders/`. El editor y el motor dev compilan desde esa
fuente. Fuentes actuales a migrar: `Basic`, `Sprite`, `UI` (`.vert`/`.frag`).

### 2.1b Directiva "HLSL-vanilla" — regla de estilo para shaders (decisión 2026-08-12)

Slang es un superconjunto de HLSL: acepta HLSL vanilla y le añade features propias. Para
maximizar portabilidad futura (cambio a HLSL+DXC puro sin reescribir) y usar el lenguaje que
domina la industria 3D, **los shaders se escriben en un subconjunto HLSL-vanilla**. Se usa
**Enfoque A**: atributos `vk` directos en el shader (compartidos con DXC), no registers + mapeo.

**Permitido — HLSL vanilla (el cuerpo del shader):**
- Tipos: `float`, `float2/3/4`, `float4x4`, `int`, `uint`, `bool`.
- `cbuffer`, structs, `mul()`, `normalize()`, `dot()`, `max/min/clamp`, operadores.
- `Sampler2D.Sample(uv)` (combined image sampler).
- Semánticas `SV_Position` / `SV_Target` y etiquetas de entrada `: POSITION0/TEXCOORD0/COLOR0`.
- Structs de entrada/salida con `[[vk::location(N)]]`.

**Permitido — atributos de stage/binding estándar (punto de convergencia Slang↔DXC):**
- `[shader("vertex"/"fragment")]` — entrada de stage.
- `[[vk::binding(a, b)]]` — binding `a`, set `b`.
- `[[vk::location(N)]]` — ubicación de atributo/salida.
- `[[vk::push_constant]]` — push constant.

Estos atributos son los **mismos que usa DXC con `-fvk`**, así que el mismo shader compila en
Slang (multi-target) y en DXC (SPIR-V). Son "HLSL + atributos de binding estándar", no features
exóticas de Slang.

**Prohibido — features exclusivas de Slang sin equivalente en HLSL/DXC** (solo si un shader lo
exige y se documenta): generics `<T>`, `interface`/`extension`, `var`/`let`, `__intrinsics`,
`This`, matrices/funciones propias de Slang, dynamic dispatch. Si un shader necesita una de
estas, se marca con comentario `// SLANG-ONLY: <feature>` y se justifica en este doc.

**Ejemplo mínimo permitido:**
```hlsl
[shader("vertex")]
VSOutput main(VSInput input) {              // cuerpo 100% HLSL vanilla
    VSOutput o; o.position = mul(push.mvp, float4(input.inPosition, 0.0, 1.0)); return o;
}
```
**Prohibido:** `float3 tint<T>(T x)` (generic), `interface ILight { float3 eval(); }` (interface),
`let v = ...` (Slang `let`), `Texture2D<float4>` especializaciones, etc.

Los 6 shaders migrados en el spike ya cumplen esta directiva (~95% HLSL vanilla; lo único de
Slang son los atributos de stage/binding/push_constant).

### 2.2 `IShaderCompiler` — interfaz pública (desacople clave)
```
engine/include/LeirEngine/Rendering/ShaderCompiler.h
class IShaderCompiler {
    virtual CompileResult Compile(const ShaderSource& src, ShaderTarget target,
                                  const ShaderCompileOptions& opts) = 0;
    virtual std::vector<ShaderTarget> GetSupportedTargets() const = 0;
    virtual ShaderReflection Reflect(const ShaderSource& src, ShaderTarget target) = 0;
};
```
- Interfaz en el **engine**, implementación en el **editor** (`SlangShaderCompiler` usa libslang).
- El motor **no conoce `slang`** en ningún header público. Solo conoce `IShaderCompiler`.
- **Migración futura**: si Slang desaparece, se escribe `OtherShaderCompiler : IShaderCompiler`
  y nada más cambia.

### 2.2b `IGlobalSession` / `ISession` — ciclo de vida y amortización

La integración usa la API C++ de Slang, **no** `slangc` por CLI. La
recomendación oficial del proyecto ([discusión #9354](https://github.com/shader-slang/slang/discussions/9354),
core contributor): *"cualquier sistema donde el tiempo de compilación importe no debería usar
`slangc` en CLI; debería usar la API C++"*. Motivo medido: `loadBuiltinModule` (cargar el core
module de Slang) toma **~80 ms por invocación** — con la CLI se paga en cada shader más el spawn
del proceso; con la API se amortiza entre todos los compiles del mismo proceso.

**Decisión de linkado: STATIC** (`SLANG_LIB_TYPE=STATIC`). En vez de una DLL/SO compartida, se
linkan las librerías estáticas de Slang (`libslang-compiler.a` + `libcompiler-core.a` +
`libcore.a` + `libminiz.a` + `liblz4.a` en Linux; sus `.lib` equivalentes en Windows/MSVC),
**absorbidas dentro de `LeirEngineEditor.exe`**. Resultado:
- Se mantiene el setup actual de "2 archivos": `LeirEngine.dll` + `LeirEngineEditor.exe`. No hay
  que copiar `slang-compiler.dll` ni `slang-llvm.dll` junto al editor (la alternativa shared, que
  además renombró `slang.dll`→`slang-compiler.dll` en v2025.21, metería 2 DLLs extra).
- Solo el exe del editor engorda (~25-35 MB, orientativo); `LeirEngine.dll` no lo toca, y los
  juegos exportados siguen sin conocer Slang (Sección 2.5).
- La integración se hace en el **target `LeirEngineEditor`** únicamente, nunca en el engine.

```
createGlobalSession()                       // UNA vez, al abrir el editor (~30-80 ms), reutilizable
   └─ createSession(SessionDesc)            // targets: SPIR-V + DXIL + MSL + WGSL + GLSL ES
      └─ loadModuleFromSourceString()      // cache de módulos en el session (import reusado)
         └─ createCompositeComponentType + link   // programa 🔗
            ├─ getTargetCode(i)           // blob SPIR-V/DXIL/MSL/WGSL/GLSL
            └─ getLayout()                // reflection → bindings derivados (Sección 3.3)
```

- **Ciclo de vida**: `IGlobalSession` se crea una vez al iniciar el editor y vive toda la sesión.
  `ISession` (config per escena/compile) se descarta tras exportar — la RAM activa cae al nivel
  base. Multi-target en un solo session: el front-end corre una sola vez y emite los 5 formatos.
- **Hot-reload de shaders en el editor** (beneficio concreto): editar el shader → recompilar con
  el `ISession` vivo (~ms) → inyectar el nuevo binario en el backend Vulkan actual sin reiniciar.
- **`slangc` queda como fallback build-time/CI**: para el build dev del engine sin editor y para
  CI (evita linkear la lib pesada en runners). No es el camino principal.
- **Reflection**: `ProgramLayout::getLayout()` alimenta `IShaderCompiler::Reflect` → la firma de
  bindings que consume la RHI (Sección 3.3).

### 2.3 Targets y formatos (estado verificado de Slang, 2026-08)
| Target | Formato | Estado Slang | Uso |
|---|---|---|---|
| Vulkan | SPIR-V | ✅ soportado | Windows/Linux/Android |
| D3D12 | DXIL (`sm_6_0` — el driver Intel UHD soporta hasta 6.5; `sm_6_6` falla en PSO creation, ver Fase 2b) | ✅ soportado | Windows |
| Metal | MSL | ⚠️ experimental | macOS/iOS |
| WebGPU | WGSL | ⚠️ WIP | Web |
| WebGL2 | GLSL ES 3.00 (`#version 300 es`) | ⚠️ limitado | Web fallback |

- Para WebGL2, el exporter pide a Slang el target **GLSL** con profile compatible ES 3.00
  (los 6 shaders actuales son triviales; Slang emite `in`/`out`, `texture()`, output declarado).
- Nota oficial de Slang: el target GLSL "no espera paridad de features con otros backends"
  → riesgo documentado (Sección 7).

### 2.4 Exporter por plataforma
El editor exporta para cada plataforma **solo** su formato:
- Windows → SPIR-V (Vulkan) y/o DXIL (D3D12), según backend elegible
- macOS/iOS → MSL
- Linux/Android → SPIR-V
- Web → WGSL + GLSL ES 3.00 (para el fallback WebGL2)

### 2.5 Runtime (cero conocimiento de Slang)
`Shader` carga el binario correspondiente al backend activo. El runtime ni siquiera incluye el
nombre `slang` en una ruta de include.

### 2.6 Fallback Web → WebGL2
- Detección en el loader web: `navigator.gpu` + `requestAdapter()` OK → backend WebGPU; si falla
  (Linux flag-gated en Chrome/Firefox, Firefox Linux/Android, WebViews móviles, GPU blocklist de
  driver) → backend **WebGL2**.
- WebGL2 usa shaders GLSL ES 3.00 generados por el exporter con el target GLSL de Slang.
- El motor degrada features según `GCaps` (sin compute, sin bindless, sin MRT...). Mismo RHI,
  distinto backend.
- Contexto verificado 2026-08: WebGPU es baseline en Chrome/Edge 113+, Firefox 141+, Safari 26
  (macOS Tahoe 26 / iOS 26), pero Linux sigue flag-gated y hay long-tail de dispositivos → el
  fallback sigue siendo necesario.

## 3. RHI propia — diseño completo

> Inspirada en el modelo mental de WebGPU (objetos de pipeline explícitos, bind groups, render
> passes) pero **no copiada**: bindings derivados por reflection, passes persistentes, barreras
> automáticas con modo manual, bindless-first y capacidades ricas. Nombres propios Leir.

### 3.1 Conceptos y objetos centrales
- **`RDDevice`** (`RenderDevice`): dueño de GPU, colas, memoria, frames-in-flight, capacidades.
  Crea todos los recursos.
- **`GResource`** (handle opaco, no objeto): base para `GBuffer` (vertex/index/uniform/storage)
  y `GTexture` (color/depth/RT/UAV/sampled) y `GSampler`. Referencia contada por handle; el
  backend concreto escribe el pointer nativo tras un `void*` (PIMPL).
- **`GPipeline`**: pipeline de graphics o compute compilado. Conoce su **firma de bindings desde
  la reflection del shader** (3.3).
- **`GBindTable`**: conjunto de recursos concretos vinculados a una firma (evolución del bind
  group). Validado contra la firma real, no contra layouts escritos a mano.
- **`GPassTemplate`**: estado de render pass **persistente y reutilizable** (attachments,
  load/store/clear, scissor/viewport). `RenderTexture`/offscreen lo reusa sin re-encodear cada
  frame.
- **`GCommandGraph`**: el grafo de trabajo por frame (3.2).
- **`GCaps`**: capacidades y límites del backend (3.6).

### 3.2 `GCommandGraph` — comandos
- Por frame, el motor registra `RenderPassRecord` + `DrawRecord`/`CopyRecord`/`ComputeRecord`
  en un grafo.
- El backend lo traduce a sus comandos nativos y **genera las transiciones de estado
  automáticamente por last-use tracking**.
- **Record multithread**: el grafo se construye desde hilos worker en paralelo; el backend
  serializa al ejecutar.

### 3.3 Bindings derivados por reflection (diferencial #1)
- `GPipeline` obtiene su firma (textures, buffers, samplers, push/root constants, constant
  buffer layout) **de la reflection del compilador de shaders** vía `IShaderCompiler::Reflect`.
- `GBindTable` se construye contra esa firma y se valida en runtime (debug) o carga (release).
- Elimina la fricción #1 de WebGPU: bind group ↔ pipeline layout mismatch escrito a mano.

### 3.4 Sincronización: `Auto` y `Explicit`
- `Auto` (default): barreras/estados derivados del `GCommandGraph` por el backend (WebGPU esconde
  todo; Vulkan expone todo a mano; nosotros: auto por defecto).
- `Explicit`: API de barreras/eventos de primera clase para paths de poder (compute + render
  intercalados, ordenamiento fino).

### 3.5 Bindless-first
- Resource indexing como modelo primario (descriptor indexing Vulkan/D3D12, argument buffers
  Metal, bindless de WebGPU).
- `GBindTable` soporta tables con array de recursos; el límite lo da `GCaps`, nunca el diseño.
- ✅ **Implementado (2026-08-14, paso 3 de Plan B)**: tabla bindless por backend (Vulkan
  descriptor indexing + update-after-bind → 1M texturas; D3D12 heaps SRV/sampler con tabla
  space1/space2). Detalles en el paso 3 de "Plan B".

### 3.6 `GCaps` rico
- Campos: max textures/UBOs/samplers por table, bindless soportado, MRT, instancing, compute,
  sRGB, formatos RT, etc.
- El mismo código del motor se adapta (WebGL2 vs WebGPU vs Desktop) con `GCaps`, no con `#if`.

### 3.7 Especialización de shaders
- Generics/specialization de Slang expuestos como feature de primera clase en `GPipeline`
  (compilar variantes a pedido).

### 3.8 Frames-in-flight y swapchain
- `RDDevice` abstrae el sync de frames (2-3 en vuelo), presentación y resize. `BeginFrame(skipRenderPass)`,
  `BeginSwapchainOverlay`, `Present` (la API actual de `VulkanDevice` migra 1:1).

## 4. Backends

| Backend | Plataformas | Shader | Estado |
|---|---|---|---|
| **Vulkan** | Windows, Linux, Android | SPIR-V | v1 (migrar el actual) |
| **D3D12** | Windows | DXIL | v2 |
| **Metal** | macOS, iOS | MSL | v3 (MoltenVK queda como fallback) |
| **WebGPU** | Web (Emscripten) | WGSL | v4 |
| **WebGL2** | Web (fallback) | GLSL ES 3.00 | v4 (degradado) |
| (futuro) OpenGL ES / D3D11 | legacy | — | opcional |

Editor: backend del host (Windows: Vulkan o D3D12 elegible por `LEIR_BACKEND`).

### 4.1 Resultados del Spike (Fase 0, 2026-08-12)

Herramienta: `slangc 2026.13.1` del Vulkan SDK 1.4.357.0
(`C:\VulkanSDK\1.4.357.0\Bin\slangc.exe`). Los 6 shaders actuales
(`Basic`, `Sprite`, `UI`, .vert/.frag) se reescribieron a `.slang` (sintaxis
HLSL/Slang: `[shader("vertex"/"fragment")]`, `[[vk::binding(binding, set)]]`,
`[[vk::push_constant]]`, tipos `Sampler2D` combinados) y se compilaron a cada
target. **30/30 compilaciones OK** entre los 5 targets viables:

| Shader | SPIR-V | DXIL | Metal | WGSL | GLSL 450 |
|---|---|---|---|---|---|
| Basic.vert | OK | OK | OK | OK | OK |
| Basic.frag | OK | OK | OK | OK | OK |
| Sprite.vert | OK | OK | OK | OK | OK |
| Sprite.frag | OK | OK | OK | OK | OK |
| UI.vert | OK | OK | OK | OK | OK |
| UI.frag | OK | OK | OK | OK | OK |

**Verificación de compatibilidad SPIR-V (render idéntico al actual):**
- `[[vk::binding(a, b)]]` = **binding `a`, set `b`** (orden binding, set).
- Basic.vert: UBO → set 0, binding 0; push constants offsets 0/12/16/28/32/44/48/64
  (coinciden con `struct PushConstants` C++); atributos inPosition/Normal/TexCoord 0/1/2.
- Basic.frag: sampler → set 1, binding 0 (coincide con `layout(set=1, binding=0)`);
  push constants del fragment idénticos al vertex (mismo rango compartido).
- Sprite: sampler → set 0, binding 0; push constants 0/64/80 (= 96 bytes =
  `sizeof(SpritePushConstants)`); atributos 0/1.
- UI.vert: push constant `float2 screenSize` @0 (8 bytes = `sizeof(Vector2)`), stage
  VERTEX only; atributos 0/1/2. UI.frag: sampler set 0, binding 0; sin push constant.

**Hallazgos y decisiones con datos:**
1. **GLSL ES 3.00 NO soportado directamente por Slang** — no existe profile ni
   capability `glsl_es`/`glsl_300_es` (los profiles GLSL van solo de 130 a 460
   desktop). El doc §2.3 asumía pedir "GLSL con profile ES 3.00" — **incorrecto**.
   Para WebGL2 (fallback) hay que usar **SPIRV-Cross** (SPIR-V → GLSL ES 3.00) o
   escribir shaders WebGL2 a mano. El escape hatch SPIRV-Cross ya estaba previsto
   en §7; ahora es el **camino obligatorio** para WebGL2.
2. **Warnings de Metal benignos**: cada shader emite `warning[E40100]: entry point
   'main' has been renamed to 'main_0'` — Slang renombra el entry point para no
   colisionar con `main()` del host MSL. Es normal; no es un error.
3. **Binding combinado**: para emular `sampler2D` (combined image sampler) de GLSL
   en Slang hay que usar el tipo `Sampler2D` (un solo binding). Separar en
   `Texture2D` + `SamplerState` con el mismo binding produce
   `warning[E39001]: explicit binding overlap`.
4. **Target MSL**: el nombre del target ahora es `metal` (antes `msl`).
5. **GLSL 450 generado es limpio** (verificado en Basic.frag): `sampler2D` con
   `binding = 0, set = 1`, push constants std430 con los pads, mismo algoritmo.

Conclusión: los 6 shaders actuales son **100% migrables a Slang** con render
idéntico en Vulkan (SPIR-V verificado), y los targets DXIL/Metal/WGSL/GLSL-450
compilan sin cambios. El único gap es WebGL2 (GLSL ES), que requerirá SPIRV-Cross.

## 5. Fases

### RHI mínima evolutiva (paso previo al RHI completo) — decisión 2026-08-12

Para lograr **Vulkan + D3D12 intercambiables en Windows sin romper nada**, se va por un enfoque
**incremental**: primero una **RHI mínima evolutiva** que abstrae solo lo que el motor usa hoy
(device, buffer, texture, pipeline, render pass, command buffer, descriptor set) con **handles
opacos** (sin `Vk*` en headers públicos). El primer backend es **Vulkan** reusando
`VulkanDevice` (sin reescribir su lógica), se verifica **regresión cero**, y recién ahí se añade
D3D12. Las features avanzadas del diseño completo (§3: `GCommandGraph` con record multithread,
bindless-first, bindings por reflection, `GCaps`, especialización) **no bloquean este objetivo**:
se migran al RHI completo en un paso posterior, cuando la RHI mínima esté estable con ambos
backends desktop.

```
Fase 0-1: shaders Slang (hecho) → [RHI mínima + Vulkan] ✅ → [RHI mínima + D3D12]
                                                              ↓ (estable, 2 backends desktop)
                                                       RHI completo §3 (GCommandGraph, bindless, reflection, GCaps)
```

### Checkboxes de fases

- [x] **Fase 0** — Spike: `slangc` → SPIR-V/DXIL/MSL/WGSL/GLSL con los 6 shaders actuales;
      inspeccionar MSL y GLSL ES. **Hecho 2026-08-12** — 30/30 OK, render SPIR-V idéntico
      (§4.1); GLSL ES 3.00 no soportado directo → SPIRV-Cross.
- [x] **Fase 1** — Migrar shaders a `.slang` + `slangc` en CMake (renderer Vulkan intacto).
      **Hecho 2026-08-12** — 6 `.slang` en `engine/shaders/`, CMake usa
      `slangc -target spirv -profile spirv_1_3`, `.spv` en la misma ruta/nombres; render idéntico.
- [x] **Fase 2a — RHI mínima + backend Vulkan** (paso previo al RHI completo §3).
      **Hecho 2026-08-12** (commit `737dd53` + fixes `e4f186b`/`8345ce0`):
  - [x] Interfaz `RenderBackend`/`IRHI` mínima en headers públicos SIN `Vk*`: handles opacos
        `RHICommandBuffer`, `RHIBuffer`, `RHITexture`, `RHIImageView`, `RHISampler`, `RHIPipeline`,
        `RHIPipelineLayout`, `RHIDescriptorSet`, `RHIDescriptorSetLayout`, `RHIRenderPass`,
        `RHIFramebuffer`, `RHIDeviceMemory` (+ structs `RHIClearValue`, `RHIDescriptorWrite`,
        `RHIDescriptorImageInfo`, `RHIDescriptorBufferInfo`, enums de formato/uso/estado/layout).
  - [x] Backend **Vulkan**: `VulkanBackend` que envuelve/adapta `VulkanDevice` (lógica nativa intacta).
        Se crea vía `BackendFactory::Create()` según el selector `LEIR_BACKEND` (`LEIR_BACKEND_VULKAN`
        por defecto). Incluye `WaitIdle()`, `CmdTransitionImageLayout`, `CmdPushConstants` (con
        `ShaderStageMask`).
  - [x] Migrados a la interfaz RHI: `Shader`, `Material`, `Mesh`, `Texture2D`, `RenderTexture`,
        `RenderPipeline`, `UIRenderer`, `Font` (+ ejemplo `PhysicsDemo`). Headers públicos sin `Vk*`
        (solo `VulkanDevice.h`/`VulkanBackend.h` tocan Vulkan, ambos internos).
  - [x] `editor/src/main.cpp` crea el backend RHI (`BackendFactory::Create`) en vez de `VulkanDevice`
        directo.
  - [x] Verificación **regresión cero**: render idéntico, validation layers limpias (stderr vacío),
        selector `LEIR_BACKEND` funcionando. Fixes incluidos en la misma tanda:
        - `CmdTransitionImageLayout` en `RenderTexture::BeginRender/EndRender` (transiciones del RT
          que la migración había perdido).
        - `WaitIdle()` en `RenderTexture::Resize` (elimina flicker/glitch al arrastrar splitters).
        - Semáforos render-finished **por imagen de swapchain** + in-flight fence por imagen
          (`VUID-vkQueueSubmit-pSignalSemaphores-00067`).
        - Bug de la unión `VkClearValue` en `CmdBeginRenderPass` (el clear de color se pisaba con el
          del depth → fondo magenta del viewport; se añadió `RHIClearValue::isDepth`).
        - Crash de arranque (`vector Line 1931`): `CreateSyncObjects` ya no toca
          `m_RenderFinishedSemaphores` (los crea `CreateSwapchain`, que corre antes en el ctor).
- [ ] **Fase 2b — Backend D3D12** (v2, sobre la misma RHI mínima): implementar la interfaz con
      D3D12; los shaders ya están en DXIL desde Fase 0/1. `LEIR_BACKEND=d3d12` corre igual.
      **Avance 2026-08-12**: `D3D12Backend` implementa la RHI mínima completa (device, buffers,
      textures 2D + staging con row-pitch alineado, samplers, descriptor sets sobre heaps
      shader-visible, pipeline layout/root signature con CBV root + tablas SRV/SAMPLER, PSO,
      framebuffer/render pass, swapchain 3 buffers con fences por frame). Dos bugs de driver/compilación
      encontrados y resueltos con datos (info queue):
      - **Device removed** (`0x887A0005`, reason `0x887A0001`): `CopyTextureRegion` exige
        `RowPitch % 256 == 0` (`D3D12_TEXTURE_DATA_PITCH_ALIGNMENT`) cuando
        `UnrestrictedBufferTextureCopyPitchSupported=false` (Intel UHD) — la textura 1×1 usaba
        row pitch 4. Fix: `RenderBackend::GetCopyRowPitchAlignment()` (1 por defecto, 256 en
        D3D12) y `Texture2D`/`CopyBufferToImage` alinean el staging por filas.
      - **PSO creation fail** (`0x80070057`/E_INVALIDARG): el driver Intel solo soporta hasta
        `sm_6_5` y los DXIL se compilaban `sm_6_6`. Fix: CMake usa `-profile sm_6_0`.
      - **Crash Intel UMD en `SetGraphicsRootDescriptorTable`** (AV `0xc0000005` en
        `igd12umd64.dll`, offset fijo `0x1ad7da` en cada run): se llamaba a los setters de root
        arguments **sin vincular antes la root signature** (`SetGraphicsRootSignature`), undefined
        behavior que el driver Intel materializa como AV. Fix: `CmdBindDescriptorSets` enlaza la
        root signature del layout antes de tocar root params. (Un intento intermedio de migrar el
        sampler set a SRV root descriptor + root sampler se descartó: **las texturas no admiten
        root descriptors** — solo Raw/Structured buffers, `Root Signature doesn't match Pixel
        Shader`; se mantienen las tablas de descriptores.)
      **Estado**: `LEIR_BACKEND=d3d12` corre el editor (12-15 s sin crash, con y sin debug layer),
      stderr limpio, binds de descriptor sets OK, sin device removal. **Paridad de render D3D12 vs
      Vulkan completada y verificada por el usuario (2026-08-13)**: colores, orientación de cubo/
      cámara y nitidez de texto con `hidpi:true` idénticos (ver checkboxes BUG01/BUG02/BUG03 abajo).
      **Teardown D3D12 verificado (2026-08-13)**: cierre limpio en 162-186 ms, sin VUIDs ni crash
      (ver "Teardown limpio" abajo).

#### Teardown limpio D3D12 (2026-08-13, verificado)

- [x] **Teardown limpio verificado con cierre normal** (antes se mataba el proceso con
      `Stop-Process` y nunca se sabía si salía sin VUIDs). Reproducción: lanzar el editor, esperar
      y cerrar con `WM_CLOSE` → los `[Timing]` de `main.cpp`/`CoreApplication.cpp` bisecan el
      teardown; un crash caía directo a WER (varios segundos) sin log porque `CrashDiagnostics`
      solo atrapaba excepciones C++.
- **Fix 1 — `CrashDiagnostics` con `SetUnhandledExceptionFilter`** (`OnUnhandledException` →
  `LogStackWalk`, `EXCEPTION_EXECUTE_HANDLER` = sin WER ni diálogo). Cierra el hueco SEH/AV que
  dejaba cualquier crash nativo mudo (y que también habría atrapado el bug del double-free de
  Vulkan).
- **Fix 2 — el crash real (0x87d en teardown)**: el SEH dump apuntó a
  `RenderTexture::~RenderTexture` → `DestroyResources` → `DestroyMemory` → release de un
  `ID3D12Resource` todavía referenciado por la GPU. La **debug layer** D3D12
  (`D3D12SDKLayers`) levantó `0x87d` en el release. El dtor destruía recursos **sin `WaitIdle`**
  (a diferencia de `Resize()`, que sí espera). Fix: `m_Device->WaitIdle()` al inicio de
  `RenderTexture::~RenderTexture`. Resultado: cierre 162-186 ms, teardown completo, sin
  `crash_diagnostics.log` y sin reportes WER nuevos (3 corridas).


#### Limitaciones conocidas del backend D3D12 (2026-08-13)

> **DECISIÓN (registrada)**: los dos ítems de abajo se **documentan como limitación** y se resuelven
> de raíz en el **RHI completo (§3)**, no en la RHI mínima. NO arreglarlos ahora.
>
> **RESUELTO (2026-08-14, Fase 3 bindless)**: el ítem #1 se eliminó de raíz con la migración
> bindless-first (paso 3 de "Plan B"). Ya no hay `WriteDescriptorSets` de imagen en runtime — las
> texturas se registran en la tabla bindless y el resize del viewport RT reescribe el slot en
> `SrvCpu(index)`/`SamplerCpu(index)` **in-place**, sin alocar nada. Quedan vigentes #2 y #3.

1. **Slots de descriptores SRV/sampler que nunca se liberan.** ~~El backend aloca un slot del
   `srvHeap` (4096) por cada `WriteDescriptorSets` de imagen (`AllocSrv()`, `D3D12Backend.cpp:912`)
   y un slot de `samplerHeap` (64) por cada sampler único (`AllocSampler()`, cacheado), pero **nunca
   los devuelve**~~. **ELIMINADO (Fase 3 bindless, 2026-08-14)**: el camino `AllocSrv`/`AllocSampler`/
   `samplerCache`/`srvSlot` quedó muerto; el único `WriteDescriptorSets` restante es el UBO (CBV,
   no aloca slots). El heap SRV ya no crece con los resizes del viewport RT.
2. **`cmdList4` sin uso** (residuo del intento descartado de root sampler): declarado en
   `D3D12Backend.cpp:196` y creado por `QueryInterface` en `CreateFrameObjects` (`:423`), nunca se
   usa. Se puede eliminar en cualquier limpieza menor.
3. **`mainRenderPass`/`overlayRenderPass` nunca borrados** en `~Impl` (2 `RenderPassRec` `new` en el
   ctor, usados como handles por el render pipeline que NO los destruye). Leak cosmético de ~2
   structs por backend; el `RenderTexture` sí crea/destruye su propio render pass.

### Checkboxes — Paridad de render D3D12 vs Vulkan (Fase 2b, 2026-08-13)

Los 3 bugs reportados al activar `LEIR_BACKEND=d3d12` con `hidpi:true`. Objetivo: render visual
idéntico entre backends con soluciones **universales** (sin hardcodear por API). Regla aprendida:
la convención NDC es **per-backend** (D3D12/Metal/WebGL: y-up → viewport positivo; Vulkan/WebGPU:
y-down → viewport negativo); el código compartido (Camera, RenderPipeline) queda como matemática
GLM pura y front-face **CCW en todos los backends**.

- [x] **BUG03 — cubo invertido/faltan caras + eje Y de cámara invertido (D3D12)**.
      Causa: `Camera::SetPerspective` hace `m_ProjectionMatrix(1,1) *= -1.0f` incondicional — era
      la compensación de Vulkan (NDC y-down) pero rompe D3D12 (NDC y-up, igual a GLM). Fix:
      quitar el flip de `Camera` y mover la compensación al backend Vulkan con viewport de altura
      negativa en la pasada 3D (swapchain `VulkanDevice::BeginFrame` + `VulkanBackend::CmdBeginRenderPass`
      para el RenderTexture). D3D12 sin cambios. **Verificado por el usuario (2026-08-13): cubo y
      cámara correctos en ambos backends.**
- [x] **BUG01 — colores más oscuros en D3D12**. Causa: swapchain D3D12 en `B8G8R8A8_UNORM`
      (Vulkan en `B8G8R8A8_SRGB`) → sRGB no aplicada al presentar. Fix original: `B8G8R8A8_UNORM_SRGB`
      en el formato del swapchain, `ResizeBuffers` y los `colorFormats` de los render passes builtin
      (main + overlay). **Desviación encontrada en el smoke test**: el driver **Intel UHD
      device-removes el swapchain** (`DXGI_ERROR_DEVICE_REMOVED` 0x887A0001) al crearlo con sRGB.
      Fix final (equivalente): el **recurso** del backbuffer queda en `UNORM` (CreateSwapchain y
      `ResizeBuffers`), pero los **RTVs del backbuffer se crean con `B8G8R8A8_UNORM_SRGB`**
      (`InitBackBuffers`, `D3D12_RENDER_TARGET_VIEW_DESC` con `ViewDimension=TEXTURE2D`); los
      `colorFormats` de main/overlay pasan a `UNORM_SRGB`. El encode sRGB ocurre en el store del
      RTV → idéntico a un swapchain sRGB. **Verificado por el usuario: colores idénticos a Vulkan.**
- [x] **BUG02 — UI pixelada con `hidpi:true` solo en D3D12**. Diagnóstico: capturas DPI-aware +
      análisis programático (cross-correlación → renders **alineados a píxel** entre backends,
      escala/posiciones idénticas; solo difieren bordes de glifos en ~0.7pp). Causa confirmada:
      el atlas de fuente se rasterizaba a **`fontSize` lógicos** (16/13px) y se upscaleaba con
      sampler **`Nearest`** a 1.25× → trazos desiguales (grumoso). Fix universal: **rasterizar el
      atlas a `fontSize × contentScale`** (`Font::Font(..., float contentScale)`), manteniendo
      todas las métricas en **unidades lógicas** (atlas px ÷ scale: `advance`/`bearing`/`size`/
      `LineHeight`/`Ascender`/`SpaceWidth`), así el layout no cambia y cada texel mapea 1:1 a un
      píxel físico → texto nítido a cualquier DPI en **ambos** backends. `Font::SetContentScale()`
      re-rasteriza el atlas in-place (los `Font*` holders siguen válidos); el editor la llama en
      `OnContentScaleChanged` y pasa `GetContentScale()` al crear las fuentes. **Verificado por el
      usuario (2026-08-13): texto perfecto en D3D12 con `hidpi:true`.**

- [x] **Fase 3** — `IShaderCompiler` en el editor + exporter multi-formato + hot-reload.
      Verificación (2026-08-14): export 6/6 en los 5 targets (30 archivos en `shaders_export/`);
      **hot-reload funcionando** — editar `Basic.vert.slang` en vivo → `[HotReload] ... -> Basic.vert.dxil`
      (5412 bytes), stderr vacío (sin errores de compilación ni VUIDs), salida limpia.
      Implementado con **libslang dinámica** del SDK (no la `SLANG_LIB_TYPE=STATIC` original —
      desviación documentada en el "Estado Plan A" abajo).
      **Plan de acción detallado → "Plan A" abajo (2026-08-14).**
- [ ] **Fase 4** — Backend **Metal** (+ MoltenVK fallback). Verificación: macOS.
      **Pospuesto hasta después de WebGPU** (ver decisión en el Plan C: no hay Mac; CI solo
      compila, no renderiza; VM con macOS no da Metal real).
- [ ] **Fase 5** — **WebGPU + WebGL2** + Emscripten + capa de plataforma (sacar GLFW).
      Verificación: `leir_engine.js`, fallback funcionando.
      **Plan de acción detallado → "Plan C" abajo (2026-08-14).**
- [ ] **Fase 6** — **Android** (reusa Vulkan + plataforma). Verificación: APK.
- [ ] **Fase 7** — **iOS** (reusa Metal + plataforma). Verificación: App iOS.
- [ ] **Migración al RHI completo §3** (cuando la RHI mínima esté estable con Vulkan+D3D12):
      `GCommandGraph` con record multithread, bindless-first, bindings por reflection,
      `GCaps`, especialización de shaders.
      **Plan de acción detallado → "Plan B" abajo (2026-08-14).**

### Plan de acción — A: Fase 3 / B: RHI completo §3 / C: WebGPU (2026-08-14)

Orden recomendado: **A → B → C** (A es pre-requisito de B; C valida el diseño de B con un
backend distinto). Decisión clave: el **3er backend es WebGPU, no Metal** — se desarrolla y
verifica en Windows (Chrome/Edge lo soportan nativo), su modelo (bind groups + render passes)
espeja §3 (valida GCaps / sincronización Auto / bindless), y su fallback WebGL2 es el backend
"degradado" que justifica `GCaps`. Metal queda para cuando exista acceso a hardware real
(no hay Mac; macOS en VM sobre PC no expone Metal — solo framebuffer software; GitHub Actions
macOS runners solo verifican compilación, sin render).

#### Plan A — Fase 3: `IShaderCompiler` + reflection + hot-reload (pre-requisito)

Reflection es el "diferencial #1" del RHI completo (§3.3): hoy las firmas de bindings se
escriben a mano en C++ (layout + desc set + binding por pipeline) y hay que mantenerlas en
sync con el shader. Con `Reflect()` el `GPipeline` deriva su firma del shader mismo.

Pasos:
1. **libslang estática en el superbuild** — `FetchContent` en `dependencies/CMakeLists.txt` con
   `SLANG_LIB_TYPE=STATIC`, flag CMake opcional (risco §7: pesada de compilar). **Solo el editor
   la linkea**; el engine DLL sigue sin depender de Slang (aislamiento §6).
2. **Interfaz pública `IShaderCompiler`** (`engine/include/LeirEngine/RHI/` o `Shaders/`):
   `ShaderTarget` (SPIR-V / DXIL / MSL / WGSL / GLSL-450 / GLSL-ES), `CompileResult`,
   `ShaderReflection` (bindings, push/root constants, buffer layout, stage). Ningún nombre
   `slang` en headers públicos (§6).
3. **Implementación en el editor** (`editor/src/Shaders/`): `SlangShaderCompiler` usando
   `IGlobalSession` → `ISession` → `createModuleFromSource` / `createEntryPoint` /
   `getLayout()` para reflection. Wrapper C++ sobre la C API de Slang.
4. **Exporter multi-formato**: dado el `.slang`, compilar a SPIR-V (Vulkan), DXIL (D3D12),
   MSL (Metal), WGSL (WebGPU), GLSL-450 (debug) — y GLSL-ES vía **SPIRV-Cross** (hallazgo del
   spike §4.1: Slang no emite GLSL ES 3.00 directo). Verificación: los 6 shaders actuales
   compilan a los 5+1 targets (la tabla 30/30 del spike §4.1 pasa a generarse en runtime).
5. **Hot-reload**: file watcher sobre `engine/shaders/*.slang`; al cambiar → recompilar en
   runtime → recrear pipelines. Verificación: editar un shader y ver el cambio en vivo.
6. **Verificación**: export por plataforma + hot-reload funcionando; regresión: render Vulkan
   idéntico (validation layers limpias); CI valida SPIR-V/WGSL (DXIL solo exporter Windows, §7).

**Estado Plan A (2026-08-14):** implementado y verificado (todo 6/6 en los 5 targets). Pasos 1-5
hechos, verificación de export completa. Desviaciones/hallazgos con datos:

- **libslang del SDK en vez de estática por FetchContent** (paso 1 modificado): se usa la
  precompilada del Vulkan SDK (`$VULKAN_SDK/Include/slang` + `Lib/slang.lib` + 5 DLLs de
  `Bin/slang*.dll` copiadas junto al exe). Razón documentada en `editor/CMakeLists.txt`: un build
  por fuente arrastra el compilador + LLVM (lento/frágil); la del SDK es version-exacta (2026.13.1)
  con el `slangc` que compila los shaders. Solo editor; engine sin Slang (§6).
- **`slang_createGlobalSession` (C API simple) NO habilita GLSL**: zeroea el `SlangGlobalSessionDesc`
  → `enableGLSL=false` → el módulo `glsl` nunca se carga → `error[E38201]: 'glsl' module not
  available`. Fix: `SlangGlobalSessionDesc desc = {}; desc.apiVersion = SLANG_API_VERSION;
  desc.enableGLSL = true; slang_createGlobalSession2(&desc, &session)`.
- **`loadModuleFromSource` (módulos en memoria) rompe la validación de capabilities**:
  cualquier `cbuffer` global exige `Std140DataLayout`, que es *unavailable* en DXIL/Metal/WGSL →
  `error[E36107]` en `getEntryPointCode` (y `slangc` compilaba el mismo `.slang` OK). No era
  `[[vk::binding]]`, ni matrix layout, ni versión de lenguaje, ni profile (bisección con un harness
  C++ contra `slang.lib`). Fix: cargar el módulo desde el **file system** (`ISession::loadModule`
  con la ruta del archivo) en vez de `loadModuleFromSource`. `CompileFromSource` (API pública
  in-memory) hace stage a un `.slang` temporal y pasa por el mismo camino de archivo. Verificado:
  SPIR-V/DXIL/Metal/WGSL/GLSL-450 → **6/6** cada uno (30 archivos en `shaders_export/`).
- TODO_UI_EVENT_FLOOD RULE respetado: los logs de export son `[debug]`/`[info]` acotados por acción,
  nunca por frame.
- **Vendoring de libslang — `editor/vendor/slang/` v2026.14.1** (2026-08-14, CI verde en 3
  plataformas): las prebuilt del release de [shader-slang](https://github.com/shader-slang/slang)
  quedan commiteadas en el repo (naming moderno `slang-compiler`/`slang-rt`, sin el proxy
  deprecado `slang.dll`; ver `editor/vendor/slang/README.md`). El tooling del editor deja de
  depender de `$VULKAN_SDK` y su gate se abre a todas las plataformas (antes
  `WIN32 AND MSVC AND DEFINED ENV{VULKAN_SDK}`). CMake compartido `cmake/SlangTooling.cmake`
  (`leir_setup_slang_target`, usado por editor y tests): include vendored + link por OS
  (Windows `slang-compiler.lib`, Linux `.so.0.2026.14.1`, macOS `.0.2026.14.1.dylib`) + copia
  POST_BUILD de las DLL/.so/.dylib + `BUILD_RPATH` (`$ORIGIN` Linux / `@loader_path` macOS).
  Smoke test CTest **`SlangExportTest`** (`tests/SlangExportTest.cpp`): corre el export 6/6 × 5
  targets en CI — valida link + carga dinámica (dlopen/dyld) + codegen en las 3 plataformas.
  Fixes conexos (CI estaba rojo desde `3dfc232`, invisible porque varios commits usaban
  `[skip ci]`):
  - `LEIR_SHADER_DIR` y `LEIR_SHADER_SOURCE_DIR` ahora se definen **siempre** en el engine
    (antes dentro de `if(SLANGC)` → macOS/Linux sin `slangc` fallaban con "undeclared identifier
    LEIR_SHADER_DIR"); el compile de shaders sigue gated a `slangc` (best-effort).
  - Faltaba **`enable_testing()`** en el root `CMakeLists.txt` → `ctest` nunca registró ni corrió
    ningún test ("No tests were found"). `PhysicsTest` y `SlangExportTest` ahora corren de verdad.
  - `TempSlangFilePath()` (CompileFromSource) ahora usa `TEMP`→`TMPDIR`→`TMP`→`.` (antes solo `TEMP`).
  - **CI Windows pasa a compilar con MSVC (`cl`)** (2026-08-14): el runner de GitHub Actions no
    configura `cl.exe`, así que el preset `windows-ci-debug` (Ninja) caía en el MinGW de la imagen
    (`C:/mingw64`); los exes MinGW **se cuelgan silenciosamente antes de `main()`** al ejecutarlos
    (los tests nunca corrieron: ctest no encontraba tests por falta de `enable_testing()`, y al
    arreglarlo ambos tests —incluso PhysicsTest, sin Slang— colgaban sin imprimir nada). Fix:
    paso `ilammy/msvc-dev-cmd@v1` + `-DCMAKE_CXX_COMPILER=cl` → CI Windows = preset local
    `windows-debug` (MSVC). Verificado: CI verde en 3 plataformas (Windows MSVC: PhysicsTest +
    SlangExportTest; DXIL compila con slangc). `ctest --timeout 120` como guard contra hangs.
  - DXIL es target **solo-Windows**: requiere el compilador externo `dxc` (dxcompiler), que solo
    existe en el Vulkan SDK de Windows → `AllTargets()` lo omite en no-Windows (el smoke test
    espera 5 targets en Windows, 4 en macOS/Linux).

#### Plan B — Migración al RHI completo §3 (GCommandGraph, bindless, reflection, GCaps)

Construir sobre la RHI mínima (Vulkan+D3D12 estables y paritarios) y la reflection del Plan A.
Scope: **sin frame graph total** (§8); barreras auto + modo `Explicit` (§3.4). Cada paso con
verificación de paridad Vulkan↔D3D12.

Pasos (en orden, cada uno verificado antes del siguiente):
1. **`GCaps`** (§3.6): struct de capacidades por backend (max textures/UBOs/samplers por table,
   bindless, MRT, instancing, compute, sRGB, formatos RT). Rellenar Vulkan y D3D12; exponer vía
   `RDDevice`. Verificación: `Println` de caps por backend; degradación por `if (caps.x)`, no `#ifdef`.
   ✅ **Hecho 2026-08-14 (Fase 1)**: `GCaps` en `RHI.h`, `RenderBackend::GetCaps()`, relleno Vulkan
   (desde `VkPhysicalDeviceProperties/Features2` + descriptor indexing) y D3D12 (tier de binding,
   límites de heaps). Editor imprime `[GCaps]` por backend. Verificado: Vulkan
   `textures=200 ubo=200 samplers=64 ssbo=200 push=256B bindless=true` (iGPU Intel UHD, descriptor
   indexing disponible); D3D12 `1M slots, samplers=2048, push=256B, bindless=true` (tier 3).
2. **Bindings por reflection** (§3.3): `GPipeline` obtiene su firma de `IShaderCompiler::Reflect`;
   `GBindTable` se valida contra esa firma (runtime debug / load release). Eliminar los layouts
   escritos a mano y la clase de errores "Root Signature doesn't match Pixel Shader". Verificación:
   renders idénticos con validación en runtime; un binding mal puesto falla en debug.
   ✅ **Hecho 2026-08-14 (Fase 2)**: firma por **sidecar offline** (el exporter emite los bindings por
   shader; el engine carga el sidecar en runtime). Engine queda 100% slang-free (§6) y el flujo es
   idéntico para SPIR-V y DXIL (parsear DXIL en runtime exigiría linkear dxcompiler/LLVM al DLL).
   Formato canónico `<name>.reflect.json` = `{stage, bindings:[{name,set,binding,type,count,stage}],
   pushConstants:[{stage,offset,size}]}`. Nuevo `ShaderLayout.h/.cpp` en el engine:
   `LoadShaderReflectionFromSidecars`, `CreateSetLayoutsFromReflection` (ascendente por set),
   `CreatePipelineLayoutFromReflection` (valida set a set + **fusiona push ranges solapados por
   offset+size con stage combinado** — requerido por Vulkan), `ValidateSetLayoutAgainstReflection`.
   Migrados a layouts derivados: `Material` (set0 UBO + set1 sampler), `UIRenderer` (set0 sampler,
   push 8B vertex), `RenderPipeline::Sprite` (set0 sampler, push 96B). Fallback legacy cuando falta
   el sidecar (engine standalone). El editor genera los sidecars en `LEIR_SHADER_DIR` antes de crear
   el `Shader` (`ShaderExporter::WriteRuntimeSidecars`, compilador creado antes en `OnInit`);
   `ShaderExporter::ExportAll` los emite también en el export root; hot-reload los regenera.
   **Hallazgos arreglados durante la implementación**: (1) la reflexión de Slang para SPIR-V reporta
   los recursos como `DescriptorTableSlot`(9)/`Mixed`(1), NO `ConstantBuffer`/`PushConstantBuffer` —
   la clasificación correcta es por `BindingRange` del `TypeLayoutReflection`
   (`getBindingRangeType` → `BindingType::PushConstant`/`ConstantBuffer`/`CombinedTextureSampler`);
   (2) el tamaño del push constant viene de `leaf->getElementTypeLayout()->getSize()` (bytes), no de
   `getSize(PushConstantBuffer)` (=1, sin sentido) — patrón canónico de `slang-reflection-json`/docs.
   **Verificado**: ctest 2/2; editor Vulkan y D3D12 limpios (0 VUIDs/errores, antes un aluvión de
   `vkCreateDescriptorSetLayout`/push-range/mismatch); paridad pixel-diff vs Fase 1 (Vulkan 0.39%,
   D3D12 0.25% = ruido cursor/FPS) y cross-backend 1.33% ≈ baseline 1.4%; sidecars 6/6 y
   `SlangExportTest` valida 6 `.reflect.json` por ejecución. Nota: `nlohmann_json` pasó a **PUBLIC**
   en `engine/CMakeLists.txt` (header-only; lo necesita el editor para serializar sidecars).
3. **Bindless-first** (§3.5): descriptor indexing (Vulkan `VK_EXT_descriptor_indexing` / D3D12
    heap SRV grande) + `GBindTable` con arrays; límite por `GCaps`, no por diseño. **Paga la deuda
    documentada** ("Limitaciones conocidas del backend D3D12" #1: slots SRV/sampler nunca liberados)
    — nada se aloca/libera por frame; se indexa directo en el shader. Verificación: resize del
    viewport RT en D3D12 sin crecimiento de heap SRV (se elimina el leak por resize).
    ✅ **Hecho 2026-08-14 (Fase 3)**: una **tabla bindless por backend** reemplaza los sets
    single-sampler por textura. `RHI.h`: `RHIDescriptorBinding.bindless` (arrays runtime;
    `count=UINT32_MAX` = unbounded, el backend lo sustituye por su bound), `RHIDescriptorWrite.
    dstArrayElement`, `Format::R32_SFLOAT`, `RHIVertexAttribute.semanticIndex`. `RenderBackend`:
    5 métodos — `RegisterBindlessTexture`/`UpdateBindlessTexture`/`UnregisterBindlessTexture`
    (free-list de índices)/`GetBindlessDescriptorSet`/`GetBindlessMaxTextures`.
    **Vulkan**: features descriptor indexing con **update-after-bind** (imprescindible: el iGPU
    Intel UHD tiene `maxPerStageDescriptorSamplers=64` y `maxPerStageResources=200`, y un binding
    bindless de 200 CIS violaba `VUID-VkPipelineLayoutCreateInfo-descriptorType-03016` y
    `VUID-VkGraphicsPipelineCreateInfo-layout-01688`). El layout usa PARTIALLY_BOUND per-binding +
    `VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT` y el pool `VK_DESCRIPTOR_POOL_
    CREATE_UPDATE_AFTER_BIND_BIT`; la tabla se dimensiona con los límites update-after-bind →
    **1.048.576 texturas**. Gotchas del header SDK 1.4.357: el set-level
    `VK_DESCRIPTOR_SET_LAYOUT_CREATE_PARTIALLY_BOUND_BIT` fue **eliminado** (ahora es per-binding
    via `VkDescriptorSetLayoutBindingFlagsCreateInfo`), el feature genérico
    `descriptorBindingUpdateAfterBind` fue eliminado (usar `descriptorBindingSampledImageUpdate
    AfterBind`) y `maxPerStageDescriptorUpdateAfterBindResources` se llama
    `maxPerStageUpdateAfterBindResources`.
    **D3D12**: heaps shader-visible SRV (4096) + sampler (64 → **2048**, `kBindless`); root
    signature bindless = tabla SRV `space1` + tabla sampler `space2` (ambas `NumDescriptors=
    kBindless`, pixel) — DXIL divide `Sampler2D textures[]` en `t0,space1unbounded` +
    `s0,space2unbounded`. `CreateSampler` ya no aloca slot (guarda el `D3D12_SAMPLER_DESC`),
    `UpdateBindlessTexture` reescribe SRV+sampler **in-place** en el slot del índice → **el resize
    del viewport RT ya no crece ningún heap**. El camino legacy `AllocSrv`/`AllocSampler`/
    `samplerCache`/`srvSlot` quedó muerto (el único `WriteDescriptorSets` restante es el UBO).
    **Shaders** (6): `Basic.vert/frag` y `Sprite.vert/frag` ganan `uint textureIndex` al final del
    push (mismos offsets en C++: 128/96; std430 144/112, ambos stages declaran el struct idéntico
    → range único) y una `Sampler2D textures[]` bindless (`[[vk::binding(0,1)]]` Basic /
    `[[vk::binding(0,0)]]` Sprite), indexada `textures[push.textureIndex]` (uniform por draw → sin
    NonUniform). `UI.vert` gana el atributo loc3 `float fragTexIndex : TEXCOORD1` (`UIVertex.
    textureIndex`, formato `R32_SFLOAT`, semantic TEXCOORD index 1); `UI.frag` usa
    `textures[NonUniformResourceIndex((uint)input.fragTexIndex)]` — un draw mezcla varias
    texturas — pero **solo para los targets que lo soportan**: `NonUniformResourceIndex` no
    compila en WGSL/Metal/GLSL (import falla), por lo que se emite vía `__target_switch`
    (`case hlsl: case spirv:` lo usan; `default:` indexa directo). Se verifica empíricamente que
    el índice DXIL del `textureIndex` es DWORD 32 (Basic) / DWORD 24 (Sprite), alineado con los
    structs C++.
    **Engine**: `Texture2D` (se registra en `CreateFromData`, se des-registra en el dtor,
    `GetBindlessIndex`), `RenderTexture` (registro en ctor, `UpdateBindlessTexture` in-place en
    `Resize`, des-registro en dtor — eliminados `m_DescSetLayout/m_DescPool/m_DescriptorSet`),
    `Material` (sin pool/set; `Bind` enlaza el set bindless global en set 1; fallback legacy
    bindless), `RenderPipeline` (push `textureIndex` en `RenderMeshRenderer`; `RenderSprite`
    enlaza el set bindless en set 0 y elimina `descSetCache`/`descPool`; `RenderOverlay` sin
    cambios), `UIRenderer` (eliminados `GetOrCreateDesc`/`m_DescCache`/`m_DescPool`; el set
    bindless se enlaza **una vez** en `Flush`; batching por `texIndex`+scissor; los viewports usan
    `GetBindlessIndex()`).
    **Verificado (2026-08-14)**: ctest 2/2; editor Vulkan y D3D12 limpios (0 VUIDs / 0 errores
    debug layer), close limpio; bindless table Vulkan `1.048.576 (update-after-bind)`; paridad
    cross-backend 1.52% ≈ baseline 1.4%; el leak de SRV por resize quedó eliminado estructuralmente
    (no queda ningún path que aloque SRVs por textura/frame).
4. **`GCommandGraph`** (§3.2): `RenderPassRecord` + `DrawRecord`/`CopyRecord`/`ComputeRecord`
   registrados por frame; el backend traduce a comandos nativos y **genera transiciones de estado
   por last-use tracking** (reemplaza los `TransitionImageLayout`/`CmdBarrier` manuales).
5. **Record multithread** (§3.2): el grafo se construye desde hilos worker en paralelo; el backend
   serializa al ejecutar. Verificación: benchmark de frame time vs hilo único.
6. **`GPassTemplate`** (§3.1): render pass persistente/reutilizable (attachments, load/store/clear,
   scissor/viewport); `RenderTexture`/offscreen lo reusan sin re-encodear por frame.
   ✅ **Hecho 2026-08-14 (Fase 1)**: `RHIPassTemplateDesc`/`RHIPassTemplate` en `RHI.h`,
   `RenderBackend::CreatePassTemplate`/`DestroyPassTemplate`, `CmdBeginRenderPass` ahora toma el
   template. Vulkan precomputa clears + viewport Y-flip + renderArea; D3D12 clears + viewport +
   scissor. `RenderTexture` usa template persistente (recreado solo en resize o si cambian los
   clears). Verificado: paridad de render vs baseline (Vulkan 0.3%, D3D12 0.2% de diff = cursor/FPS),
   layers limpias, cierres 219-321 ms. Nota: bug de doble-free en Resize (destroy explícito +
   BuildPassTemplate) arreglado durante la verificación — el crash lo capturó CrashDiagnostics
   (doble `delete` de `PassTemplateRec` → `_Orphan_all` → 0xC0000005).
7. **Especialización de shaders** (§3.7): generics de Slang expuestos en `GPipeline`; compilar
   variantes a pedido.
8. **Verificación final**: paridad de render Vulkan↔D3D12 con screenshots pixel-diff; validation
   layers / debug layer limpias; teardown limpio (benchmark cierre 162-186 ms se mantiene); sin WER.

#### Plan C — Backend WebGPU + WebGL2 (3er backend)

Validación real del diseño §3 (GCaps, sincronización Auto, bindless) + apertura a Web. Se
desarrolla en Windows (Chrome/Edge 113+ nativos) — sin Mac.

Pasos:
1. **Capa de plataforma** (Fase 5 lo exige): abstraer GLFW detrás de `PlatformWindow`/
   `IPlatform` (window, input, event loop, vsync). `CoreApplication`/`InputManager` migran a la
   capa. Verificación: desktop (Vulkan/D3D12) sigue funcionando sin cambios de comportamiento.
2. **Emscripten en el superbuild**: toolchain + `FetchContent`/preset dedicado; target
   `leir_engine.js`/`.wasm`. Verificación: build web del engine + editor demo arranca en Chrome.
3. **Backend WebGPU** (`WebGPUBackend` sobre la RHI): wgpu-native o Dawn C API; shaders WGSL vía
   Slang (spike F0: 30/30 OK). Implementa la misma interfaz `RenderBackend`; modelo de sync
   **Auto** (WebGPU esconde las barreras).
4. **WebGL2 fallback** (§2.6): SPIRV-Cross (SPIR-V → GLSL ES 3.00), `GCaps` degradado (sin
   compute, sin bindless, sin MRT), tables clásicas. Verificación: mismo build corre con WebGPU
   o WebGL2 según `navigator.gpu`/fallback.
5. **Verificación**: `leir_engine.js` corre en Chrome/Edge local con WebGPU y con WebGL2 forzado;
   render del demo idéntico al desktop (a escala); CI: build Emscripten en Linux runner.
6. **Cierre del diseño §3**: con WebGPU (Auto, bindless, GCaps) + WebGL2 (degradado) + Vulkan/D3D12
   (Explicit, manual) el modelo completo queda validado en 4 backends reales.

## 6. Aislamiento / desacople total

- El engine público **solo conoce**: `IShaderCompiler`, `ShaderTarget`, `CompileResult`,
  `ShaderReflection`. Ningún nombre `slang` en headers públicos.
- Si Slang desaparece → nueva impl de `IShaderCompiler`. Los artefactos exportados ya están en
  formatos estándar (SPIR-V/DXIL/MSL/WGSL), así que el runtime ni se entera.
- La RHI es el front-end estable; backends y compilador son implementaciones intercambiables.
- Toda la API de compilación, reflection y export queda documentada detrás de `IShaderCompiler`.

## 7. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| MSL / WGSL / GLSL experimentales en Slang | Spike F0; escape hatch SPIRV-Cross (→MSL) / Tint·naga (→WGSL) |
| libslang pesada de compilar | Flag CMake opcional; solo el editor la linkea (estática, sin DLLs de distribución); CI usa slangc |
| DXIL/validador en CI Linux | DXIL solo en exporter Windows; CI Linux valida SPIR-V/WGSL |
| GLSL sin paridad de features | WebGL2 es el backend degradado (features limitadas por diseño vía GCaps). **Spike 2026-08-12**: Slang no emite GLSL ES 3.00 directo (sin profile) → WebGL2 usa SPIRV-Cross (SPIR-V → GLSL ES), ya previsto |
| Complejidad RHI | Enfoque incremental (decisión 2026-08-12): RHI **mínima** primero (backend Vulkan + D3D12 desktop, regresión cero), luego se migra al RHI completo §3 (GCommandGraph/bindless/reflection/GCaps) cuando esté estable. Scope v1: sin frame graph total, barreras auto + modo Explicit |
| Bindless limitado en WebGL2 | GCaps por backend; WebGL2 usa tables clásicas |

## 8. NO hacemos (v1)

Frame graph total, compute de alto nivel, ray tracing, shaders dinámicos en runtime (hot-reload
queda como feature futura opcional con libslang en el editor).
