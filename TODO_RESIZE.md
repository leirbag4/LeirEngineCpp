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
