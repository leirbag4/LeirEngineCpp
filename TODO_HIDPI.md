# HiDPI — TODO

Documentación conceptual + plan de implementación del soporte HiDPI para LeirEngine
(engine + editor, cross-platform: Windows / macOS / Linux).

Estado: **IMPLEMENTADO y VERIFICADO en Windows** (ver plan al final).

---

## 1. El problema en una frase

La UI del motor se dibuja en **píxeles físicos** (canvas = framebuffer) pero el **input llega en
coordenadas lógicas** (`glfwGetCursorPos`). En displays con escala ≠ 100% (ej. Windows 125%) estas
dos coordenadas difieren, y además la UI **no escala con el DPI** (debería verse más grande).

---

## 2. Conceptos base

### Coordenadas lógicas vs físicas

Windows trabaja con dos espacios de píxeles:

| | Qué es | En una notebook 1920×1080 @ 125% |
|---|---|---|
| **Físico (framebuffer)** | Los píxeles reales del panel | 1920×1080 |
| **Lógico (DIP/screen units)** | Lo que ve Windows a escala | 1536×864 (1 DIP = 1.25 px físicos) |

GLFW expone ambos (idéntico en las 3 plataformas):

| Función GLFW | Devuelve |
|---|---|
| `glfwGetFramebufferSize` | píxeles físicos (lo que necesita Vulkan) |
| `glfwGetWindowSize` / `glfwGetCursorPos` | unidades lógicas |
| `glfwGetWindowContentScale` | el factor `dpr` (1.0 / 1.25 / 1.5 / 2.0...) |

`dpr = framebufferSize / windowSize`.

### DPI awareness (solo Windows)

El proceso debe **declarar** su awareness; GLFW 3.3+ lo hace automáticamente (per-monitor v2).

- **Unaware**: Windows le "miente" al proceso con escala 1.0 (todo consistente: ventana ==
  framebuffer == mouse) y **estira el resultado final 1.25×** al panel físico. Texto suavizado
  (upscale), pero todo alinea. *Es el estado aparente del motor hoy.*
- **Per-monitor aware**: el proceso recibe valores reales (framebuffer físico ≠ window lógico) y
  **el cálculo de escala es nuestro**.

### Las 3 plataformas

| Plataforma | Origen del scale | `glfwGetWindowContentScale` |
|---|---|---|
| Windows | DPI awareness → DIPs | 1.0 / 1.25 / 1.5 / 2.0... |
| macOS | `backingScaleFactor` (Retina, sin opt-out) | 1.0 / 2.0 |
| Linux | X11: `Xft.dpi`; Wayland: `wl_output` scale | 1.0 / 2.0 (GLFW 3.4: fractional) |

Ningún SO nos "pasa una variable": el programa declara awareness y **consulta** los valores via
GLFW. En las 3 plataformas se usa el mismo código.

---

## 3. Estado actual del motor (por qué es bug)

- **Layout + UI + renderer en físicos**: `SetScreenSize(GetWidth(), GetHeight())` con
  `GetWidth()` = framebuffer size (`CoreApplication.cpp`, ctor). El shader `UI.vert` divide por
  `screenSize` = `GetSwapchainExtent()` (físico). Anchos de splitter (300) en físicos.
- **Input en lógicos**: `glfwGetCursorPos` (`InputManager.cpp:25,122,136`).
- **Consecuencias a 125%**:
  - El mouse se desfasa 1.25× respecto a la UI (click-position, caret, drag, hit-test).
  - La UI **no se agranda** con el DPI (paneles/letras igual de chicos que a 100%).
  - `CenterWindow` mezcla workarea física con tamaño lógico → centrado corrido.

### Observación empírica (Windows 11, notebook 14" 1920×1080, usuario)

- A 125% **y** a 100% se ve **todo igual y pixel-perfect**; la ventana coincide con
  `settings.json` (`window: { width: 1346, height: 700, pos_x: 165, pos_y: 170 }`) en ambas escalas.
- **Diagnóstico confirmado con código (2026-07-31)**:
  - `DPI awareness: 2` → el proceso **sí es per-monitor aware v2** (GLFW llama
    `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` al init, `win32_init.c:692`).
  - A 125%: `Window sizes: logical 1346x700, framebuffer 1683x875, contentScale 1.25` y
    `Surface capabilities: currentExtent 1683x875` → la ventana se crea a **px físicos** y el
    framebuffer/swapchain son físicos; el motor renderiza **1:1 físico** ignorando `contentScale`.
  - **Por eso "se ve igual"**: la ventana ocupa la misma fracción del monitor en ambas escalas
    (mismo nº de px físicos). El editor es de facto un modo **"Fixed 1×"**: a 125% el texto de
    16px físicos = 12.8 DIPs → **más pequeño que el resto de Windows**, y sin estirar (1:1), así
    que nada se desalinea.
  - **Semántica GLFW por plataforma** (importante):
    - *Windows (aware)*: `glfwGetWindowSize == glfwGetFramebufferSize` = **físicos** (WM_SIZE
      lParam pasa directo, `win32_window.c:1023-1048`); lógicos = físico ÷ contentScale. El
      cursor (`WM_MOUSEMOVE` → `GET_X_LPARAM`, `win32_window.c:858`) también es **físico**.
    - *macOS/Linux*: window size = lógicos, framebuffer = físicos, cursor = lógicos.
  - **Conclusión**: la UI deja de estar en físicos; pasa a **lógicos** (`lógicos = fb ÷ scale`),
    el input se convierte físico→lógico en Windows, y los render targets (swapchain ya lo es por
    extensión de superficie, viewport RT) quedan en físicos = lógico × dpr.

### Implementación (base HiDPI completa, Windows verificado)

- `CoreApplication`:
  - Ctor recibe `hidpi`; crea la ventana a `lógico × scale` físicos cuando HiDPI está activo
    (misma área lógica a cualquier DPI).
  - `GetWidth/GetHeight` = **lógicos** (derivados de `framebuffer ÷ GetContentScale()`, uniforme
    en las 3 plataformas); `GetFramebufferWidth/Height` = físicos; `GetContentScale()` = scale
    real o 1.0 si HiDPI off (`SetHidpiEnabled`).
  - `glfwSetWindowContentScaleCallback` → `OnContentScaleChanged()` virtual + actualiza el scale
    del InputManager.
  - `CenterWindow` **sin** división (workarea/pos comparten unidades nativas en cada plataforma).
- `InputManager`: `ToLogical(x,y)` divide por el scale efectivo en Windows (no-op en mac/linux);
  `SetContentScale()` lo setea CoreApplication (antes de `Init` y al cambiar DPI).
- `UIRenderer`: push-constant `screenSize` = tamaño lógico del canvas (no swapchain).
- Editor: `settings.window.hidpi` (default true); RT del viewport = `lógico × dpr` (3D nítido);
  camera aspect en lógico; `OnContentScaleChanged` loguea (layout/RT se re-sincronizan por frame).
- **Fix de arranque maximizado** (encontrado durante la verificación): el ctor ya no pisa
  `m_Width/m_Height` con el rect windowed guardado (`if (maximized) { m_Width = width; ... }`
  eliminado). `UpdateNormalRect` descarta el estado maximizado, así que los lógicos conservan el
  tamaño real maximizado; antes la UI quedaba estirada (~1.49×) y el mouse desfasado hasta que un
  resize posterior disparara `HandleWindowResize`.

---

## 4. Cómo lo hacen los motores modernos

### La idea clave: SEPARAR dos escalas independientes

```
píxeles finales = unidades_lógicas × dpr × renderScale
                    │                │        │
                 layout UI      DPI del   resolución de
                 y texto       sistema    render (juego)
```

- **`dpr`**: hace que la UI del editor se vea más grande a mayor DPI.
- **`renderScale`**: cuántos píxeles físicos se renderizan para el **contenido** (viewport 3D, o
  un juego 2D). **El DPI del SO NO debe tocar el mundo del juego** — solo la resolución de los
  render targets.

### Referencias de la industria

| Motor | UI | Mundo |
|---|---|---|
| **Unity** | Canvas Scaler: *Constant Pixel Size* (escala con DPR), *Scale With Screen Size* (resolución de referencia), *Constant Physical Size* (cm, VR) | `Screen.width/height` = físicos; sprites con `pixelsPerUnit`, independiente del DPI; **Pixel Perfect Camera** (RT baja resolución + nearest + integer scaling) |
| **Unreal** | UMG con resolución de diseño + DPI scale curve por dispositivo | `r.ScreenPercentage` = % de resolución de pantalla, perilla separada |
| **Godot** | Content Scale Mode (canvas_items / viewport) | integer scaling para pixel-perfect |
| **WPF / Chrome / VS Code** | Todo en DIPs; el render sale nítido al píxel físico | — |

**Regla de oro**: una sola coordenada lógica para UI + input, y físicos solo en el borde final
(swapchain / render target).

---

## 5. Respuestas a preguntas frecuentes (educativo)

- **¿El viewport RT se ve afectado por DPI?** No en contenido: el RT se renderiza a
  `lógico × dpr` para nitidez (el 3D se ve igual, solo más definido). La cámara usa unidades
  propias del mundo.
- **¿Un juego 2D pixel-perfect en display HiDPI?** Se renderiza a su resolución base (ej.
  320×180) y se upscalea con **nearest + integer scaling** (1×,2×,3×). La UI (menús/HUD) se escala
  por separado con `dpr`. Es una fase futura (`renderScale`), pero la arquitectura no debe
  impedirlo → el mundo nunca se expresa en píxeles de UI.
- **¿Se puede seguir posicionando la UI por píxeles en código?** Sí — la API sigue en píxeles.
  En modo `System`, "píxel" = unidad lógica y el motor multiplica por `dpr` internamente; en modo
  `Fixed`, es el píxel físico crudo (comportamiento actual).
- **¿Android al exportar?** Mismo patrón: `dpr` = densidad del display (dp/density buckets),
  `renderScale` = resolución del juego.

---

## 6. Decisiones tomadas

- Orden de trabajo: **HiDPI primero** (base correcta), luego descriptor → RenderTexture, luego
  colapso + docking básico.
- Toggle HiDPI con dos políticas:
  - `System` (usa dpr del SO) — **valor por defecto**.
  - `Fixed` (dpr = 1.0, comportamiento actual) — opt-in del usuario.
- Alcance de esta iteración: **solo la base DPI** (no `renderScale`/pixel-perfect 2D todavía).

---

## 7. Plan de implementación

### Paso 0 — Diagnóstico ✅
- Log en `CoreApplication` al arrancar de `GetWindowSize`, `GetFramebufferSize`, `GetContentScale`
  y el DPI awareness context (`GetAwarenessFromDpiAwarenessContext`). A 125% confirmado: proceso
  **per-monitor aware v2**, ventana/framebuffer en físicos, `contentScale 1.25`. (Ver §3.)

### Paso 1 — Engine (`CoreApplication`) ✅
- `GetContentScale()` (de `glfwGetWindowContentScale`; 1.0 si HiDPI off).
- `GetWidth()/GetHeight()` → **lógicos** = `framebuffer ÷ GetContentScale()` (uniforme: en Windows
  window==framebuffer==físicos para aware; mac/linux framebuffer físico, window lógico).
  `GetFramebufferWidth/Height` = físicos.
- Ctor recibe `hidpi` y crea la ventana a `lógico × scale` físicos (misma área lógica a cualquier
  DPI). Los callbacks de size derivan el lógico desde el framebuffer.
- `CenterWindow`: **sin** división — workarea/pos/size comparten unidades nativas por plataforma
  (físicas en Windows aware, lógicas en mac/linux).
- `glfwSetWindowContentScaleCallback` → recalcular scale + `OnContentScaleChanged()` (virtual) +
  propaga el scale al InputManager.
- Toggle: `SetHidpiEnabled(bool)` → si off, `GetContentScale()` = 1.0. Editor lo pasa desde
  `settings.window.hidpi` (nuevo, default `true`).
- `InputManager::ToLogical()`: divide por el scale efectivo en Windows (cursor físico en proceso
  aware); no-op en mac/linux. `SetContentScale()` llamado antes de `Init` y al cambiar DPI.

### Paso 2 — Engine (`UIRenderer`) ✅
- Push-constant `screenSize`: de `GetSwapchainExtent()` (físico) al **tamaño lógico del canvas**
  (`m_ScreenSize`, set en `Render(cmd, canvas)`). `UI.vert` ya divide por screenSize → cubre el
  swapchain físico sin tocar vértices; viewport/scissor quedan físicos (correcto).

### Paso 3 — Editor (`main.cpp`) ✅
- `SetScreenSize`, `ApplyPanelLayout`, sprite positions: usan `GetWidth/Height` → quedan lógicos
  automáticamente.
- `UpdateViewportRenderTarget`: RT del viewport a `round(lógico × dpr)`; camera aspect lógico.
- `OnContentScaleChanged` → loguea (layout/RT se re-sincronizan cada frame en `OnUpdate`).
- Ctor pasa `settings.window.hidpi`.

### Paso 4 — Verificación ✅ (usuario, 2026-07-31)
- A 125%: UI **más grande que antes** (igual que el resto de Windows), texto nítido nativo (1.25×
  píxeles, sin stretch), mouse alineado, splitters correctos, ventana = 1346×700 lógicos. ✅
- A 100%: idéntico visual a hoy. ✅
- Toggle off (`hidpi: false`): comportamiento "Fixed 1×" actual. ✅
- Arranque maximizado (`maximized: true`, con y sin HiDPI): tamaño lógico correcto y mouse
  alineado desde el primer frame (fix del ctor). ✅ (logs: `logical 1536x793` = fb÷1.25 con
  HiDPI, `logical 1920x991` == fb en Fixed.)

### Pendientes a futuro (fuera de esta iteración)
- `renderScale`: pixel-perfect 2D (RT base + nearest + integer scaling), screen percentage 3D.
- Abstracción de resolución para export móvil (Android dp / density buckets).
- Docking multi-window (ventanas flotantes del SO) — el docking por pestañas/anidamiento ya está
  implementado (ver `TODO_DOCKING.md`, Fase 1-3 completas).

---

## 8. Referencias de código relevantes

- `engine/src/Core/CoreApplication.cpp` — ctor crea ventana a `lógico × scale` (físicos); lógicos =
  `framebuffer ÷ GetContentScale()`; `CenterWindow` (unidades nativas); content-scale callback;
  `ToLogical`.
- `engine/shaders/UI.vert` — divide por `push.screenSize` (mapping px → NDC).
- `engine/src/UI/UIRenderer.cpp` — push-constant `screenSize` = `m_ScreenSize` (lógico del canvas).
- `engine/src/Input/InputManager.cpp` — `ToLogical(x,y)` (÷ scale en Windows, no-op mac/linux);
  `SetContentScale()`.
- `editor/src/main.cpp` — `SetScreenSize(GetWidth(), GetHeight())`, `ApplyPanelLayout`,
  `UpdateViewportRenderTarget` (RT), `OnShutdown` (guarda tamaño lógico ya).
