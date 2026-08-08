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
      scroll / thumb / scrollbar sin que caret o edición interfieran.
- [x] Verificación manual del usuario: wheel, thumb, drag, scrollbars, selección intacta,
      read-only — **todo funciona**.
- [ ] Caso borde: líneas vacías, `\n` finales, scroll X con una línea muy larga.
- [ ] Actualizar `TODO_UI_INPUT.md`: marcar F3.1 (scroll offset), F3.3 (render offset) y
      F3.4 (scrollbars UITextArea) como resueltos.

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
