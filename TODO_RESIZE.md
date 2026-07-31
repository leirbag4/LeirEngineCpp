# TODO: Soporte de resize del editor (ventana / viewport)

## Estado actual (lo que encontré al investigar)

El editor renderiza la escena 3D a un **render target interno (RenderTexture)** y esa imagen
se dibuja pegada en el panel `Viewport`. Para que la escena no se vea estirada, el **aspecto
de la foto debe coincidir con el tamaño real (en píxeles) del panel Viewport en pantalla**.

Hoy el tamaño de la foto se calcula con una fórmula aproximada:

```cpp
// editor/src/main.cpp:98-99
m_ViewportW = (uint32_t)(GetWidth() * 0.78f);
m_ViewportH = (uint32_t)(GetHeight() - 30);
```

que NO coincide con el ancho real del viewport (= ventana − Hierarchy − Inspector):

| Resolución de ventana | Foto (RT) | Viewport real | ¿Coincide? |
|---|---|---|---|
| 1280x720 (antes) | 998 x 690 (1.45) | 860 x 690 (1.25) | No |
| 1600x900 (ahora) | 1248 x 870 (1.43) | 1096 x 870 (1.26) | No |

→ La escena se ve horizontalmente estirada (sutil pero distorsionada). Es preexistente.

### Infraestructura de resize faltante (para la opción B)

- **No hay callback de framebuffer**: `glfwSetFramebufferSizeCallback` no está registrado
  en ningún lado (solo teclado/ratón en `engine/src/Input/InputManager.cpp:27-31`).
- `m_Width`/`m_Height` de `CoreApplication` se setean una sola vez en el ctor
  (`engine/src/Core/CoreApplication.cpp:15-16`) y nunca se actualizan al redimensionar.
  La UI usa esos valores viejos cada frame (`editor/src/main.cpp:352`).
- `VulkanDevice::m_FramebufferResized` existe con `WasResized()`/`ResetResized()`
  (`engine/include/LeirEngine/Rendering/VulkanDevice.h:100-101,174`) y el camino de recrear
  el swapchain está (`engine/src/Rendering/VulkanDevice.cpp:818-833`) pero **nadie pone el
  flag en `true`** — está dormido.
- La ventana es redimensionable (`GLFW_RESIZABLE = GLFW_TRUE`, CoreApplication.cpp:24), así
  que hoy estirar la ventana produce estado roto: el swapchain se puede recrear por
  OUT_OF_DATE pero RT, cámara y UI quedan con valores viejos.

## Opciones

| Opción | Qué es | Esfuerzo | Resultado |
|---|---|---|---|
| **A** | Calcular el tamaño de la foto con el ancho real del viewport, usando constantes compartidas para los anchos de Hierarchy/Inspector (en el layout y en el tamaño de la RT). Bloquear resize de la ventana para estabilidad | Chico | Aspecto correcto a tamaño fijo (1600x900) |
| **B** | Resize real: callback de framebuffer → actualizar `m_Width`/`m_Height` → recrear RenderTexture + `RecreatePipeline` del material → actualizar aspect de cámara → layout. Rehabilitar `GLFW_RESIZABLE`. Idealmente a futuro: paneles redimensionables tipo dock | Grande (milestone propio) | Editor redimensionable estilo industria |

## Decisión

1. **Opción A (IMPLEMENTADA)** — para que el Inspector (InspectorTransformPanel) funcionara con
   aspecto correcto en 1600x900. Se bloqueó el resize de la ventana mientras tanto.
2. **Opción B (IMPLEMENTADA)** — infraestructura de resize completa y ventana redimensionable
   de nuevo. Detalle abajo.

## Qué incluye la Opción A (implementada)

- `leir_settings.json`: `1600 x 900` (16:9, correcto para monitor 1920x1080, ventana no-fullscreen).
- Constantes compartidas en `editor/src/main.cpp`:
  - `kHierarchyWidth = 264.0f` (era 200 → +32%)
  - `kInspectorWidth = 290.0f` (era 220 → +32%)
  - `kBottomBarHeight = 30.0f`
  usadas tanto en los offsets del layout como en `m_ViewportW`/`m_ViewportH`.
- `engine/src/Core/CoreApplication.cpp`: `GLFW_RESIZABLE = GLFW_FALSE` (REVERTIDO en la B).

---

# Plan B (resize): IMPLEMENTADO

## Diseño

- **RT in-place**: la `RenderTexture` se redimensiona con `Resize(w,h)` sin cambiar el puntero
  (conserva render pass y sampler, que no dependen del tamaño).
- **Callback en `CoreApplication`** (el engine posee la ventana): actualiza `m_Width`/`m_Height`
  y expone `virtual OnWindowResized(w,h)` que las subclases sobreescriben. El editor no toca GLFW.
- **Tamaño del RT derivado del layout real**: se lee `UIViewportPanel::GetComputedRect()` después
  de `UpdateLayout()` — el RT siempre coincide con lo que se dibuja, aunque cambien anchos de paneles.
- **Verificación por frame barata**: `UpdateViewportRenderTarget()` compara tamaños (no-op si igual).
  El flag de VulkanDevice queda solo para el recreo del swapchain.

## Qué se hizo

### Fase 1 — Infraestructura de resize (engine)
- `CoreApplication.h/.cpp`:
  - Ctor registra `glfwSetFramebufferSizeCallback` + `glfwSetWindowUserPointer`.
  - Callback estático → `HandleWindowResize(w,h)` → actualiza `m_Width/m_Height` (si >0) y llama
    `virtual OnWindowResized(w,h)`.
  - `GLFW_RESIZABLE = GLFW_TRUE` (se revirtió el lock de la opción A).
- `VulkanDevice.h`: nuevo `NotifyResized()` → `m_FramebufferResized = true`
  (el camino de recrear swapchain ya existía en `RecreateSwapchain`, ahora se dispara en present).

### Fase 2 — RenderTexture redimensionable (engine)
- `RenderTexture.h/.cpp`: refactor a `CreateRenderPass()` / `CreateSampler()` / `CreateResources()`
  / `DestroyResources()`. Nuevo `Resize(w,h)`: `vkDeviceWaitIdle` → destroy → create. Render pass
  y sampler se conservan (formats fijos).
- `UIRenderer.h/.cpp`: nuevo `InvalidateViewportDescriptor(RenderTexture*)` que libera el
  descriptor set viejo (`vkFreeDescriptorSets`) y lo borra de `m_VpDescCache` (evita apuntar a una
  image view destruida y evita acumular descriptor sets en cada resize).

### Fase 3 — Editor (`editor/src/main.cpp`)
- `EditorApp::OnWindowResized` → `m_VulkanDevice->NotifyResized()`.
- Nuevo `UpdateViewportRenderTarget()`: lee el rect del viewport → `Resize()` del RT →
  `InvalidateViewportDescriptor` → actualiza el aspect de la cámara (`SetPerspective`) → log.
- Llamado en `OnUpdate` justo después del `SetScreenSize + UpdateLayout` existente.

## Bugs encontrados durante la verificación y corregidos

1. **Crash al resize (`Assertion failed: window != NULL` en `window.c:711`)**:
   `VulkanDevice` usaba `glfwGetFramebufferSize(glfwGetCurrentContext(), ...)` en
   `ChooseSwapchainExtent` y `RecreateSwapchain`. Como la ventana es Vulkan-only
   (`GLFW_NO_API`), `glfwGetCurrentContext()` devuelve SIEMPRE `NULL` (GLFW solo setea el
   slot con `glfwMakeContextCurrent`, que rechaza ventanas NO_API). Al primer resize,
   `vkAcquireNextImageKHR` devolvía `VK_ERROR_OUT_OF_DATE_KHR` → `RecreateSwapchain()` →
   assert → cierre. Fix: `VulkanDevice` guarda `GLFWwindow* m_Window` y lo usa directo.
   (El bug existía desde antes de la Opción B; la ventana no era redimensionable en A.)

2. **Layout congelado al resize**: el root del editor se creaba con
   `Rect2D::Absolute(0,0,GetWidth(),GetHeight())`, un rect absoluto que ignora el
   `availableSize` del layout. Aunque `SetScreenSize`/`UpdateLayout` corrieran con el nuevo
   tamaño, el viewport (hijo del root) quedaba siempre en 1046x870. Fix: el root usa
   `AnchorSet::Stretch()` (como el canvas), así `ComputeLayout` lo rellena con el screen size
   real cada frame.

3. **DPI/startup inconsistente**: `m_Width/m_Height` arrancaban con el tamaño pedido en
   settings (coords de ventana) mientras swapchain/RT usan framebuffer pixels. Con DPI != 100%
   el UI quedaba en un área equivocada. Fix: `CoreApplication` ctor lee `glfwGetFramebufferSize`
   tras crear la ventana y lo usa como tamaño lógico (consistente con el camino de resize).
   En DPI 100% no cambia nada.

## Fullscreen flicker — corregido (carrera CPU-GPU en el vertex buffer de la UI)

**Síntoma**: en fullscreen, al mantener click derecho y rotar la cámara, los paneles de debug
parpadeaban mostrando el atlas de la fuente cubriendo toda el área del panel; al soltar el click
desaparecía. En ventana no se notaba.

**Causa raíz**: `UIRenderer` escribía un **único** vertex buffer cada frame
(`vkMapMemory` + `memcpy`) mientras el frame anterior (N-1) podía seguir ejecutándose y
leyendo el mismo buffer. Con `MAX_FRAMES_IN_FLIGHT = 2`, `BeginFrame` solo espera el fence
del frame N-2 (`VulkanDevice.cpp`), no el del N-1 → el CPU sobreescribe mientras el GPU dibuja
→ vértices partidos: un quad de glyph (descriptor del atlas) leía los vértices del quad de
fondo (rect completo + UVs 0..1) → el atlas estirado sobre el panel. Solo se veía al rotar
porque ahí el overlay de debug (mouse/FPS/labels) cambia cada frame; con UI estática los datos
partidos eran idénticos e invisibles.

**Fix**: **doble-buffer del vertex buffer de la UI** (`m_VertexBuffers[2]` +
`m_VertexMemories[2]`, indexado por `GetCurrentFrameIndex()`), mismo patrón que el UBO del
`RenderPipeline` (`m_UBOBuffers[frame]`). Cada buffer solo se escribe tras esperar el fence de
su propio slot → sin carrera.

## vsync respetado (present mode)

`VulkanDevice::ChoosePresentMode` elegía SIEMPRE `MAILBOX`, ignorando `window.vsync` de
settings. Ahora: `vsync=true` → `FIFO` (throttling al refresh, sin tearing), `vsync=false` →
`MAILBOX`. Nuevo campo `VulkanDeviceConfig.vsync` (default `true`), el editor lo pasa desde
`LeirSettings::Get().window.vsync`.

## Descriptor pool de UI con FREE_DESCRIPTOR_SET_BIT

`InvalidateViewportDescriptor` (resize del viewport) llamaba `vkFreeDescriptorSets` sobre un
pool creado sin `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` → validation error
`VUID-vkFreeDescriptorSets-descriptorPool-00312` en cada resize. Fix: `CreateDescriptorPool`
acepta `flags` y el pool de la UI se crea con el flag.

## Verificación (checklist)

- [x] Build completo OK (engine DLL + editor EXE).
- [x] Estirar la ventana: sin crash/flicker, swapchain se recupera.
- [x] Maximizar/restaurar y minimizar→restaurar: correctos.
- [x] Aspecto correcto en todos los tamaños (sin distorsión de la escena 3D).
- [x] Paneles refluyen (Inspector/Hierarchy fijos, debug panels anclados al viewport).
- [x] DragInputs + auto-select + live value siguen funcionando.
- [x] Sin warnings/errores de Vulkan en consola durante el resize.
- [x] Fullscreen: sin flicker al rotar la cámara (vertex buffer doble-buffered).

---

# Fase 5 (pendiente, aplazada a futuro)

Ideas para la próxima iteración profesional, NO bloqueantes:

- **Paneles redimensionables tipo dock** (divider arrastrable entre Hierarchy/Inspector/viewport).
  El RT ya lo aguanta porque su tamaño deriva del layout (`GetComputedRect`), así que un cambio de
  ancho de panel dispararía el resize solo.
- **Soporte HiDPI**: hoy se usa tamaño de ventana (window coords) para layout/UI mientras el
  swapchain usa framebuffer size (glfwGetFramebufferSize). En DPI >100% difieren. Preexistente,
  documentado como caveat. Habría que unificar con escala de DPI (o usar framebuffer size como
  tamaño lógico).
- **Refinar `InvalidateViewportDescriptor`**: el pool de la UI ya soporta free
  (`VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`). Pendiente de revisar si conviene un
  pool más grande o recrear el pool en resize.


---

# Plan B (layout): `SizePolicy::Content` — IMPLEMENTADO

## Problema que resuelve

El `InspectorTransformPanel` dentro del Inspector se estiraba en alto ocupando todo el panel.
Causa raíz: `SizePolicy::Fill` en un Column significa "estirar en alto para ocupar todo el
sobrante" (`UIElement::ComputeColumnLayout`). Encadenado (panel Fill → filas Fill → DragInputs
Fill en el Row) deformaba todo. El ancho en un Column siempre es full-width
(`childW = innerW`), así que el Fill solo afectaba el alto.

Antes de esto, la única forma de "alto fijo" era `Fixed` + `GetMinSize().y` hardcodeado
(número mágico frágil).

## Qué se agregó

### Engine — `engine/include/LeirEngine/UI/UIElement.h` / `engine/src/UI/UIElement.cpp`

- Nuevo valor en el enum `SizePolicy`: `Content`.
- Nuevo método virtual `Vector2 UIElement::GetContentSize() const`:
  - Para `LayoutMode::Free` → devuelve `GetMinSize()`.
  - Para `Row`: ancho = `padding + Σ(hijos) + spacings + padding`, alto = `padding + max(hijos)`.
  - Para `Column`: alto = `padding + Σ(hijos) + spacings + padding`, ancho = `padding + max(hijos)`.
  - El tamaño "natural" de cada hijo se obtiene con `GetNaturalSize()`:
    - hijo `Content` → se recorre recursivamente (`GetContentSize()`).
    - hijo `Fixed`/`Fill`/`Grow` → `GetMinSize()` (el Fill/Grow no tienen tamaño de contenido intrínseco).
- En `ComputeRowLayout` y `ComputeColumnLayout` (tanto en el pase de `fixedTotal` como en el de
  tamaño del hijo):
  - hijo `Content` → tamaño = `max(GetContentSize(), GetMinSize())` (el min size actúa como piso).

### Editor — `editor/src/UI/InspectorTransformPanel.cpp` / `editor/src/main.cpp`

- `InspectorTransformPanel` ahora usa `SizePolicy::Content` (main.cpp) — el alto se calcula del
  contenido, no se estira.
- Las filas Position/Rotation/Scale dentro del panel usan `SizePolicy::Content` (alto ~22px,
  el de los DragInputs), en vez de `Fill`.

## Resultado

El panel ocupa exactamente el alto de su contenido (~101px: título + 3 filas + padding/spacing),
el ancho sigue siendo full-width del Inspector, y los DragInputs no se deforman. Si el contenido
cambia (fuente, más filas, etc.) el alto se recalcula solo — sin números mágicos.

## Nota

`UITestPanel`/`CameraTestPanel` (paneles flotantes con rect absoluto) siguen usando `Fill` en
sus filas internas y no se vieron afectados. Para los paneles anclados en Column/Row, `Content`
es ahora la opción recomendada sobre `Fill` cuando se quiere alto/ancho de contenido.

