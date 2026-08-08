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

## Plan 2026-08-06 — Clip escopado al viewport de contenido (fix texto bajo scrollbars)

### Estado del bug (usuario verificó visualmente)

El scrollbar horizontal ya funciona (drag/rueda/thumb/esquina), pero al aparecer
la scrollbar vertical:

1. Las **últimas letras** de cada línea quedan tapadas por la barra vertical.
2. Se ve una **línea finita** por donde pasan las letras debajo de la scrollbar
   horizontal y al costado de la vertical.

### Causa raíz (confirmada en código)

- La layout YA reserva el espacio de las barras: `GetViewportSize()` devuelve
  `W − scrollbarWidth` (y alto análogo), y `OnLayoutComputed` posiciona el
  contenido con `layoutW = max(availW, GetContentSize().x)`. Es decir, el diseño
  es **Modelo A (reserva de espacio)**.
- Pero el **clip no está escopado**: `ScrollView` tiene `SetClip(true)` y su rect
  de clip es el rectángulo COMPLETO (`W × H`). `UIRenderer::RenderElement` usa ese
  rect como scissor para todos los hijos (contenido Y barras, que hoy comparten el
  mismo padre).
- Consecuencia: en overflow horizontal el texto se extiende hasta `W` (no hasta
  `W − barra`) → pasa por debajo de la barra vertical opaca (letras tapadas), y las
  barras tienen un **inset de 2px** (`SyncScrollbar` usa `- 2.0f`) que deja un
  hueco en el borde donde el texto se filtra → la "línea finita".

No es una optimización: es una inconsistencia entre layout (reserva) y clip (no
escopado). Ver explicación de modelos A/B en la discusión.

### Decisión: Modelo A puro (igual que Unity `ScrollRect` / HTML `scrollbar-gutter: stable`)

Estructura objetivo:

```
ScrollView (sin clip directo en el contenido; o clip = rect completo solo para
            no cullar las barras)
├── m_Viewport   ← NUEVO UIElement interno, SetClip(true), rect = área de
│   │                contenido {x, y, W − bw, H − bw}
│   └── m_Content ← se reparenta acá (hijo del viewport, no del ScrollView)
├── m_VScrollbar (hijo del ScrollView → renderiza en su franja, afuera del clip)
└── m_HScrollbar (ídem)
```

Así el contenido se recorta estrictamente al área útil y las barras quedan como
siblings (nunca solapan texto, cero sliver). Es el patrón `RectMask` de Unity /
`QAbstractScrollArea` de Qt / gutter estable de web.

### Cambios en `engine/src/UI/ScrollView.cpp` / `.h`

1. **Nuevo miembro `m_Viewport`** (`unique_ptr<UIElement>` con `SetClip(true)`),
   creado en el ctor. `SetContent` reparenta `m_Content` como hijo del viewport
   (mantener reordenamiento topmost de las barras para hit-test).
2. **Layout del viewport**: en `OnLayoutComputed`, posicionar `m_Viewport` en
   absoluto al área de contenido:
   - con V bar activa: `x .. x + W − bw` (y el alto completo salvo H bar);
   - con H bar activa: `y .. y + H − bw`;
   - sin barras: el área completa.
   El rect del viewport reemplaza al "layoutW/availW" del contenido actual: el
   contenido sigue siendo ancho `max(natural, viewportW)` pero el clip del
   viewport lo recorta (ya no hace falta que el clip del ScrollView sea el que
   recorta).
3. **`GetViewportSize`** se deriva del rect de `m_Viewport` (única fuente de
   verdad para clamps, `SetRange`, drag, wheel y `SyncScrollbar`).
4. **Scrollbars**: mantener la esquina (V inferior frena ante H bar y viceversa),
   pero **revisar el inset de 2px**: con el clip escopado ya no hay filtraciones;
   decidir si se quita el inset (barras flush al viewport, sin hueco decorativo
   por donde se vea el fondo) o se mantiene como margen (el hueco queda vacío, no
   con texto pasando). Ajustar la franja para que el track pegue justo con el
   borde del viewport.
5. **Hit-test**: como el contenido ahora es hijo del viewport (más chico que el
   ScrollView), el hit-test del strip de las barras queda libre automáticamente;
   igual mantener las barras como hijos topmost del ScrollView (no del viewport).
6. **Drag/wheel** leen clamps del viewport; sin otros cambios de lógica.

### Verificación

- Build completo DLL + editor.
- Correr el editor con la línea larga de test (`HORIZONTAL SCROLL TEST ...` en
  `main.cpp`): scrollear a `maxX` → las letras terminan justo en el borde del
  viewport, la barra vertical ya NO tapa las últimas letras y no hay línea finita
  por la que se vea pasar el texto.
- Probar resize del panel (las barras aparecen/desaparecen) → el clip del viewport
  acompaña sin sliver ni culling incorrecto del contenido.
- Preguntar al usuario si se mantiene la línea de test o se remueve antes de cerrar.

### Notas de contexto (para el registro)

- Sistemas profesionales: Windows clásico = Modelo A (reserva); Windows 11,
  macOS/iOS, GTK = Modelo B (overlay translúcido auto-hide); Unity `ScrollRect`,
  Unreal `ScrollBox`, Qt `QAbstractScrollArea` = Modelo A con viewport+scrollbar
  siblings; web = ambos vía CSS (`scrollbar-gutter: stable` = A, overlay scrollbars
  = B). Nuestro diseño apunta a A; el bug era que el clip no seguía a la reserva.
- El modelo B (overlay) NO aplica a un log/consola opaca: las barras A transparentan
  contenido a propósito y se ocultan; un log necesita siempre visible y opaco → A.

### Implementación 2026-08-06 (Opción 1, build OK)

Se hizo la Opción 1 (nodo interno `m_Viewport`, Modelo A puro):

- `ScrollView.h`: nuevo miembro `UIElement* m_Viewport`; `ApplyContentLayout`
  declarada con `(float layoutW, float layoutH)`.
- `ScrollView.cpp`:
  - ctor: `m_Viewport = new UIElement()` con `SetClip(true)`, añadido como primer
    hijo; las barras siguen siendo hijos directos del ScrollView (topmost para
    hit-test, siblings del viewport).
  - `~ScrollView`: elimina también `m_Viewport` (RemoveChild + delete).
  - `SetContent`: `m_Content` se reparentea al viewport (no al ScrollView); después
    se reordenan las barras a topmost.
  - `OnLayoutComputed`: posiciona el viewport absoluto sobre el área utilizable
    (`{cr.x, cr.y, cr.x+availW, cr.y+availH}`) y llama `m_Viewport->ComputeLayout`
    para refrescar su computedRect/clip; luego `ApplyContentLayout`; `SyncScrollbar`;
    y por último reaplica `ApplyContentLayout` (idempotente) ante el ajuste de
    offset hecho por el callback de la barra.
  - `ApplyContentLayout`: el contenido se acopla al **origen absoluto del viewport**
    (`vp.x/y`) y se desplaza con `-ScrollOffset`, así hereda la posición global real
    y queda dentro del clip del viewport (nunca alcanza las franjas de las barras).
- Efecto: el clip del contenido = viewport (`W − barra`); las barras quedan fuera
  del clip → no se tapan las últimas letras ni queda la línea finita.

Verificado: build DLL+editor OK; editor corre sin stderr/VUID; la barra horizontal
sigue capturando su drag (hit-test topmost OK).

### Fix del borde inferior del track (2026-08-07, scissor truncado)

El usuario reportó que con `hidpi=false` la scrollbar vertical medía track 10px /
thumb 6px, pero la horizontal media **track 9px / thumb 5px** (2px de track arriba
del thumb, 1px abajo). Con `ui_outlines: true` se veía que **la línea inferior del
track no se dibujaba** → pista: algo estaba "comiéndose" el píxel de abajo.

**Causa raíz**: no era la geometría (en `UIScrollbar` el track era `10.0` y el thumb
`6.0` en ambas orientaciones, y el snapping a entero ya estaba en floats). Era el
**scissor** de `UIRenderer.cpp`:

```cpp
// antes (trunca hacia abajo)
s.offset.y = (int32_t)(logicalClip.y * scale);
s.extent.height = (uint32_t)(logicalClip.w * scale);
```

El `ScrollView` tiene `SetClip(true)` → el clip del scrollbar = su propio rect
(fraccional, p.ej. `bottom = 1615.806`). El truncado a entero deja afuera del
scissor la fila final (la que cae en la parte fraccionaria del borde) → el quad del
track horizontal pierde su última fila (y su outline). La barra vertical no sufría
porque su borde derecho caía en un entero tras el `round`.

**Fix**: helper `ScissorFromLogicalClip` en `UIRenderer.cpp` que usa **floor del
offset y ceil del borde opuesto** (redondeo expansivo/conservador) en `pushQuad` y
`ApplyScissor`. El scissor abarca todo píxel que el quad toque; el overshoot es
estampado por el propio quad (el scissor solo restringe). Se mantiene el batching
(igual comparación de `VkRect2D`). Resultado: track 10px / thumb 6px con 2px arriba
y 2px abajo en ambas orientaciones, y el borde inferior del track se dibuja.

**Otras decisiones de la ronda (no revertidas)**:
- `std::round` en los offsets del thumb y del track (`UIScrollbar::OnLayoutComputed`
  y `ScrollView::SyncScrollbar`): no eran la causa, pero mantienen la geometría en
  píxel entero. Se conservan.
- Se removió el log temporal `ScrollbarDebugLog` de `UIScrollbar.cpp` (incluyó
  `<cstdio>`/`<filesystem>`).

Verificado por el usuario: "ahora si se ve perfecto!".
