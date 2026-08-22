# WebGPU — Single-source de shaders (Slang → WGSL)

Objetivo: **escribir el shader una vez en `.slang`** y que Slang genere el WGSL
para **WebGPU native** (editor, wgpu-native) y **WebGPU web** (export,
emscripten), en vez de mantener `.wgsl`/`.web.wgsl` a mano (que derivan).

Decisión de arquitectura (confirmada por el usuario, 2026-08-21):

- **Opción A**: bindless se mantiene en **native** (Vulkan/D3D12/WebGPU-native,
  mismo patrón del engine). La **web** degrada el bindless a textura única por
  draw (limitación de naga/`binding_array`, experimental en Chrome/Tint, no
  portable hoy). Un solo `.slang` genera ambos via flag `#ifdef`.
- **Scope**: todos los shaders del engine (Grid/Gizmo/Basic/Sprite/UI) — ver
  estado abajo. Queda el build web (Etapa C).
- **Future-proof**: cuando `binding_array` aterrice en la web, se cambia el flag
  web a ON — cero cambio en el motor.

Estado real de bindless en web (2026): no estandarizado (gpuweb #380, abierto),
prototipo activo en Chrome/Dawn/Tint, **no** en naga (Firefox/wgpu-web). La
industria (Bevy, Unity, Godot) usa `#ifdef BINDLESS` / degradación por-draw.

## Plan (checkboxes)

- [x] Confirmada Opción A + scope Grid (2026-08-21)
- [x] **Fase 0 — Verificar la salida WGSL de Slang** (Grid.vert/frag.slang, vía
      `SlangExportTest` → `tests/slang_export/wgsl/`). Hallazgos:
      - **Entry**: `fn main(` → hay que renombrar a `fn vs_main(`/`fn ps_main(`.
      - **Push**: emite `var<uniform> push_0` **sin** `@group/@binding` → hay que
        anotarlo `@group(1) @binding(0)` (el grupo del push del backend =
        `setLayouts.size()` = 1 para el grid).
      - **UBO**: `@binding(0) @group(0)` correcto ✓.
      - **Vertex inputs**: Slang asigna las locations desde las SEMANTIC
        (POSITION0/TEXCOORDN/COLOR0), no del `vk::location` → salen
        `start=0, end=5, color=6, cornerX=1, cornerY=2, width=3, spacing=4`.
        Hay que reordenar a 0..6 en orden de declaración (lo que el C++
        `GetAttributeDescriptions` provee).
      - **VSOutput/PSInput**: locations 0-4 correctos ✓.
      - El math del fog y los push constants quedaron intactos en el export ✓.
- [x] **Fase 1 — Generar el WGSL del grid desde `.slang`** en el arranque del
      editor (junto a `WriteRuntimeSidecars`), con post-procesado:
      - entry `fn main(` → `fn vs_main(` / `fn ps_main(`;
      - push uniform anotado `@group(1) @binding(0)` (layout del backend);
      - reordenar locations del `vertexInput_*` a 0..6 en orden.
      **HECHO (2026-08-21)**: `ShaderExporter::WriteRuntimeWebGpuShaders` +
      helpers en ShaderExporter.cpp, cableado en main.cpp OnInit. Log verificado:
      `[WebGPU] grid WGSL 2/2`, grid pipeline OK, stderr vacío. WGSL generado
      leído y confirmado (push `@group(1)@binding(0)`, inputs 0..6, `vs_main`/
      `ps_main`, fog intacto).
- [x] **Fase 1b — Bug de convención de matriz en WGSL (encontrado en la
      verificación)**: Slang emite `vector * matrix` (fila-vector) para el
      `matrix * vector` lógico porque su target WGSL guarda las matrices
      transpuestas. Con nuestro UBO GLM column-major la matriz reconstruida es
      M, así que `v * M` = **M^T · v** → proyección transpuesta → el grid salía
      como "rectángulos sólidos apuntando para todos lados". Fix:
      `FixWgslMatrixMultiply` (intercambia a `matrix * vector` = M·v correcto).
      Verificado por el usuario: grid OK en wgpu-native.
- [x] **Fase 2 — Limpieza**: borrados `engine/shaders/Grid.vert.wgsl` +
      `Grid.frag.wgsl` hand-written y su copia del CMake (`WGSL_SOURCES`). El
      grid ahora se genera solo desde `.slang`.
- [x] **Fase 3 — Verificar**: **HECHO por el usuario (2026-08-21)**: grid en
      wgpu-native se ve bien (líneas con fog). Vulkan/D3D12 no se afectan
      (no usan WGSL).
- [x] **Fase 4 — Docs**: `TODO_GRID_LOD_DISTANCE_FADE.md` (deuda: grid YA
      cableado, queda el resto de shaders) y `AGENTS.md` (entrada 2026-08-21)
      actualizados. `TODO_WEB_EXPORT.md` no aplica (el grid no está en el
      export web; los `.web.wgsl` de Basic/Sprite/UI siguen a mano hasta la
      extensión a todos los shaders).

**ESTADO: Grid single-source COMPLETO** (todas las fases del grid cerradas).

## Etapa A (2026-08-21) — Gizmo single-source (HECHA)

- [x] `WriteRuntimeWebGpuShaders` ahora genera `Gizmo.vert/frag.wgsl` desde el
      `.slang` (misma post-procesado que el grid: `vs_main`/`ps_main`, push
      `@group(1)@binding(0)`, inputs 0..5 reordenados, `FixWgslMatrixMultiply`).
      Gizmo.frag es passthrough (sin push) → solo renombra la entry.
- [x] Borrados los `Gizmo.vert/frag.wgsl` hand-written + su copia del CMake.

## Etapa B (2026-08-21) — Basic/Sprite/UI single-source con `#ifdef LEIR_BINDLESS` (HECHA)

- [x] `#ifndef LEIR_BINDLESS / #define LEIR_BINDLESS 1` + bloques `#if LEIR_BINDLESS`
      en `Basic.frag`/`Sprite.frag`/`UI.frag` (default bindless → Vulkan/D3D12
      no cambian; `LEIR_BINDLESS=0` → textura única para web).
- [x] **Defines en Compile**: `IShaderCompiler::Compile`/`CompileFromSource` ganan
      `macroDefines` ("NAME=VALUE;..."); `SlangShaderCompiler::CreateSession` los
      parsea a `PreprocessorMacroDesc`.
- [x] `WriteRuntimeWebGpuShaders` reescrito: genera **los 5 pares**
      (Grid/Gizmo/Basic/Sprite/UI) con `LEIR_BINDLESS=1`, **push group por
      shader derivado de la reflection** (conteo de sets: Basic=2, UI/Sprite/
      Grid/Gizmo=1), y post-procesado por etapa.
- [x] **Fix `FixWgslBindless`**: Slang emite el bindless como `array<texture_2d
      <f32>>` (inválido en WGSL/naga) → se convierte a `binding_array<..., 16>`
      (kBindlessMax del backend).
- [x] **Fix `AnnotateWgslPush`**: generalizado — anota cualquier `var<uniform>`
      global sin `@group` (UI usa `screenSize_0`, no `push_0`).
- [x] Borrados los `Basic/Sprite/UI.*.wgsl` hand-written + el bloque de copia
      WGSL del CMake (ya no hay `.wgsl` a copiar; el runtime los genera).
- [x] **Verificado por el usuario (2026-08-21)**: editor en wgpu-native con los
      10 shaders generados (Basic/Sprite/Grid/Gizmo/UI) — "todo perfecto", sin
      errores de validación wgpu, el usuario pudo volar la cámara.

## Pendiente futuro

- [ ] **Etapa C — export web (`.web.wgsl`) generado (opción a)**:
      - [x] **Generador HECHO**: `ShaderExporter::WriteWebShaders` genera
            Basic/Sprite/UI `.web.wgsl` desde el `.slang` con `LEIR_BINDLESS=0`
            + post-procesado (entry, push group, textura única, matriz). El
            editor los genera al arrancar a `LEIR_SHADER_DIR`. Verificado:
            `[WebGPU] web WGSL 6/6`, salida correcta (textura única, sin
            binding_array).
      - [ ] **Integración del build web**: el WebEngineDemo preloada
            `engine/shaders@/shaders` (aún los `.web.wgsl` hand-written). Falta
            que el build web use los GENERADOS: o (a) preloadar el dir generado
            + generarlos en el build (tool host + slangc), o (b) commitear los
            generados en `engine/shaders` con check de drift en CI. Decidir.
      - [ ] Borrar los `.web.wgsl` hand-written de `engine/shaders`.
      - [ ] Verificar el export web (emscripten, WebEngineDemo/WebDemo).

## Archivos relevantes

- `engine/shaders/*.slang` — fuente única de todos los shaders (Grid/Gizmo/
  Basic/Sprite/UI). Los `.wgsl` native ya NO existen (se generan al arrancar);
  los `.web.wgsl` hand-written quedan hasta la Etapa C.
- `editor/src/Shaders/ShaderExporter.cpp` — `WriteRuntimeSidecars`,
  `WriteRuntimeWebGpuShaders` (WGSL native, LEIR_BINDLESS=1) y
  `WriteWebShaders` (`.web.wgsl`, LEIR_BINDLESS=0) + helpers:
  `GenerateWgslPairs`/`RenameWgslEntry`/`AnnotateWgslPush`/
  `RenumberVertexInputLocations`/`FixWgslMatrixMultiply`/`FixWgslBindless`.
- `engine/include/LeirEngine/RHI/IShaderCompiler.h` + `editor/src/Shaders/
  SlangShaderCompiler.cpp` — `macroDefines` en Compile/CompileFromSource.
- `engine/CMakeLists.txt` — sin copia de `.wgsl` (se generan en runtime).
- `engine/src/RHI/WebGPUBackend.cpp` — entry points `vs_main`/`ps_main`
  (línea 1366/1370), push group = `setLayouts.size()` (línea 1480),
  `kBindlessMax = 16`.
- `engine/include/LeirEngine/RHI/WebGPUBackend.h` — `GetShaderFileExtension()`
  → `.wgsl` (native) / `.web.wgsl` (web).