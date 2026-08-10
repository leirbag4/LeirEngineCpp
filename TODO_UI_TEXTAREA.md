# UITextArea Scroll Completo — TODO

Estado: implementar scroll completo en `UITextArea` (offset + scrollbars), manteniendo
caret, cursor, selección y edición intactos.

Complementa `TODO_UI_INPUT.md` (F3.1/F3.3/F3.4) y reutiliza `UIScrollbar` + los
booleans de enable que se agregaron a `ScrollView` en `TODO_UI_SCROLLBARS.md`.

## Contexto / Decisión

- El texto del `UITextArea` se dibuja **directo en `UIRenderer`** (no vía hijos), así que
  el UITextArea lleva su **propio** `m_ScrollOffset`, no un `ScrollView` interno.
- Las scrollbars son **hijos del UITextArea** (`UIScrollbar(true)` vertical +
  `UIScrollbar(false)` horizontal), el mismo patrón que usa `ScrollView`.
- Booleans para habilitar/deshabilitar cada scrollbar (idea del usuario: p. ej. poder
  deshabilitarlas en ciertos contextos).
- NUNCA romper caret/cursor/selección/edición: todo eso ya funciona, solo se desplaza.

## Arquitectura de scrollbars (de ScrollView)

- El **dueño** de las scrollbars es el widget contenedor (`ScrollView`; para el textarea
  será el propio `UITextArea`).
- `UIScrollbar` es un `UIPanel`: track = fondo propio, thumb = `UIPanel` hijo.
- `ScrollView::SyncScrollbar()`: `SetActive(overflow)`, posición con **offsets ABSOLUTOS**
  (TopLeft + bordes), `SetRange(viewport, content)`, `SetValue(off / maxScroll)`.
- `GetViewportSize()` debe restar el espesor de cada barra habilitada del eje
  correspondiente (vertical: ancho; horizontal: alto).
- El callback del ctor mapea value [0,1] → offset: `off.y = v * GetMaxScrollY()` /
  `off.x = v * GetMaxScrollX()`.

## Fase 1 — Scrollbar horizontal en ScrollView (consola) — ✅

Ver `TODO_UI_SCROLLBARS.md` / resumen de cambios:

- [x] `ScrollView.h/.cpp`: `m_HScrollbar = new UIScrollbar(false)` en ctor + callback
  `off.x = v * GetMaxScrollX()`; destruido en dtor.
- [x] `GetViewportSize()`: resta `scrollbarWidth` del ancho (vertical) y el alto de la
  barra horizontal del alto (solo si habilitada).
- [x] `SyncScrollbar()` extendido a ambos ejes (vertical derecha, horizontal abajo,
  `SetActive(overflowX)`, `SetRange(viewportW, contentW)`, `SetValue(off.x / maxX)`).
- [x] Booleans: `SetVerticalScrollbarEnabled(bool)` / `SetHorizontalScrollbarEnabled(bool)`
  + getters. Deshabilitada = no se crea/posiciona ni se reserva espacio.
- [x] Wheel: vertical normal; **Shift+wheel → scroll horizontal** (`Keyboard::IsDown(Shift)`).
- [x] Drag touch-style en ambos ejes (`off.x -= delta.x`).
- [x] Getters `GetVerticalScrollbar()` / `GetHorizontalScrollbar()`.
- [x] Verificado en consola con una línea larga.

## Fase 2 — Scroll offset en UITextArea

**Archivos:** `engine/include/LeirEngine/UI/UITextArea.h`, `engine/src/UI/UITextArea.cpp`,
`engine/src/UI/UIRenderer.cpp`

### F2.1 — Estado + clip

- [x] `m_ScrollOffset` (Vector2) — clamped via `SetScrollOffset`, max computed each layout.
- [x] `SetClip(true)` en el ctor del UITextArea para que el texto que desborda quede
      recortado al rect del widget.
- [x] `GetMaxScrollY()` / `GetMaxScrollX()`:
  - Alto contenido = `lineCount * lineH` (+ padding).
  - Ancho contenido = max over líneas de `GetCursorXAt(GetLineEnd(line))` (o
    `Font::MeasureText` de cada línea).
  - Viewport = rect del widget − espesor scrollbars habilitadas (`GetViewportSize()`).

### F2.2 — Rendering con offset

**Archivo:** `engine/src/UI/UIRenderer.cpp` (bloque UITextInput)

- [x] `textX0 -= scrollOffset.x` y `baselineY -= scrollOffset.y` (para baseline del texto,
      caret `caretY` y quads de selección `sx/ex`).
- [x] La selección multiline ya dibuja un rect por línea; se resta el offset a cada uno.
- [x] El caret usa `GetCursorX`/`GetCursorLine` con el mismo offset aplicado.
- [x] El clip del UITextArea se encarga de no dibujar fuera del rect (scissor).
- [x] El textarea ya **no** aplica line-wrap (`LayoutText` con maxWidth=0); las líneas
      largas se desbordan y salen por el scrollbar horizontal. El texto del `UITextArea`
      por lo tanto ya no hace wrapping a la altura (cambio correcto para scroll real).

### F2.3 — Auto-follow del caret

**Archivo:** `engine/src/UI/UITextArea.cpp`

- [x] Tras cada mutación del cursor (flechas, click, Home/End, Enter, tipeo, selección):
      `EnsureCaretVisible()` ajusta `m_ScrollOffset` para traer la línea/columna activa al
      viewport (con un margen de línea).
- [x] Scroll vertical clásico: cuando el caret baja más allá del fondo visible, desplaza para
      que la línea activa quede visible.

### F2.4 — Scrollbars en el UITextArea

**Archivo:** `engine/include/LeirEngine/UI/UITextArea.h`, `engine/src/UI/UITextArea.cpp`

- [x] Hijos `UIScrollbar(true)` (vertical) + `UIScrollbar(false)` (horizontal), posicionados
      en `OnLayoutComputed` (offsets absolutos como ScrollView).
- [x] Sincronización bidireccional: scrollbar → offset (callback `SetOnScroll`), offset →
      scrollbar (`SetRange` + `SetValue`) en cada layout.
- [x] Booleans `SetVerticalScrollbarEnabled(bool)` / `SetHorizontalScrollbarEnabled(bool)`.
- [x] `OnScroll` override: wheel vertical; Shift+wheel horizontal.

### F2.5 — Integración / verificación

- [x] `TextAreaDebugPanel` (`editor/src/UI/TextAreaDebugPanel.cpp`): se añadió un segundo
      área **read-only** (`SetEditable(false)`) con 40 líneas largas para verificar el wheel /
      scroll / thumb / scrollbar sin que caret o edición interfieran. **Nota 2026-08-08:** el
      área read-only se **eliminó** a petición tras la verificación — el tab "Text Area" quedó
      con solo el UITextArea editable + la etiqueta de estado. El feature `SetEditable` permanece
      en el engine.
- [x] Verificación manual del usuario: wheel, thumb, drag, scrollbars, selección intacta,
      read-only — **todo funciona**.
- [ ] Caso borde: líneas vacías, `\n` finales, scroll X con una línea muy larga.
- [x] Actualizar `TODO_UI_INPUT.md`: marcar F3.1 (scroll offset), F3.3 (render offset) y
      F3.4 (scrollbars UITextArea) como resueltos. (*hecho 2026-08-08, junto con
      `GetSelectedText()` que faltaba en F2.1*)

## Fase 3 — Fix de rendimiento (60 → 10 fps con el panel visible) — ✅

**Problema:** con el área read-only de 40 líneas largas visible, la FPS caía de 60 a 10.
**Causa raíz:** `GetContentSize()` era **O(N²)** — por cada línea llamaba a
`GetLineEnd(line)` (re-escaneo desde el inicio) y a `GetCursorXAt(GetLineEnd(line))`
(otro re-escaneo desde el inicio). Con 40×120 chars eso era ~40 re-escaneos por llamada,
y se ejecutaba ~3-4 veces por frame (`SetScrollOffset` + `SyncScrollbars` dentro de
`OnLayoutComputed`) → ~90M operaciones de char por frame.

**Fix:** `GetContentSize()` reescrito como **single-pass O(N)** sobre `m_Text` — recorre
cada codepoint una sola vez (misma lógica UTF-8/espacio/avance que `GetCursorXAt`),
acumulando el ancho de la línea actual y el máximo sobre todas las líneas, contando el
número de líneas con los `\n` de paso. Se eliminó el per-line `GetLineEnd` + `GetCursorXAt`.

**Archivo:** `engine/src/UI/UITextArea.cpp` (GetContentSize)
**Estado:** ✅ Builds verdes (LeirEngine + LeirEngineEditor). Verificado por el usuario:
FPS estable en 60 con el panel Text Area visible.

## Fase 4 — Word Wrap completo (`SetWordWrap`) — código hecho, build ✅ (verificación visual pendiente del usuario)

**Objetivo:** wrap profesional en `UITextArea` con caret, selección, click/drag,
flechas Up/Down y scroll correctos cuando las líneas visuales no coinciden con las
lógicas (`\n`). Opcional por instancia vía `SetWordWrap(bool)`.

**Implementado:**
- Modelo de filas visuales `std::vector<VisualRow> m_VisualRows` (`{startByte, endByte, width}`),
  reconstruido perezosamente en `EnsureVisualRows()` — compara `m_ModelGen` más
  `m_BuiltWrapWidth != WrapLimit()` (o sea se reconstruye en resize del widget).
- `WrapLimit()` = `max(0, cr.z − stripVertical − 8)`; con wrap off devuelve `FLT_MAX`
  (una fila por línea lógica, comportamiento idéntico al anterior).
- `SetWordWrap(bool)`, `IsWordWrapEnabled()`, invalida modelo + reclamp scroll.
- `SetText`/`SetFont` override + `OnTextMutated` → `InvalidateWrapModel()`.
- `GetLineCount()/GetLineStart()/GetLineEnd()/GetCursorLine()/GetCursorCol()/
  GetCursorXAt()` operan sobre filas visuales (wrap off = lógicas).
- Renderer: rama wrap ON dibuja cada fila con `LayoutText(sub, 0)`. Caret y selección
  por fila visual (offset Y por `line * lineH − scrollY`).
- Navegación: flechas Up/Down sobre filas (preservan `m_TargetX`, scan de columna con
  space-width correcto), `Home`/`End` al start/end de la fila visual.
- `GetContentSize()`: wrap ON → `{viewport.x, filas*lineH + 8}` (sin hscroll),
  wrap OFF → máximo de filas lógicas (single-pass, sin O(N²)).
- Panel `TextAreaWrapPanel` (pestaña "Text Area Wrap") con toggle `SetWordWrap`
  y status (wrap, líneas lógicas vs visuales, cursor).
- Registrado en `main.cpp` + agregado a `kDebugIds` (DockManager).
- **Builds green**: LeirEngine.dll + LeirEngineEditor.exe.

> **Pendiente:** verificación visual del usuario (toggling wrap, click/drag, flechas,
> scroll) antes de considerarlo cerrado.

### F4.1 — Modelo de líneas visuales (núcleo)

Nuevo modelo en `UITextArea`: una caché de filas visuales
`{{startByte, endByte} -> width}` reconstruida perezosamente (O(n) single-pass),
invalidada por generación (`m_ModelGen`) + ancho de wrap cambiado.

- [x] `std::vector<VisualRow> m_Rows` construido por pasada única sobre `m_Text`:
  - El run `\n` cierra la fila actual (una fila vacía también).
  - Wrap activo: antes de que `x+advance > wrapLimit` se corta. Con separador
    (space) previo en la fila → se corta **en la palabra** (la fila termina en el
    space excluido y la nueva fila empieza después de él); sin space → hard break.
  - Wrap apagado: una fila por línea lógica (igual que hoy) → comportamiento
    idéntico al actual.
- [x] `SetWordWrap(bool)` / `IsWordWrapEnabled()` → `m_ModelGen++` + reclamp scroll.
- [x] `EnsureVisualRows()` perezoso (compara `m_ModelGen` + `m_BuiltWrapWidth`);
      también se invalida con `SetText` y cualquier mutación (`OnTextMutated` override).
- [x] `wrapLimit = max(0, cr.z - stripV - 8)` (equivalente a `max(0, GetViewportSize().x - 8)`).

### F4.2 — Funciones de navegación por fila visual

- [x] `int VisualRowOfChar(int byteIdx)` — fila de un índice (índice == fin de fila →
      fila anterior).
- [x] `GetLineCount()/GetLineStart()/GetLineEnd()` pasan a devolver **filas
      visuales** (con wrap off = lógicos, comportamiento idéntico). Usado por
      renderer y flechas.
- [x] `GetCursorLine()` = fila visual del caret; `GetCursorCol()` = byte − start fila.
- [x] `GetCursorXAt(int)` override: X relativa a la **fila** actual del índice
      (reseteo `\n` + filas envueltas). `GetCursorX()` deriva de eso.

### F4.3 — Renderer (UIRenderer.cpp)

- [x] Evaluar `textArea->IsWordWrapEnabled()`: si está ON, dibujar **por fila** — por
      cada fila sacar su sub-texto (rango), `LayoutText(sub, 0)`, y desplazar cada glyph
      a `cr + 4 − scrollX + rowOffsetY` (una pasada por fila). Si OFF, mantener el
      dibujo actual en un solo `LayoutText(text, 0)`.
- [x] Caret / selección ya usan `GetCursorLine`/`GetLineStart/End`/`GetCursorXAt`
      visuales → no requieren cambios extra (verificado al integrar).

### F4.4 — Pointer (click/drag) por fila visual

- [x] `OnPointerDown`/`OnPointerMove`: `row = (localY + scroll.y − padTop)/lineH` clamped
      a filas visuales; columna = `byteAt(row, localX)` (rama idéntica a la actual pero
      con start/len de la fila) — `GetLineStart/End(line)` ya devuelven filas visuales.

### F4.5 — Flechas Up/Down + Start/End de fila

- [x] `OnKeyDown` Up/Down sobre filas visuales (preserva `m_TargetX`).
- [x] `Home` → inicio de la fila visual. `End` → fin de la fila visual (no de toda la
      línea lógica). (Wrap off: fila = lógica, igual que hoy.)

### F4.6 — Scroll/EnsureCaretVisible/Content

- [x] `EnsureCaretVisible()` usa la fila visual del caret.
- [x] `GetContentSize()`: con wrap ON, ancho = viewport (sin hscroll) y alto =
      filas × lineH; con wrap OFF, máximo de líneas lógicas (single-pass actual).
- [x] Scrollbar horizontal se oculta automáticamente con wrap ON (content.x ≈ vp.x).

### F4.7 — Panel editor "Text Area Wrap"

- [x] `editor/src/UI/TextAreaWrapPanel.h/.cpp` (al estilo del TextAreaDebugPanel):
      área editable con texto largo + botón toggle `SetWordWrap(on/off)` + status
      labels. Título de la pestaña **"Text Area Wrap"**.
- [x] Registrar en main.cpp + agregarlo a `kDebugIds` de `DockManager::BuildDefaultLayout`.

## Propiedad Editable (UITextInput / UITextArea)

Nuevo en el mismo paso (pedido del usuario antes de empezar Fase 2):

- `bool IsEditable() const` / `void SetEditable(bool)` en `UITextInput` (base, heredado por
  `UITextArea`).
- `false` = read-only textbox: se conserva el scroll (wheel/scrollbars/drag), pero:
  - `OnPointerDown` devuelve `false` → no se coloca caret ni focus.
  - `OnKeyDown` / `OnTextInput` / `InsertChar` guardan `m_Editable` → no edición de nada
    (ni teclado ni mouse ni selección/drag).
  - `OnFocus` sólo marca `m_Focused` si `m_Editable`; `IsCaretVisible()` requiere editable.
  - `SetEditable(false)` además limpia focus/captura del canvas y la selección/drag.

## Estado general

| Tarea | Estado |
|-------|--------|
| Fase 1 — Scrollbar horizontal en ScrollView/consola | ✅ |
| F2.1 — Estado + clip | ✅ |
| F2.2 — Rendering con offset | ✅ |
| F2.3 — Auto-follow del caret | ✅ |
| F2.4 — Scrollbars UITextArea | ✅ |
| F2.5 — Integración / verificación | ✅ |
| Fase 3 — Fix de rendimiento (O(N²) → O(N)) | ✅ |
| Fase 4 — Word wrap completo (`SetWordWrap`) | ✅ build (verificación visual pendiente) |
