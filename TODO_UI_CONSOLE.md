# TODO_UI_CONSOLE.md — ConsolePanel del editor

Fecha: 2026-08-05 · Estado: ✅ implementado (v1)

## Objetivo

Consola de logs estilo Unity dockeable en el editor: mensajes en tiempo real de
`XConsole`, filtros Info/Warn/Error, Clear, timestamps, scroll con rueda y
scrollbar, auto-follow al fondo. Apoyado en la infraestructura nueva del engine
(clipping por scissor + `UIScrollbar` + wheel dispatch) — ver
`TODO_UI_SCROLLBARS.md`.

## Decisiones (aprobadas)

1. **Composición de primitivas del engine**, no widget nativo con self-clipping:
   `ConsolePanel` = `UIPanel` (Column) → header (Row de `UIButton`) + `ScrollView`
   (con scrollbar integrado) cuyo contenido es un Column de `UILabel`.
2. **Wheel del mouse en la v1**: `UIElement::OnScroll(float)` + dispatch del
   `ScrollEvent` al hovered en `UICanvas`.
3. **Timestamp por mensaje**: `LogMessage.time` (`HH:MM:SS.mmm`) mostrado como
   prefijo `[time] ` en cada línea.
4. **Solo Info/Warning/Error**: Trace/Debug no se muestran. El ring buffer de
   `XConsole` retiene únicamente Info/Warn/Error (decisión de diseño: los
   debug-only evictaban los mensajes útiles del buffer de 1000).

## Implementación

### Engine
- `Log.h/.cpp`: `LogMessage.time`; `XConsole::GetVersion()` (contador monótono
  que bumpea solo con Info/Warn/Error emitidos o `Clear()`) para que el panel
  detecte mensajes nuevos **sin** copiar el buffer cada frame.

### Editor (`editor/src/UI/ConsolePanel.h/.cpp`)
- Estructura:
  ```
  ConsolePanel (UIPanel, Column)
  ├── ConsoleHeader (UIPanel, Row, SizePolicy=Content)
  │     ├── [Info] [Warning] [Error]  ← toggles con color activo/inactivo
  │     └── [Clear]  ← XConsole::Clear()
  └── ConsoleScrollView (ScrollView, Fill)
        └── ConsoleLines (UIElement, Column)  ← 1 UILabel por mensaje
  ```
- Líneas: `"[HH:MM:SS.mmm] texto"`, color por nivel (Info gris claro, Warning
  amarillo, Error rojo), `SizePolicy::Fixed`, cap `kMaxLines = 300`.
- **Rebuild lazy** en `Refresh()` (llamado cada frame): solo reconstruye cuando
  `XConsole::GetVersion()` cambia o cambia el filtro (`m_FilterStamp`).
- **Auto-follow**: si antes del rebuild el offset estaba en el fondo
  (`offset.y >= maxScrollY - 1`), se mantiene en el fondo tras reconstruir.
- `ClearHoverIfInside()`: antes de destruir la columna de líneas limpia
  hover/focus del canvas si el hovered está dentro del panel (evita punteros
  colgantes al borrar labels en vivo).
- **Teardown**: `m_LineColumn` se libera vía `DeleteUiSubtree(m_ConsolePanel)`
  del editor (el dtor del panel NO borra la columna → evita double-free).

### Integración (`editor/src/main.cpp`)
- Creación + `SetFont(m_FontSmall.get())`, `RegisterPanel("ConsolePanel",
  "Console", m_ConsolePanel, true)` (closeable).
- `Refresh()` por frame en `OnUpdate`.
- `DeleteUiSubtree(m_ConsolePanel)` en `OnShutdown`.
- `DockManager::BuildDefaultLayout`: `"ConsolePanel"` agregado a `kDebugIds` →
  va al pane debug (Pane:Debug) en layouts default.
- Layouts persistidos viejos: `PlaceMissingPanels` lo agrega como tab (activa)
  del primer pane; el usuario lo reubica con drag y queda persistido.
- `UIRenderer::SetContentScale(GetContentScale())` en `OnInit` y
  `OnContentScaleChanged` (el scissor de clip es físico).

## Verificación

- Build completo (DLL + editor + PhysicsDemo) sin errores.
- Corrida del editor (12s, consola activa): `[Console] rebuilt 21 lines
  (info=true warning=true error=true)` — el panel construye líneas Info/Warn/Error.
- Sin crash, stderr vacío, sin VUID de validación.
- Detección de bug: en la primera iteración la consola mostraba Trace/Debug y
  llegaba a 160 líneas; se corrigió filtrando Trace/Debug y reteniendo solo
  Info+ en el buffer → el panel reconstruye una sola vez tras startup.
- **Bug posterior arreglado (2026-08-06)**: los `[UIEvent]` se emitían a nivel
  Info → la consola reconstruía en cada movimiento del mouse (auto-scroll + FPS
  60→20 + glitches por overflow del UIRenderer). Los `[UIEvent]` ahora son
  `Trace` y `Flush()` trunca de forma no destructiva — ver
  `TODO_UI_EVENT_FLOOD.md`. Verificado: `[Console] rebuilt` **1 total** en todo
  un sweep del mouse (antes 1 por pointer event).
- **Bug del flash al resize de docksplitters (2026-08-06)**: arrastrar un
  splitter hacía parpadear el texto (horizontal: flash al soltar con
  "Settings Saved"; vertical: texto invisible hasta soltar, titilando). Causa:
  `Refresh()` corría **después** de `UpdateLayout()` en `OnUpdate` → los labels
  recién creados por `RebuildLines` tenían `m_ComputedRect = {0,0,0,0}` y el
  clip del `ScrollView` los cullaba (intersección 0 → `return`) → **1 frame
  vacío por rebuild**. Fix: mover `Refresh()` **antes** de `UpdateLayout()`
  (main.cpp) → los labels nuevos quedan con layout el mismo frame. Además el log
  `"Viewport resized"` de `UpdateViewportRenderTarget()` era Info **por frame**
  durante un drag vertical (rebuild cada frame) → ahora está **debounced**
  (`m_PendingW/H` + `m_LastLoggedW/H`): un drag entero emite 0 mensajes y
  exactamente 1 al estabilizarse. Verificado por el usuario.

## Roadmap

- **Wrap de líneas largas** (reemplazar por truncado a `maxWidth` del label).
- **Pausa** (freeze del scroll mientras está activa).
- **Toggle Trace/Debug** en la consola (requiere que el buffer los retenga — hoy
  se desechan; se puede añadir un `SetRetainLevel`).
- **Toggle auto-follow**.
- **Búsqueda/filtro por texto** (estilo Unity).
- **Pooling de labels** (reusar nodos en vez de destroy/recreate por rebuild).
- **Doble click para copiar / menú contextual**.
- Console `ConsolePanel` ya no es solo "output": futuro `CommandLine` (ejecutar
  comandos del editor en el mismo panel, estilo Unity Console).
