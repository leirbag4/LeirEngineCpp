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

1. **Ahora → Opción A** para que el Inspector (InspectorTransformPanel) funcione con aspecto
   correcto en la ventana de 1600x900. Se bloquea el resize de la ventana mientras tanto.
2. **Apenas el Inspector funcione → Opción B** (refactor profesional): infraestructura de
   resize completa y re-habilitar la ventana redimensionable.

## Qué incluye la Opción A (implementada)

- `leir_settings.json`: `1600 x 900` (16:9, correcto para monitor 1920x1080, ventana no-fullscreen).
- Constantes compartidas en `editor/src/main.cpp`:
  - `kHierarchyWidth = 264.0f` (era 200 → +32%)
  - `kInspectorWidth = 290.0f` (era 220 → +32%)
  - `kBottomBarHeight = 30.0f`
  usadas tanto en los offsets del layout como en `m_ViewportW`/`m_ViewportH`.
- `engine/src/Core/CoreApplication.cpp`: `GLFW_RESIZABLE = GLFW_FALSE` (se revierte en B).

## Qué debe incluir la Opción B (pendiente)

- Registrar `glfwSetFramebufferSizeCallback` en la ventana.
- Actualizar `m_Width`/`m_Height` de `CoreApplication` en el resize.
- Marcar `m_FramebufferResized = true` (y consumirlo donde ya existe el camino de recrear swapchain).
- Recrear `RenderTexture` del viewport con el nuevo tamaño y `m_Material->RecreatePipeline(...)`.
- Actualizar el aspect de la cámara del viewport.
- Re-habilitar `GLFW_RESIZABLE = GLFW_TRUE`.
- (Ideal, no bloqueante) Paneles del editor redimensionables / dockeables como Unity.

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

