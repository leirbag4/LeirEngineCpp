# TODO_UI_OPTIMIZATIONS.md — Optimizaciones UI (core)

Fecha: 2026-08-06 · Estado: 🔧 en progreso (A y B terminados, C pendiente)

## Contexto

El usuario confirmó: con muchas líneas de texto en la consola los FPS caen de
forma brutal (~10 FPS), y el texto fuera del rango visible sigue frenando la
app. Diagnóstico completo en `TODO_CONSOLE_PERF.md`.

**Importante**: los 3 fixes apuntan al **core del engine** (`engine/`), no al
editor. El `ConsolePanel` fue solo el caso de prueba. Cuando un usuario final
escriba su propia UI (HUD de juego, UI en RenderTexture, apps), usará los mismos
`UILabel`, `ScrollView`, `UIRenderer` → los fixes benefician a toda UI del motor.

## Fix A — Cache del tamaño natural del label

### Problema
`UILabel::GetMinSize()` → `MeasureText()` (UILabel.cpp:20) corría por **cada**
label, **cada frame**, en `UpdateLayout()` (vía `ScrollView::OnLayoutComputed` →
`ComputeColumnLayout` → `GetNaturalSize`), aunque la línea estuviera fuera del
viewport. Es O(total de chars) por frame sin culling.

### Solución (✅ aplicada 2026-08-06)
Cachear el tamaño medido en `UILabel`:
- `m_CachedSize` (Vector2) + flag `m_SizeValid`.
- Invalidado por `MarkDirty()` (que ya llaman `SetText`, `SetFont`,
  `SetFontSize`, `SetMaxWidth`, `SetWordWrap`, `SetAlignment`).
- `GetMinSize()` devuelve el cache sin medir (O(1)) la primera vez que está
  válido; solo mide de nuevo cuando algo invalida.

### Checklist
- [x] Campo `m_CachedSize` + flag en `UILabel`
- [x] Invalidación en `SetText`/`SetFont`/`SetFontSize`/`SetMaxWidth`/`SetWordWrap`
- [x] `GetMinSize()` usa el cache
- [x] Verificado: con 300 líneas el layout no mide nada por frame (build OK)

## Fix B — Batch de draw calls en UIRenderer

### Problema
`Flush()` hacía **1 `vkCmdDraw` por quad** con `vkCmdSetScissor` + bind de
descriptor set. Con ~20-30 líneas visibles × ~50 chars ≈ 1.000-1.500 draw calls
solo de la consola por frame. CPU-bound en Vulkan.

### Solución (✅ aplicada 2026-08-06)
Los quads ya están contiguos en el vertex buffer. `Flush` ahora agrupa por
(textura, scissor):
- Nuevo helper `GetOrCreateDesc(Texture2D*)` (deduplica el código de
  asignación/cache de descriptor sets).
- `pushQuad` lambda: mismo descriptor + mismo scissor que el batch actual →
  extiende el batch; si cambia → `flushBatch()` (un `vkCmdDraw`), setea scissor
  (solo si cambió, via `ApplyScissor`) y bind de descriptor (solo si cambió).
- Aplica a las 3 capas en un solo loop (regular / viewport / debug), reutilizando
  el estado de batch entre capas.
- **Bug encontrado y corregido (glitch gráfico, verificado por el usuario)**:
  batching con `TRIANGLE_STRIP` conectaba el último vértice de un quad con el
  primero del siguiente → triángulos diagonales basura por toda la pantalla, y
  un triángulo que seguía al cursor al escribir (el caret se batchaba con el
  texto vecino). Fix: el buffer intercalado usa **6 slots por quad** (4 vértices
  + 2 vértices degenerados que repiten el último del quad actual y el primero
  del siguiente), rompiendo el strip entre quads. Cada batch dibuja
  `count*6 - 2` vértices desde `quadIdx*6`.
- Stats por frame expuestas en `UIRenderStats { quads, vertices, drawCalls,
  batches }` via `GetLastStats()`.

### Checklist
- [x] Batch en la capa regular UI
- [x] Batch en la capa debug overlay
- [x] Contador de draw calls reales expuesto para las stats (`m_LastStats`)
- [x] Véaseguros: build OK, editor corre sin VUID
- [x] Fix de strip bridges con vértices degenerados (glitch reportado por el usuario)
- [ ] (pendiente de verificación del usuario) Visual correcto + FPS alto con la consola llena

## Fix C — Culling de layout por líneas visibles (futuro)

### Problema
`ComputeColumnLayout` y `GetContentSize` recorren **todos** los hijos del content
column cada frame (ScrollView.cpp:88, UIElement.cpp:82), incluso fuera de rango.
Con A + B probablemente no se necesite para ~300 líneas; necesario solo para
listas de miles de items.

### Solución
Skip del `ComputeColumnLayout` de labels fuera del viewport. Más frágil: los
labels fuera aportan al `GetContentSize` del scroll (scrollbar). Viable solo si
A+B no alcanzan o para ScrollView con virtualización.

### Checklist
- [ ] (futuro) Evaluar si hace falta tras A+B
- [ ] (futuro) Virtualización del ScrollView si es necesario

## Archivos clave

- `engine/src/UI/UILabel.h/.cpp` — GetMinSize → MeasureText (Fix A)
- `engine/src/UI/Font.h/.cpp` — MeasureText/LayoutText (costo por char)
- `engine/src/UI/UIRenderer.cpp` — Flush (1 draw/quad) + RenderElement (cull) (Fix B)
- `engine/src/UI/ScrollView.cpp` — OnLayoutComputed (layout completo) (Fix C)
- `engine/src/UI/UIElement.cpp` — ComputeColumnLayout / GetContentSize
- `engine/src/UI/UIDebugOverlay.h/.cpp` — stats (DrawCalls, batches, memoria)
- `editor/src/UI/ConsolePanel.cpp` — RebuildLines (caso de prueba)
