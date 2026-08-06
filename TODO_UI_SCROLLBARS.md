# TODO_UI_SCROLLBARS.md — Clipping + Scrollbars (engine)

Fecha: 2026-08-05 · Estado: ✅ implementado

## Problema

- `UIRenderer` no tenía clipping: todo quad se dibujaba sin scissor, así que el
  contenido de un scroll se desbordaba del panel (grep inicial de
  `Scissor|VK_DYNAMIC_STATE|vkCmdSetScissor` en `UIRenderer.cpp` sin resultados).
- `ScrollView` existía solo con drag-to-scroll, sin clamps, sin scrollbar, sin
  rueda del mouse, y posicionaba el contenido en coords relativas (bug latente:
  faltaba la posición global del propio ScrollView).
- No había dispatch de `ScrollEvent` al UI (solo actualizaba el polling de `Mouse`).
- `UITextArea` tampoco tenía scroll offset (ver `TODO_UI_INPUT.md` F3.1/F3.3).

## Decisión

Hacer bien el cimiento en el engine: **clipping por scissor por nodo** +
**scrollbar real** + **wheel en el UI**. La consola del editor (y futuros
ScrollView/UITextArea/HierarchyPanel/ProjectPanel) se compone sobre estas
primitivas en vez de hackear un widget con auto-clip en el editor.

## Implementación

### 1. Clipping — `UIElement::SetClip(bool)` + scissor en `UIRenderer`

- `UIElement` gana `SetClip(bool)` / `IsClipEnabled()` (`m_Clip`). El rect
  computado del elemento pasa a ser región de clip para sus descendientes.
- `UIRenderer::Render` convierte el walk iterativo en **recursivo**
  (`RenderElement(elem, clip, isDebug)`):
  - Al entrar a un elemento con clip → `newClip = elemRect ∩ clipActivo`
    (absoluto). Si queda vacío → **culling de todo el subárbol**.
  - Elementos no-clip pero totalmente fuera del clip activo → cull rápido.
  - `m_CurrentClip` se aplica en `BuildBatch`/`BuildBatchDebug` a los arrays
    paralelos `m_QuadClips` / `m_DebugQuadClips`; `ViewportDraw` gana `clip`.
- `Flush()`: cada quad ya era un `vkCmdDraw` individual, así que antes de cada
  draw se aplica `ApplyScissor()` (solo llama `vkCmdSetScissor` si el rect
  cambió vs el draw anterior — barato). Los 3 loops (UI regular → viewports →
  debug overlay) comparten `lastScissor`.
- El scissor es **físico** (framebuffer del overlay pass), los rects de clip son
  **lógicos** → `UIRenderer::SetContentScale(float)` (default 1.0) y
  `scissor = clip × contentScale`, con clamp al framebuffer.
- El pipeline de UI ya declaraba `VK_DYNAMIC_STATE_VIEWPORT + SCISSOR`
  (`VulkanDevice::CreateGraphicsPipeline`), así que `vkCmdSetScissor` estaba
  disponible sin tocar el pipeline.

### 2. Wheel — `UIElement::OnScroll(float)` + dispatch

- `UIElement::OnScroll(float delta)` virtual (default `false` = no consume).
- `EventQueue` ya tenía `ScrollHook`; `UICanvas::ConnectToInputSystem` ahora lo
  registra → `ProcessScrollEvent(const ScrollEvent&)`: propaga el scroll al
  elemento hovered subiendo por la cadena de padres hasta que alguien consume.

### 3. `UIScrollbar` (nuevo, engine)

`engine/include/LeirEngine/UI/UIScrollbar.h` + `src/UI/UIScrollbar.cpp`:
- `UIScrollbar : UIPanel` — el track es el propio fondo; el thumb es un `UIPanel`
  hijo posicionado en `OnLayoutComputed` (composición de primitivas → **sin rama
  nueva en UIRenderer**).
- `SetRange(viewport, content)`, valor normalizado `[0,1]`, `SetValue/GetValue`,
  `SetOnScroll(cb)`. Cuando el contenido cabe → thumb = track y valor clamp 0.
- Drag del thumb con `CapturePointer` (patrón `UIDragFloatInput`): click en
  thumb = arrastrar con grab offset; click en track = saltar centrando el thumb.
- Orientación vertical/horizontal (`GetMinSize` 10px de ancho).

### 4. `ScrollView` rework

`engine/include/LeirEngine/UI/ScrollView.h` + `src/UI/ScrollView.cpp`:
- `SetClip(true)` en el ctor → el contenido queda scissoreado al rect del view.
- Contenido posicionado en **coords absolutas** (arregla el bug de posición
  relativa: offset = `cr.x/y - scrollOffset`; el signo correcto es "positivo =
  contenido movido arriba", ver Bug fixes abajo).
- `SetScrollOffset` con **clamps** a `[0, maxScroll]`
  (`maxScrollY = contentSize.y - viewport.y`; contentSize = `GetContentSize()`
  natural del contenido, viewport = rect del view).
- `OnScroll(delta)` (rueda): `offset.y -= delta × lineHeight`, consume si hay
  overflow.
- Drag-to-scroll con `CapturePointer` (solo si hay overflow).
- **Scrollbar vertical integrado** (`m_VScrollbar` hijo, ancho configurable):
  `SyncScrollbar()` en `OnLayoutComputed` → `SetActive(overflow)`, `SetRange`,
  `SetValue(offset/max)`, y el callback del scrollbar ajusta el offset (con
  re-aplicación del contenido tras el sync). Feedback bidireccional sin loop
  (el `SetValue` del scrollbar corta cuando el valor no cambia).

### 5. `LogMessage` (de paso, para la consola)

- `LogMessage` gana `time` (`HH:MM:SS.mmm`, ya lo calculaba `Timestamp()`).
- El ring buffer y `XConsole::GetVersion()` retienen **solo Info/Warning/Error**
  (Trace/Debug son debug-only y evictaban los mensajes útiles). Doc-comment
  actualizado en `Log.h`.

## Archivos

**Nuevos**: `engine/include/LeirEngine/UI/UIScrollbar.h`, `engine/src/UI/UIScrollbar.cpp`.
**Modificados**: `UIElement.h` (SetClip/OnScroll), `UIRenderer.h/.cpp` (scissor),
`UICanvas.h/.cpp` (scroll dispatch), `ScrollView.h/.cpp`, `Log.h/.cpp` (time + buffer),
`engine/CMakeLists.txt`, `TODO.md`, `TODO_UI_INPUT.md` (F3.4).

## Verificación

- Build completo (DLL + editor + PhysicsDemo) sin errores.
- Corrida del editor con la consola como tab activa: sin crash, stderr vacío,
  sin VUID de validación.
- `[Console] rebuilt N lines` confirma que el panel construye líneas.

## Bug fixes 2026-08-06 (usuario: consola en columna izquierda)

Tres síntomas compartían causa raíz en `engine/src/UI/ScrollView.cpp`:

1. **Track del scrollbar con alto incorrecto y que se achicaba al redimensionar.**
   `SyncScrollbar()` posicionaba el scrollbar con `anchor={1,0,1,1}` + offsets
   *relativos* (`-(w+2), 2, -2, -2`). Eso **sobrescribía** la posición absoluta
   que el padre ya había propagado (`ComputeFreeLayout` suma `m_ComputedRect.x/y`
   a cada hijo), dejando el rect del scrollbar en coordenadas relativas al origen.
   Como el ScrollView tiene `SetClip(true)`, el track quedaba recortado contra el
   rect del ScrollView → el trozo visible no cubría el alto del contenedor y se
   achicaba al encoger la consola. Fix: `AnchorSet::TopLeft` + offsets absolutos
   (`{cr.x + cr.z - w - 2, cr.y + 2, cr.x + cr.z - 2, cr.y + cr.w - 2}`), igual
   que el contenido. Ahora el track cubre todo el alto y acompaña el resize.
2. **Drag invertido (tipo touch).** El contenido se posicionaba en
   `cr.y + m_ScrollOffset.y`, pero el header documenta "positive = content moved
   up". Signo invertido: arrastrar abajo empujaba el contenido abajo (espacio
   vacío arriba + lock arriba), y la primera línea no podía salir por arriba.
   Fix: `cr - m_ScrollOffset` en los 2 bloques de `OnLayoutComputed` y drag
   `off.y = m_ScrollStart.y - delta.y` (el contenido sigue al dedo). Ahora:
   arrastrar abajo lockea arriba sin espacio vacío; arrastrar arriba clipea la
   primera línea y lockea al final con espacio vacío abajo.
3. **Thumb invertido.** `SetOnScroll` → `off.y = v * GetMaxScrollY()`. Con el
   signo viejo, thumb abajo (v↑) → offset↑ → contenido abajo. Se arregló solo
   con el punto 2 (thumb abajo → contenido arriba). Sin cambios en
   `UIScrollbar.cpp`.

La rueda (`off.y -= delta * lineHeight`) quedó correcta con el signo arreglado
del contenido. Verificado por el usuario en el editor.

## Roadmap

- Scrollbar **horizontal** (`UIScrollbar` ya soporta orientación; falta integrar
  en `ScrollView`).
- `UITextArea` scroll offset vertical (ya hay clipping: solo falta el offset +
  scrollbar).
- Política de scroll por elemento (wheel passthrough cuando el scroll está en el
  límite → permite anidar ScrollViews).
- Estilos de scrollbar (thumb con hover/pressed states, ancho configurable por
  skin).
- Pooling de rects de clip en `Flush` si algún día se bachea por textura.
