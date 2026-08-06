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

- [ ] `m_ScrollOffset` (Vector2, al menos Y; X opcional si hay líneas largas).
- [ ] `SetClip(true)` en el ctor del UITextArea para que el texto que desborda quede
      recortado al rect del widget.
- [ ] `GetMaxScrollY()` / `GetMaxScrollX()`:
  - Alto contenido = `lineCount * lineH` (+ padding).
  - Ancho contenido = max over líneas de `GetCursorXAt(GetLineEnd(line))` (o
    `Font::MeasureText` de cada línea).
  - Viewport = rect del widget − espesor scrollbars habilitadas.

### F2.2 — Rendering con offset

**Archivo:** `engine/src/UI/UIRenderer.cpp` (bloque UITextInput, ~547-592)

- [ ] `textX0 -= scrollOffset.x` y `baselineY -= scrollOffset.y` (para baseline del texto,
      caret `caretY` y quads de selección `sx/ex`).
- [ ] La selección multiline ya dibuja un rect por línea; hay que restar el offset a cada uno.
- [ ] El caret debe seguir usando `GetCursorX`/`GetCursorLine` con el mismo offset aplicado.
- [ ] El clip del UITextArea se encarga de no dibujar fuera del rect (scissor).

### F2.3 — Auto-follow del caret

**Archivo:** `engine/src/UI/UITextArea.cpp`

- [ ] Tras cada mutación del cursor (flechas, click, Home/End, Enter, tipeo, selección):
      si el caret sale del viewport → ajustar `m_ScrollOffset` para traerlo adentro
      (línea arriba/abajo, y columna izquierda/derecha si hay scroll X).
- [ ] Con scroll vertical clásico: cuando el caret baja más allá del fondo visible,
      desplazar para que la línea activa quede visible (mínimo una línea de margen).

### F2.4 — Scrollbars en el UITextArea

**Archivo:** `engine/include/LeirEngine/UI/UITextArea.h`, `engine/src/UI/UITextArea.cpp`

- [ ] Hijos `UIScrollbar(true)` (vertical) + `UIScrollbar(false)` (horizontal), posicionados
      en `OnLayoutComputed` (offsets absolutos como ScrollView).
- [ ] Sincronización bidireccional: scrollbar → offset (callback `SetOnScroll`), offset →
      scrollbar (`SetRange` + `SetValue`) en cada layout.
- [ ] Booleans `SetVerticalScrollbarEnabled(bool)` / `SetHorizontalScrollbarEnabled(bool)`
      (reutilizar API de ScrollView).
- [ ] `OnScroll` override: wheel vertical; Shift+wheel horizontal.

### F2.5 — Integración / verificación

- [ ] `DebugTextPanel` (`editor/src/UI/DebugTextPanel.cpp`): el UITextArea ya está ahí —
      escribir/pegar muchas líneas y verificar que el caret sigue al texto, las scrollbars
      aparecen con overflow, wheel/drag/thumb funcionan, y selección/edición no se rompen.
- [ ] Caso borde: líneas vacías, `\n` finales, scroll X con una línea muy larga.
- [ ] Actualizar `TODO_UI_INPUT.md`: marcar F3.1 (scroll offset), F3.3 (render offset) y
      F3.4 (scrollbars UITextArea) como resueltos.

## Estado general

| Tarea | Estado |
|-------|--------|
| Fase 1 — Scrollbar horizontal en ScrollView/consola | ✅ |
| F2.1 — Estado + clip | ⏳ |
| F2.2 — Rendering con offset | ⏳ |
| F2.3 — Auto-follow del caret | ⏳ |
| F2.4 — Scrollbars UITextArea | ⏳ |
| F2.5 — Integración / verificación | ⏳ |
