# TODO_RHI_SLANG.md — RHI propia + Slang (multiplataforma)

Documento de decisión y plan. Fecha: 2026-08-10.

## 1. Decisión y contexto

- **Objetivo**: editor en Windows/macOS/Linux; runtime del motor en Windows, macOS, Linux,
  Android, iOS y Web (WebGPU, con fallback WebGL2).
- **Shaders**: **Slang** como fuente única (HLSL-superconjunto, Khronos governance, Apache 2.0,
  producción: Source 2/Valve). Emite SPIR-V, DXIL, MSL, WGSL y GLSL desde un solo frontend.
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
| D3D12 | DXIL (`sm_6_6`) | ✅ soportado | Windows |
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

## 5. Fases

| Fase | Qué | Verificación |
|---|---|---|
| **0** | Spike: `slangc` → SPIR-V/DXIL/MSL/WGSL/GLSL con los 6 shaders actuales; inspeccionar MSL y GLSL ES | Decisión con datos |
| **1** | Migrar shaders a `.slang` + `slangc` en CMake (renderer Vulkan intacto) | Igual visual que hoy |
| **2** | **RHI propia** + backend **Vulkan**; portar Mesh/Material/Texture/RenderPipeline/UIRenderer/RenderTexture | Regresión cero, headers sin `Vk*`, validation CI |
| **3** | `IShaderCompiler` en el editor (libslang **estática**, `SLANG_LIB_TYPE=STATIC`, `IGlobalSession`/`ISession`) + exporter multi-formato | Exportar shaders por plataforma; **hot-reload de shaders funcionando** |
| **4** | Backend **D3D12** | `LEIR_BACKEND=d3d12` corre igual |
| **5** | Backend **Metal** (+ MoltenVK fallback) | macOS |
| **6** | **WebGPU + WebGL2** + Emscripten + capa de plataforma (sacar GLFW) | `leir_engine.js`, fallback funcionando |
| **7** | **Android** (reusa Vulkan + plataforma) | APK |
| **8** | **iOS** (reusa Metal + plataforma) | App iOS |

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
| GLSL sin paridad de features | WebGL2 es el backend degradado (features limitadas por diseño vía GCaps) |
| Complejidad RHI | Scope v1 acotado: sin frame graph total, barreras auto + modo Explicit |
| Bindless limitado en WebGL2 | GCaps por backend; WebGL2 usa tables clásicas |

## 8. NO hacemos (v1)

Frame graph total, compute de alto nivel, ray tracing, shaders dinámicos en runtime (hot-reload
queda como feature futura opcional con libslang en el editor).
