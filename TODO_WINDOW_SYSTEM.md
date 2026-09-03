# TODO Window System — UIWindow: ventanas internas + desacopladas

Sistema de ventanas profesional del editor/motor: una clase `UIWindow` que funciona **dentro**
del motor (embedded) y **fuera** (ventana OS desacoplada). Multiplataforma de diseño: Windows,
Linux, macOS + web (Emscripten) + Android/iOS a futuro.

> Regla: NUNCA commitear a GitHub sin que el usuario lo pida. Para CI, preguntar antes.

---

## 1. Objetivo

El editor necesita ventanas (AboutWindow, gameplay desacoplado, properties, etc.) que:

- Puedan vivir **dentro del editor** (embedded) — el día que porteemos a Android/iOS donde no
  hay ventanas OS, el mismo código anda y la ventana queda dentro del editor.
- Puedan vivir **fuera** como ventana OS real (desacoplada) en desktop — p.ej. una AboutWindow
  que sale del editor como ventana aparte.
- Funcionen en **todas las plataformas**: desktop (Win/Linux/macOS), web (Emscripten) y
  mobile (Android/iOS) a futuro.
- Tengan todo lo que tienen los frameworks/tecnologías UI profesionales: resize, maximize,
  minimize, bordes (en modo interno), barra de título, botones min/max/close, drag, modal.

---

## 2. Decisiones tomadas (sesión 2026-08-31)

| # | Decisión | Valor |
|---|---|---|
| 1 | **Orden de implementación** | **External primero** (es el costo obligatorio), internal después (casi gratis si el chrome/API es host-agnóstico) |
| 2 | **Nombre de la clase** | `UIWindow` |
| 3 | **Métodos de apertura** | `Show(UIWindow* parent)` y `ShowModal(UIWindow* parent)` (nombre moderno "modal", no "dialog") |
| 4 | **Resultado de la ventana** | `WindowResult` enum + `SetOnResult(callback)` — patrón Qt/Godot, NO bloqueante |
| 5 | **Barra de título / botones en external** | **OS nativa** por default (`GLFW_DECORATED=true`). **Futuro anotado:** modo `GLFW_DECORATED=false` + chrome propia (botones y barra unificados nuestros) |
| 6 | **Barra de título / botones en internal** | **Siempre nuestra** (dibujada por UIRenderer) — no hay chrome OS |
| 7 | **Modal** | Overlay translúcido negro sobre el padre + bloqueo de eventos al padre (internal: UIPanel hit-testable; external: dim del padre + ignorar eventos del padre en el loop) |
| 8 | **Desacople de dock panels** | `DockManager::DetachPanel(panel)` → crea UIWindow; **re-dock al cerrar la ventana** (industry default) |
| 9 | **Acceso al desacople** | Click derecho en el **tab del dock** → `UIContextMenu` contextual (diferentes items según el panel). Los que permitan desacople tienen **"Detach to Window" arriba de todo** |
| 10 | **Render en ventanas** | Cualquier ventana puede renderizar **2D y 3D**. La escena 3D ya vive en un `RenderTexture` compartido → la ventana solo muestrea el RT (sin re-renderizar escena). Multi-cámara = RT por cámara |
| 11 | **GLFW** | **Seguir usando GLFW.** Multi-ventana nativo (`glfwCreateWindow` por ventana). A futuro: capa de windowing propia debajo de `UIWindow` si queremos control total (borderless, snap, sombras) — pero es meses de trabajo, no ahora |
| 12 | **Estado de dock panels no desacoplados** | Siguen funcionando como hoy (mismo camino) — el desacople es aditivo |

---

## 3. Análisis de la arquitectura actual (estado 2026-08-31)

### 3.1 Cómo está hoy el windowing

| Componente | Estado | Atado a |
|---|---|---|
| **Ventana OS** | 1 GLFW window (`CoreApplication::m_Window`) | Un solo `GLFWwindow*` |
| **RenderBackend** (Vulkan/D3D12/WebGPU) | 1 swapchain por backend, creada contra `m_Window` | `void* window` → GLFWwindow |
| **InputManager** | Singleton, registra callbacks en `m_Window` | 1 GLFWwindow; ignora `window` en callbacks |
| **EventQueue** | Singleton global, hooks sin filtro de ventana | Global |
| **UICanvas** | 1 por app, conecta con `Set*Hook` (reemplaza) | 1 canvas, 1 ventana |
| **UIRenderer** | Renderiza 1 canvas (`Render(graph, canvas)`) | 1 canvas |
| **Escena 3D** | Renderiza a un `RenderTexture` **compartido** (off-screen) | NO ligado a la ventana |

### 3.2 Hallazgo clave

La escena 3D ya vive en un `RenderTexture` off-screen compartido → **una ventana externa solo
necesita renderizar UI** (que muestrea el RT si quiere 3D). No necesita una escena 3D separada.
Esto reduce drásticamente la complejidad de las ventanas desacopladas.

### 3.3 Análisis de GLFW

| Aspecto | Soporte GLFW | Nota |
|---|---|---|
| **Múltiples ventanas** | ✅ Sí (`glfwCreateWindow` varias veces) | Cada ventana: propio `GLFWwindow*`, surface, callbacks |
| **Resize/maximize/minimize** | ✅ `glfwSetWindowSize`, `glfwMaximizeWindow`, `glfwIconifyWindow`, `glfwRestoreWindow` | Completos |
| **Decoraciones OS** | ✅ `GLFW_DECORATED` (default true) | Chrome nativa Windows/macOS/Linux |
| **Sin decoraciones** | ✅ `GLFW_DECORATED=false` | Para nuestra barra (futuro, external) |
| **Transparencia** | ⚠️ `GLFW_TRANSPARENT_FRAMEBUFFER` (limitado) | No lo usamos por ahora |
| **Ventana modal** | ❌ Sin soporte nativo | Lo implementamos en el event loop |
| **Ventana hijo/embebido** | ❌ GLFW no tiene child windows | Ventanas internas = nuestro sistema |
| **Android/iOS** | ⚠️ GLFW 3.4+ (NDK/iOS), beta | Mobile: internal mode |
| **Web (Emscripten)** | ✅ GLFW porta bien | Single-window, internal mode |

**Conclusión GLFW:** seguir usándolo. Cubre 6 plataformas, ya integrado/testado. Reemplazar =
miles de líneas por plataforma (Win32/X11/Wayland/Cocoa/NDK/UIKit/Emscripten). A futuro
podemos abstraer detrás de nuestra propia `WindowSystem` (estilo Godot `DisplayServer`).

---

## 4. Arquitectura propuesta: UIWindow

```
UIWindow (engine/include/LeirEngine/UI/UIWindow.h) — clase base, host-agnóstica
├── API unificada: Show(parent), ShowModal(parent), Close(), Hide(), BringToFront(),
│   SetPosition/Size, Minimize/Maximize/Restore, SetTitle, flags, callbacks.
│
├── UIWindowExternal : UIWindow  — modo "desacoplado" (desktop)
│   └── GLFW window propio + SwapchainTarget (device/queues compartidos) + UICanvas
│       + UIRenderer propios. Input ruteado por GLFWwindow*. OS chrome default.
│       (futuro: GLFW_DECORATED=false + chrome propia).
│
└── UIWindowInternal : UIWindow — modo "embedded" (todas las plataformas)
    └── Hijo de un canvas existente; chrome dibujado por UIRenderer.
        Overlay layer (SetOverlayLayer(true)) → flota sobre el dock.
        Mobile/web: única opción.
```

**Contenido de la ventana:** un `UIElement*` (subárbol que el desarrollador construye).
Puede incluir `UIViewportPanel` (muestrea un RT → 3D) o puros paneles/labels (2D).

**Polimorfismo:** el código de usuario siempre usa `UIWindow*` → decide en runtime.

---

## 5. API de UIWindow (propuesta)

```cpp
enum class WindowResult { None, Ok, Cancel, Yes, No, Primary, Secondary };

class LEIR_API UIWindow : public UIPanel {
public:
    UIWindow();
    ~UIWindow() override;

    // Lifecycle
    void Show(UIWindow* parent = nullptr);          // Open no-modal
    void ShowModal(UIWindow* parent);               // Open modal (bloquea al padre)
    void Close();                                   // Cierra y (si es external) destruye host
    void Hide();                                    // Oculta, mantiene vivo
    void BringToFront();                            // Z-order (internal: reordena hijos del canvas)

    // Result
    void SetOnResult(std::function<void(WindowResult)> cb);
    void SetResult(WindowResult r);                 // llamado por la ventana (botón OK/Cancel)
    WindowResult GetResult() const;
    void SetOnAccepted(std::function<void()> cb);   // helper
    void SetOnCanceled(std::function<void()> cb);   // helper

    // State
    void SetTitle(const std::string& title);
    const std::string& GetTitle() const;
    bool IsModal() const { return m_Modal; }
    bool IsVisible() const { return m_Visible; }
    UIWindow* GetParentWindow() const { return m_ParentWindow; }

    // Position & size
    void SetPosition(const Vector2& pos);
    Vector2 GetPosition() const;
    void SetSize(const Vector2& size);
    Vector2 GetSize() const;
    void SetMinSize(const Vector2& minSize);
    void SetMaxSize(const Vector2& maxSize);
    void CenterOnParent();

    // Window state
    void Minimize();
    void Maximize();
    void Restore();
    bool IsMaximized() const;
    bool IsMinimized() const;

    // Flags
    void SetResizable(bool resizable);
    void SetHasTitleBar(bool hasTitleBar);
    void SetHasCloseButton(bool hasClose);
    void SetHasMinimizeButton(bool hasMin);
    void SetHasMaximizeButton(bool hasMax);

    // Callbacks
    void SetOnClosed(std::function<void()> cb);
    void SetOnResized(std::function<void(int,int)> cb);

    // Content
    void SetContent(UIElement* content);            // content subtree (owned by caller?)
    UIElement* GetContent() const;

protected:
    virtual void OnShow() {}                        // hook al abrir
    virtual void OnClose() {}                       // hook al cerrar
    bool m_Modal = false;
    bool m_Visible = false;
    // ...
};
```

---

## 6. Modal — cómo funciona

### 6.1 Internal
- Overlay: `UIPanel` semitransparente (negro ~0.5 alpha) sobre el canvas del padre, en overlay
  layer, `SetHitTestable(true)` para consumir clicks.
- El modal window se renderiza en overlay layer **encima** del overlay.
- Los clicks fuera del modal (sobre el overlay) no llegan al padre (el overlay es el deepest hit).

### 6.2 External
- Dim del padre: renderizar un quad translúcido sobre el swapchain del padre.
- Bloqueo: en el event loop, ignorar eventos cuyo `GLFWwindow*` == padre mientras el modal
  esté abierto.
- El modal window es una ventana OS aparte que recibe input normalmente.

---

## 7. Resultado de ventana — industria moderna

WinForms `DialogResult` (bloqueante síncrono) quedó viejo. La industria moderna:

| Tecnología | Mecanismo | Bloqueante |
|---|---|---|
| WinForms | `DialogResult ShowDialog()` | Sí (obsoleto) |
| WPF | `bool? ShowDialog()` | Sí (viejo) |
| Qt moderno | Signals `accepted()`/`rejected()` | No |
| Godot | Señales `confirmed`/`canceled` | No |
| Avalonia | `Task<TResult> ShowDialog<TResult>()` | No |
| Electron/Web | Promises / callbacks | No |
| **LeirEngine** | **`SetOnResult(cb)` + `WindowResult`** | **No** |

**Nuestra elección:** `SetOnResult(std::function<void(WindowResult)>)` + `WindowResult` enum.
Consistente con `SetOnClick`/`SetOnToolChanged` del resto del engine, no bloquea el loop.
La ventana llama `SetResult(Ok/Cancel)` + `Close()` (p.ej. desde el botón OK).

---

## 8. Desacople de dock panels

```
DockManager (sigue siendo dueño, funciona igual si NO desacoplás)
  ├── DockPane [Viewport] → content UIViewportPanel (RT de cámara 3D)   ← dockeado, como hoy
  └── (click derecho en el tab → UIContextMenu → "Detach to Window")

Cuando desacoplás un panel:
  DockManager::DetachPanel(panel)
    → crea UIWindowExternal (GLFW window + SwapchainTarget + UICanvas propio)
    → MUEVE el content del panel al window (mismo UIElement*, no copia)
    → el window renderiza su canvas en su swapchain

Al cerrar la ventana:
  → el content vuelve al dock (re-dock), posición/ratio recordados
```

- El `UIContextMenu` del tab es **contextual por panel** (diferentes items según el panel).
- Los paneles que permitan desacople tienen **"Detach to Window" arriba de todo**.
- 2D y 3D en cualquier ventana: 3D = `UIViewportPanel` muestreando el RT de la cámara
  (sin re-renderizar escena); 2D = sprites/labels en el canvas de la ventana.

---

## 9. Fases y checkboxes

### Fase A — Refactor `SwapchainTarget` (base de todo el external)

- [x] Extraer de `VulkanDevice` la parte "surface + swapchain + image views + framebuffers +
      depth + command buffers + sync" a una clase `SwapchainTarget`. Device/queues/command
      pool quedan compartidos en `VulkanDevice` (un solo device físico, múltiples
      surfaces/swapchains). `VulkanDevice::CreateSwapchainTarget(window)` crea targets
      adicionales reutilizando device/queues/render passes. (2026-08-31)
- [ ] Igual en `D3D12Backend`: swapchain + RTV heap + backbuffers + depth → `SwapchainTarget`.
- [ ] Igual en `WebGPUBackend`: `WGPUSurface` + configure → target por ventana.
- [x] `RenderBackend::CreateSwapchainTarget(window)` virtual (default nullptr) +
      `VulkanBackend` override. **Futuro:** abstraer a interfaz RHI-neutral (`ISwapchainTarget`)
      con 3 impls (Vulkan/D3D12/WebGPU) — para portar D3D12/WebGPU/macOS al final.
- [x] El main loop itera los targets: el editor llama `m_TestWindow->RenderFrame()` después
      del `EndFrame` principal (2ª ventana renderizando en Vulkan).
- [x] Verificar: 1 ventana (sin regresión) + 2ª ventana renderizando (ventana de test gris
      en Vulkan, verificada por el usuario: se mueve, redimensiona, cierra sin crash).

### Fase B — Input multi-ventana

- [x] Eventos etiquetados con `void* window` (GLFWwindow*): KeyEvent/PointerEvent/CharEvent/
      ScrollEvent ganan el campo `window` en `InputEvent.h`. (2026-08-31)
- [x] `EventQueue`: hooks removibles por token (`Add*Hook` devuelve `HookId`,
      `Remove*Hook(id)`); `ClearHooks` y `Set*Hook` legacy se mantienen. Multi-hook
      coexisten (canvas principal + gizmo log + canvas externo). (2026-08-31)
- [x] `InputManager`: `AddWindow(window)` registra callbacks en ventanas adicionales;
      estado por ventana (last mouse pos, content scale) vía `m_WindowStates`;
      callbacks etiquetan eventos con su ventana; `ToLogical` per-window. (2026-08-31)
- [x] `UICanvas::SetInputWindow(void*)` + filtro por ventana: eventos cuyo `window` no
      matchea se ignoran (nullptr = acepta todo, comportamiento single-window intacto);
      `ConnectToInputSystem` usa `Add*Hook` + tokens, `DisconnectFromInputSystem`
      remueve solo sus hooks (ya no `ClearHooks` global). (2026-08-31)
- [x] `UIWindowExternal::Show` conecta su canvas al input (SetInputWindow + Connect +
      AddWindow + content scale per-window); `DestroyNative` desconecta. (2026-08-31)
- [x] **Fix ruteo de input multi-ventana (2026-08-31, bug reportado por el usuario):**
      mover el mouse sobre la ventana externa pintaba hover/click en el editor principal.
      Causa: el **estado de polling global** (Keyboard/Mouse/Touch/Pointer) se actualizaba
      con eventos de TODAS las ventanas, y el canvas principal tenía `m_InputWindow=nullptr`
      (= acepta todo). Fix:
      - `InputManager::GetPrimaryWindow()` estático (devuelve `m_Window`; nullptr = single-window).
      - `EventQueue::Process()`: las llamadas a `Mouse/Keyboard/Touch/Pointer::ProcessEvent`
        y `Mouse::ProcessScroll` se envuelven en `IsPrimaryWindow(e.window)` — solo se
        actualizan si el evento es de la ventana principal (o primary==nullptr).
        Los hooks reciben TODOS los eventos (cada canvas filtra por su ventana).
      - `editor/src/main.cpp`: `m_Canvas->SetInputWindow(GetWindow())` — el canvas del editor
        se vincula a la ventana principal (antes nullptr = todo).
      Verificado: hover/click en la externa ya no toca el editor; en el editor funciona normal.
- [ ] ContentScale por ventana al mover entre monitores (el editor principal ya la maneja).

### Fase C — `UIWindow` base + chrome (host-agnóstico)

- [x] `UIWindow : UIPanel` — API completa (ver §5): Show/ShowModal/Close/Hide/BringToFront,
      SetPosition/Size/Min/Max, Minimize/Maximize/Restore, flags, callbacks. (2026-08-31)
- [x] Chrome widget reutilizable: barra de título (texto + botones min/max/close), bordes
      de resize (4 direcciones, CapturePointer, cursores), drag-move. (2026-08-31)
- [x] `WindowResult` + `SetOnResult` + helpers `SetOnAccepted`/`SetOnCanceled`. (2026-08-31)
- [x] Modal interno: overlay translúcido (UIPanel negro 0.55 alpha, hit-testable) +
      bloqueo. (2026-08-31)

### Fase D — `UIWindowExternal` (host GLFW, desktop)

- [x] `UIWindowExternal : UIWindow` — crea GLFW window + `SwapchainTarget` + `UICanvas`
      propio; OS chrome default; resize callback (`MarkResized`). (2026-08-31)
- [x] OS chrome nativo default (`GLFW_DECORATED=true`).
- [x] **Futuro anotado:** `GLFW_DECORATED=false` + chrome propia unificada (modo custom).
- [x] Render de contenido real en external: `UIRenderer` propio + `GCommandGraph` por
      ventana; `RenderFrame()` hace `BeginOverlayRenderPass` del target → `UpdateLayout`
      del canvas → `Render(graph, canvas)` → `CmdExecuteGraph(cmd del target, graph)`.
      (2026-08-31)
- [x] **Fix fondo negro/glitch + resize (2026-08-31, bug reportado por el usuario):**
      la ventana externa mostraba fondo negro con píxeles de colores (basura del swapchain)
      y al redimensionar el contenido quedaba cortado. Causa: el overlay render pass es
      LOAD_OP_LOAD (no limpia), y el canvas no seguía al resize. Fix:
      - `UIPanel` **fondo opaco Stretch** (primer hijo del canvas) — el canvas siempre
        pinta opaco sobre la memoria basura del swapchain (mismo patrón que el DockManager
        del editor). `m_Background` en `UIWindowExternal`.
      - `RenderFrame()` refresca `m_WindowCanvas->SetScreenSize(extent/scale)` **cada frame**
        desde el extent actual del swapchain → el layout sigue al resize.
      Verificado por el usuario: fondo limpio, contenido correcto, resize sin clipping.
- [x] Input conectado: `SetInputWindow(nativeWindow)` + `ConnectToInputSystem` +
      `InputManager::AddWindow` + content scale per-window (Fase B). (2026-08-31)
- [ ] Modal externo: dim del padre + bloqueo de eventos del padre.
- [x] **AboutWindow** — info del engine/desarrollador, botón OK, desde Help → About.
      `editor/src/UI/AboutWindow.{h,cpp}` (extiende UIWindowExternal): título, versión,
      backend, developer, botón OK que cierra con `WindowResult::Ok`. Deferred-delete
      en OnUpdate (evita use-after-free del callback del botón). **Bug corregido**: `SetFont`
      se llamaba antes de `Show()` → `GetCanvas()` era nullptr → crash en `ApplyFont`
      (`0xC0000005`). Fix: `SetFont` solo aplica la fuente si el canvas existe; la fuente se
      aplica igual al final de `Show()` vía `OnShow → ApplyFont`. Verificada por el usuario:
      Help→About abre, OK cierra sin crash. (2026-08-31)
- [x] **Botón de prueba en la ventana externa** (verifica input de widgets en la externa):
      `UIButton` "Click me" en la ventana de test que alterna el label; log confirma
      `Press/Release target: TestWinButton` ruteados a la ventana externa, sin tocar el
      editor principal. (2026-08-31)
- [x] Integración de prueba: editor crea `UIWindowExternal("Leir Test Window")` en OnInit
      (solo backend Vulkan), la renderiza en OnRender, la destruye en OnShutdown. Verificada
      por el usuario (2026-08-31): se ve, se mueve, redimensiona y cierra sin crash.

### Fase E — Desacople de dock panels

- [ ] Click derecho en el tab → `UIContextMenu` contextual por panel.
- [ ] Paneles desacoplables → item "Detach to Window" arriba de todo.
- [ ] `DockManager::DetachPanel(panel)` → crea UIWindowExternal con el content (misma
      referencia, no copia); re-dock al cerrar (posición/ratio recordados).
- [ ] 3D en ventanas: `UIViewportPanel` muestrea RT compartido.

### Fase F — `UIWindowInternal` (embedded, mobile/web)

- [ ] Mismo chrome/API, host = panel del canvas existente.
- [ ] Para Android/iOS/web (sin ventanas OS). Overlay layer.
- [ ] `UIWindow` polimórfico: el mismo código abre internal o external según plataforma/flags.

---

## 10. Archivos del plan

| Archivo | Acción |
|---|---|
| `engine/include/LeirEngine/UI/UIWindow.h` | **Nuevo** — UIWindow base |
| `engine/src/UI/UIWindow.cpp` | **Nuevo** — implementación |
| `engine/include/LeirEngine/UI/UIWindowExternal.h` | **Nuevo** (Fase D) |
| `engine/src/UI/UIWindowExternal.cpp` | **Nuevo** (Fase D) |
| `engine/include/LeirEngine/UI/UIWindowInternal.h` | **Nuevo** (Fase F) |
| `engine/src/UI/UIWindowInternal.cpp` | **Nuevo** (Fase F) |
| `engine/include/LeirEngine/Rendering/SwapchainTarget.h` | **Nuevo** (Fase A) |
| `engine/src/Rendering/SwapchainTarget.cpp` | **Nuevo** (Fase A) |
| `engine/include/LeirEngine/UI/Dock/DockManager.h` | Modificar — `DetachPanel` |
| `engine/src/UI/Dock/DockManager.cpp` | Modificar — detach/attach |
| `engine/src/UI/Dock/DockTabBar.cpp` | Modificar — context menu del tab |
| `engine/src/Core/CoreApplication.cpp` | Modificar — loop multi-ventana (Fase A/B) |
| `engine/include/LeirEngine/Input/InputManager.h` | Modificar — multi-window (Fase B) |
| `engine/src/Input/InputManager.cpp` | Modificar — callbacks por window (Fase B) |
| `engine/include/LeirEngine/Input/EventQueue.h` | Modificar — filtro por window (Fase B) |
| `engine/src/Input/EventQueue.cpp` | Modificar — filtro por window (Fase B) |
| `editor/src/UI/AboutWindow.h/.cpp` | **Nuevo** (Fase D) |
| `editor/src/main.cpp` | Modificar — Help → About abre AboutWindow |
| `engine/CMakeLists.txt` | Agregar nuevos .cpp |
| `TODO_WINDOW_SYSTEM.md` | Este archivo — ir marcando [x] |

---

---

## 12. Chrome profesional — bordes de resize, cursor y sombra (análisis 2026-08-31)

### 12.1 Cómo funciona en los sistemas operativos (investigado)

**Windows — resize borders (WM_NCHITTEST):**
- El OS envía `WM_NCHITTEST` (0x0084) al `WindowProc` para saber en qué zona de la
  ventana está el cursor. Según el valor devuelto (HTLEFT, HTRIGHT, HTTOP, HTTOPLEFT,
  HTBOTTOMRIGHT, …) el sistema **cambia el cursor automáticamente** y maneja el resize.
- El borde tiene DOS capas:
  - **Borde visible** (~1px) dibujado por DWM.
  - **Borde invisible** (área "non-client", ~7px extra) = zona donde el cursor cambia
    y el resize funciona aunque no haya línea visible. Grosor total ≈
    `GetSystemMetrics(SM_CXSIZEFRAME) + SM_CXPADDEDBORDER` (~8px).
- Apps con chrome propio implementan `HitTestNCA`: dividen la ventana en una **grilla
  3×3** y mapean el cursor a HT* según la distancia a cada borde (Microsoft docs,
  "Custom Window Frame Using DWM", Appendix C). Constantes típicas: LEFT/RIGHT = 8,
  BOTTOM = 20, TOP = 27.

**Windows — sombras:**
- DWM dibuja la sombra **automáticamente** solo para ventanas con chrome OS
  (WS_THICKFRAME/WS_CAPTION). En borderless (`WS_POPUP` sin frame), DWM no dibuja
  sombra → hay que usar `DwmExtendFrameIntoClientArea` con márgenes para que DWM
  extienda el frame (y la sombra) al área del cliente. Apps como Qt/Chromium/Electron
  hacen esto + `SetWindowRgn` con región más grande y dibujan su contenido en el área
  extendida. Alternativas modernas: `DWMWA_SYSTEMBACKDROP_TYPE`,
  `DWMWA_USE_HOSTBACKDROP_BRUSH`, `DWMWA_WINDOW_CORNER_PREFERENCE`.

**Linux:**
- X11: el window manager (KWin/Mutter/Compiz) dibuja bordes y sombras. Con CSD
  (client-side decorations, GTK/GNOME) la sombra la dibuja el cliente vía Cairo
  (drop-shadow filter). Wayland: SSD (KDE) o CSD (GNOME/GTK) según el compositor.

**macOS:**
- `NSWindow.shadow = true` → WindowServer dibuja la sombra automáticamente, incluso
  en borderless windows.

### 12.2 Nuestra implementación (ventanas internas en el canvas) — ESTADO REAL 2026-08-31

No hay OS window por ventana interna → dibujamos todo nosotros, replicando el
comportamiento profesional de Windows. **Implementado y verificado por el usuario**:

- **`HitTestZone(pos, onTitleBar)`** → grilla 3×3 (patrón HitTestNCA de Microsoft).
  Devuelve códigos HT* (`HTLEFT=10, HTRIGHT=11, HTTOP=12, HTTOPLEFT=13, HTTOPRIGHT=14,
  HTBOTTOM=15, HTBOTTOMLEFT=16, HTBOTTOMRIGHT=17, HTCLIENT=0`) según
  `m_ResizeBorderSize` (6px default, configurable). Incluye la barra de título
  (zona drag / doble-click maximiza).
- **`OnPointerMove`**: `HitTestZone` → cursor correcto (Arrow dentro; EW/NS/NWSE/NESW
  en bordes/esquinas; Arrow en barra de título). Hover de botones (close/min/max)
  aclara su color.
- **`OnPointerDown`**: botones primero (hit-test propio), luego `HitTestZone` →
  resize si es borde/esquina (con `CapturePointer`), drag si es barra de título
  (con doble-click → maximize/restore), sino `false` (el content lo maneja).
- **`OnPointerUp`/`OnPointerExit`**: termina operación + resetea cursor y hovers.
- **Fix clamp de posición en resize**: al resizear desde izquierda/arriba hasta el
  mínimo, **compensa la posición** (`newPos += newSize - clamped`) para que el borde
  opuesto NO se mueva (antes la ventana se desplazaba en X/Y).
- **Contenido edge-to-edge (margen 0) + anillo de resize FUERA del rect visual (Plan A,
  §13)**: el content llena el rect visual bajo la title bar (sin margen). El área de
  resize/cursor es un anillo transparente de `m_ResizeBorderSize` (6px) POR FUERA del
  rect visual vía `UIWindow::GetHitRect()` (override) + `UICanvas::HitTestRecursive`
  usa `GetHitRect()`. Ver "Bugs encontrados" abajo — el `parentOffset` estaba
  duplicando el inset (pre-Plan A el content se insetaba 6px y copiaba el color del body).
- **Título centrado verticalmente**: label con altura completa de la barra (`barH`),
  sin padding top (antes el texto tocaba arriba del bbox).
- **Borde visual de 1px** (configurable, apagable): `m_VisualBorderSize` (default 0 =
  sin borde, o 1px) + `SetVisualBorderSize()` + `SetBorderColor()`. **Render = Opción 3
  (4 quads vía `BuildBatch`)** — implementado en el Plan A (§13.4 paso 6).
- **Sombra** (`CreateShadow`/`DestroyShadow`/`UpdateShadowLayout`): **3 capas de
  `UIImage`** (alpha 0.28 / 0.12 / 0.05, extensión 0 / 4 / 8px sobre `m_ShadowSize`
  default 14px) insertadas en el canvas como hermanos **antes** del window, con
  `SetOverlayLayer(true)` (misma batch que el window, dibujadas primero → detrás).
  Sin textura (sin coste de render). Toggleable via `settings.json` →
  `"window.window_shadow": true/false` (default true, leído en el ctor) +
  `SetShadowEnabled()`/`SetShadowSize()`. Creada en `Activate()`, destruida en
  `Close()`/`Hide()`/dtor, reposicionada en `OnLayoutChrome` y `BringToFront`
  (reordena sombra → window). Ver "Bugs encontrados" abajo — el `parentOffset`
  duplicaba la posición de la ventana (sombra al 2× del movimiento).

### 12.3 Checkboxes — chrome profesional

- [x] `UIWindow::HitTestZone(pos)` → grilla 3×3 con `m_ResizeBorderSize` (6px default).
- [x] `OnPointerMove`: `HitTestZone` → cursor (Arrow/EW/NS/NWSE/NESW/Hand) + hover botones.
- [x] `OnPointerDown`: `HitTestZone` → resize | drag | botón.
- [x] `OnPointerUp`: terminar op + reset cursor.
- [x] Fix clamp de posición en resize (izquierda/arriba no desplazan la ventana).
- [x] `m_ResizeBorderSize` configurable (`SetResizeBorderSize`).
- [x] Borde visual 1px configurable/apagable (`m_VisualBorderSize` + `SetVisualBorderSize`)
      → **API lista, render pendiente (Plan A)**.
- [x] Sombra: 3 capas de `UIImage` detrás del window, `m_ShadowSize`, toggle
      `m_ShadowEnabled` (`SetShadowEnabled`/`SetShadowSize`) + `settings.json`.
- [x] Quitar el cursor Hand de prueba (OnInit) del editor.
- [x] BUILD + smoke + **verificado por el usuario**: content centrado, cursor en TODOS
      los lados/esquinas, resize sin desplazamiento al mínimo, sombra sigue a la ventana,
      sombra on/off via settings.

### 12.4 Bugs encontrados y corregidos (2026-08-31) — parentOffset duplicado

`UIElement::ComputeLayout(availableSize, parentOffset)` **SUMA** `parentOffset` al
rect computado (`m_ComputedRect = m_Rect.GetRect(...) + parentOffset`). Dos call sites
en `UIWindow.cpp` pasaban la posición ABSOLUTA (window/inset) como `parentOffset`,
duplicando el desplazamiento:

| Bug | Línea | Código malo | Síntoma | Fix |
|---|---|---|---|---|
| 1. Content mal ubicado | `OnLayoutChrome` | `ComputeLayout(..., {cr.x + b, cr.y + top})` | Content con 2×b a la izquierda (columna vertical de window bg), 2×titlebar de gap abajo de la barra, y right/bottom **tapados por IntWinBody** → cursor resize no funcionaba en right/bottom/abajo-derecha | `parentOffset = {cr.x, cr.y}` (el content es hijo del window; su offset ya tiene `{b, top}`) |
| 2. Sombra al 2× | `UpdateShadowLayout` | `ComputeLayout(..., {cr.x - inset, cr.y - inset})` | La sombra se movía el DOBLE que la ventana y "derivaba" a la derecha/abajo al moverla | `parentOffset = {0.0f, 0.0f}` (las capas son hijas del **canvas**; su offset ya es absoluto) |

**Regla aprendida**: el `parentOffset` de `ComputeLayout` es **solo la posición del
PADRE del elemento** (window/canvas), nunca la posición absoluta del elemento ni la
del padre + inset. Hijos de un window → `{cr.x, cr.y}`; hijos del canvas → `{0,0}`.

---

## 13. PLAN A — hit-rect expandido (margen de contenido = 0 + resize fuera del rect visual)

### 13.1 Contexto / motivación

El diseño actual (12.2) logra resize en todos los bordes con **inset de 6px invisible**
(el window copia el color del body y la franja queda del mismo color). Pero el usuario
quiere poder setear el **margen del contenido en 0** (content edge-to-edge) y que el
área de resize siga funcionando **por fuera del rect visual**: un anillo transparente
que solo captura eventos de mouse, más grande que el borde visible (1px opcional
dibujado encima, apagable).

### 13.2 Análisis del hit-testing actual (base del diseño)

`UICanvas::HitTestRecursive` (UICanvas.cpp:83-135):
- Recorre el árbol DFS, **hijos en orden inverso** (los de arriba primero).
- `inside = pos dentro de element->GetComputedRect()` (línea 124). Si ningún hijo lo
  captura, el elemento es el target.
- **Conclusión clave**: el target se decide exclusivamente por el **rect computado**
  y el orden de hijos. Un hijo que cubre el rect del padre **roba** el hit al padre.
- Por eso el inset actual funciona: `m_Content` NO cubre el borde → el window recibe
  el hit en la franja interior → cursor + resize.

### 13.3 Diseño (Opción A — recomendada) vs Opción B (contenedor transparente)

| Criterio | **A) Hit-rect expandido** | B) Contenedor transparente |
|---|---|---|
| Concepto | El window tiene 2 rects: visual (`m_ComputedRect`, para dibujar) y hit (`GetHitRect()`, expandido, para eventos) | Un `UIPanel` alpha 0 hermano del window, rect = visual + borde, que envuelve la zona de resize |
| Eventos | Van directo al window (es el elemento hit-testado) | Van **al contenedor**, NO al window → hay que enrutar manualmente cada `OnPointerDown/Move/Up` (o duplicar la lógica de resize) |
| Geometría | Un solo lugar (override de `GetHitRect()`) | Doble fuente de verdad: contenedor + window, sincronizar en cada `OnLayoutChrome` y `BringToFront` |
| Render | Sin cambio (`RenderElement` usa el rect visual) | Quad transparente por frame (batch inútil) |
| Toques al core | `HitTestRecursive` usa `GetHitRect()` en vez de `GetComputedRect()` (default idéntico) | Ninguno |
| Riesgo | Bajo (solo `UIWindow` lo override; default = rect visual) | Bajo para el resto, alto para el window (enrutamiento frágil) |
| **Veredicto** | **Ganadora** | Más código y más frágil para el mismo resultado |

**Decisión: Opción A.**

### 13.4 Implementación detallada — Opción A

**Paso 1 — `UIElement::GetHitRect()` virtual** (`engine/include/LeirEngine/UI/UIElement.h`):
```cpp
virtual Vector4 GetHitRect() const { return m_ComputedRect; }
```

**Paso 2 — `HitTestRecursive` usa `GetHitRect()`** (`engine/src/UI/UICanvas.cpp`):
- Línea 89: `const auto& r = element->GetHitRect();` (en vez de `GetComputedRect()`)
- Las comprobaciones de clip (97-122) y `inside` (124) usan `r` (el hit rect).
- El render NO cambia (`RenderElement` sigue usando `GetComputedRect()`).

**Paso 3 — `UIWindow::GetHitRect()` override** (`UIWindow.h/.cpp`):
```cpp
Vector4 UIWindow::GetHitRect() const override {
    const auto& cr = GetComputedRect();
    if (!m_Resizable || m_Maximized) return cr;
    const float b = m_ResizeBorderSize; // 6px default, configurable
    return {cr.x - b, cr.y - b, cr.z + 2.0f * b, cr.w + 2.0f * b};
}
```

**Paso 4 — Content a margen 0** (`OnLayoutChrome`):
- `m_Content->GetRect().offset = { 0.0f, top, cr.z, cr.w - top };`
- `m_Content->ComputeLayout({cr.z, cr.w - top}, {cr.x, cr.y});`
- Eliminar el `SetColor` de copia del body en `SetContent()` (ya no hace falta el inset
  invisible — el content llena el rect visual y el anillo está FUERA).

**Paso 5 — `HitTestZone` usa `GetHitRect()`**: la grilla 3×3 debe operar sobre el hit
rect (que incluye el anillo exterior) para que los bordes/esquinas se detecten fuera
del rect visual.

**Paso 6 — Borde visible** (independiente del hit). **Decisión: Opción 3 — 4 quads via
`BuildBatch`** (el mismo patrón que los outlines de debug, UIRenderer.cpp:861-869), NO
`UIImage` hijos:
- `UIRenderer::RenderElement`: al final, `dynamic_cast<UIWindow*>` → 4 llamadas a
  `Batch(nullptr, {x0,y0,x1-x0,bs}, ...)` (top/bottom/left/right) sobre los bordes del
  **visual rect**, con grosor `m_VisualBorderSize` (0 = apagado; 1/2/4px configurable
  via `SetVisualBorderSize`). Dibujadas DESPUÉS del loop de hijos → encima del content.
- Sin UIElement, sin `OwnsChild`, sin teardown, sin textura: 4 floats en el vertex buffer.
- `SetBorderColor`/`GetBorderColor` + miembro `m_BorderColor` (default `{0.42,0.46,0.55,1}`).

**Comportamiento resultante:**
- Contenido **edge-to-edge** (margen 0).
- El anillo exterior (6-8px, más grande que cualquier borde visible) captura el mouse:
  cursor de resize en todos los lados/esquinas + resize con `CapturePointer`.
- El contenido NO roba el hit en el anillo (no lo cubre) → el window lo recibe.
- El borde visible (1/2/4px) es puramente decorativo, apagable.

### 13.5 Checkboxes — Plan A

- [x] `UIElement::GetHitRect()` virtual (default = `m_ComputedRect`).
- [x] `UICanvas::HitTestRecursive` usa `GetHitRect()` (clip + inside).
- [x] `UIWindow::GetHitRect()` override (expande `m_ResizeBorderSize`, no si maximized).
- [x] Content a margen 0 en `OnLayoutChrome` + quitar la copia de color en `SetContent`.
- [x] `HitTestZone` opera sobre `GetHitRect()` (anillo exterior en la grilla 3×3); la title
      bar sigue usando el rect visual.
- [x] Borde visible: **Opción 3 — 4 quads via `BuildBatch`** en `RenderElement` (sin
      UIElement/`OwnsChild`), posicionado por `m_VisualBorderSize` + `m_BorderColor`.
- [x] Build limpio + smoke (hover del anillo exterior verificado en el log: `HitTest:
      UIWindowInternal (prev hover: IntWinBody)`) + verificación con el usuario pendiente
      (resize desde el anillo exterior, cursor en todos lados/esquinas, borde 1/2/4px on/off,
      content edge-to-edge).

---

## 14. Bug del grid (líneas chunk random) + Fijar sincronización multi-ventana (2026-09-02)

### 14.1 Diagnóstico confirmado (análisis 2026-09-02)

**Síntoma**: con la ventana externa (`Leir Test Window`) activa y el backend Vulkan,
el grid del viewport 3D muestra líneas **chunk** (10u, 100u — las gruesas que denotan
los grupos 10×) apareciendo en posiciones random, una a la vez, muy rápido, clipeadas
a rectángulos de pantalla. Las líneas finas (1u) nunca glitchean. Al subir la cámara
el glitch pasa de las 10u a las 100u (es el **nivel chunk activo** el que glitchea).
`NaN` del guard del grid = 0, y desactivar el near-plane clip no lo arregla.

**Causa raíz**: **write-after-read hazard de recursos dinámicos**. Todos los recursos
dinámicos del editor que se reescriben cada frame (grid vertex/UBO buffers,
`RenderPipeline` UBOs, `GizmoRenderer` buffers) se indexan con
`GetCurrentFrameIndex()` del `VulkanDevice` principal y confían en el fence del
principal (`MAX_FRAMES_IN_FLIGHT = 2`). Pero la ventana externa renderiza
**después** de `EndFrame()` del principal en el **mismo queue**, con su **propio**
ring de frames y compartiendo el **mismo command pool**. Con 2 rings de frames
independientes inyectando submits al mismo queue, el ring del principal ya no es el
único árbitro del progreso del GPU → el CPU escribe un slot de buffer que el GPU aún
lee → un vértice con coordenadas basura → una línea chunk desplazada a posición
random, clipeada al scissor del viewport.

**Confirmación empírica**: desactivar la creación y el render de la ventana externa
(`if (false)` en main.cpp) elimina el glitch por completo.

**CAUSA RAÍZ CONFIRMADA (verificado por el usuario, 2026-09-02)**: el **orden de
render** era el problema. La ventana externa se renderizaba **DESPUÉS de
`EndFrame()`** del principal, haciendo `vkQueueSubmit` en el **mismo queue** después
del submit del principal. Eso desincronizaba la relación entre el frame index del
device (que ya avanzó en `EndFrame`) y la disponibilidad real del buffer del grid:
el CPU reescribía un slot que el GPU aún leía del submit del principal anterior. Al
mover `m_TestWindow->RenderFrame()` **ANTES de `m_Backend->EndFrame()`** (dentro del
mismo frame lógico), el bug desaparece por completo. Las Fases 1 (3 frames) y 2
(command pool propio) ayudan pero NO eran la causa raíz — el fix definitivo es el
orden correcto: **todas las ventanas renderizan ANTES de presentar el principal**.

**Por qué solo las chunk**: son las líneas MÁS LARGAS (span = ventana×2, hasta 8000
unidades para 100u) → más vértices en el buffer → mayor probabilidad de que un
vértice caiga en la zona del race.

**Timeline semaphore (`VK_KHR_timeline_semaphore`) — NO aplica**: es core en Vulkan
1.2 y coordina múltiples queues/ventanas con un contador global, pero NO resuelve el
write-after-read de un buffer HostCoherent (el fence binario clásico ya lo protege;
el problema es que el grid esperaba el fence equivocado). Además rompe la
portabilidad del RHI (D3D12 usa `ID3D12Fence`, WebGPU/Metal otro modelo). Un fence
binario por slot existe en TODOS los backends → es la elección multiplataforma.

**Rendimiento**: el fix NO agrega sincronización costosa; hace que la existente sea
correcta (dedicada por slot, cubriendo todas las ventanas). Subir de 2→3 frames en
vuelo no reduce FPS (permite que el CPU corra 1 frame adelantado → menos stalling en
CPU-bound). El viewport RT NO se triplica (sigue siendo UN RT, escrito+leído dentro
del mismo submit, protegido por el fence del ring). NADA de `vkDeviceWaitIdle` por
frame. El costo es el mismo número de esperas de fence — solo que ahora esperan el
fence correcto.

### 14.2 Arquitectura objetivo (Fase 3 — patrón de la industria)

Motores profesionales (The Forge, Granite, bgfx, Unreal) usan un **único ring de
frames LÓGICO compartido**:

- **Un solo contador de frame lógico** que avanza UNA vez por frame del editor (no
  por ventana).
- Cada ventana tiene su propio command buffer / swapchain target / **command pool**
  propio, y graba dentro del frame lógico actual.
- Todos los recursos dinámicos CPU-escritos (grid, gizmos, UBOs per-frame) viven en
  el ring y se indexan por el frame lógico.
- Cada slot del ring tiene un **fence dedicado** que se señaliza cuando TODOS los
  submits de ese frame lógico (todas las ventanas que consumieron ese slot) terminan.

Esto escala a cualquier número de ventanas (2D/3D) y habilita grabación de comandos
en paralelo (cada ventana tiene su propio `GCommandGraph` → worker threads).

### 14.3 Fase 1 — Diagnóstico de confirmación (N=3 + re-activar la ventana externa)

- [x] 1.1 `MAX_FRAMES_IN_FLIGHT = 2 → 3` en `engine/include/LeirEngine/Rendering/VulkanDevice.h` (línea 199). El swapchain ya crea `minImageCount+1 = 3` imágenes → alinear N con el image count (estándar).
- [x] 1.2 `MAX_FRAMES_IN_FLIGHT = 2 → 3` en `engine/include/LeirEngine/Rendering/SwapchainTarget.h` (línea 155).
- [x] 1.3 Re-activar la ventana externa: `if (false)` → `if (vulkan)` en la creación (`main.cpp` OnInit) y `if (false && m_TestWindow)` → `if (m_TestWindow)` en OnRender.
- [x] 1.4 Build limpio + smoke test.
- [x] 1.5 Verificar con el usuario: grid sin glitch con la ventana externa abierta, a varias alturas de cámara (LOD 1/10 y 10/100).
- [x] 1.6 Fix crash Fase 1: arrays de recursos por-frame fijados a 2 actualizados a 3.
- [x] 1.7 **CAUSA RAÍZ CONFIRMADA (verificado por el usuario)**: el orden de render
      era el problema. La ventana externa DEBE renderizar ANTES de `EndFrame()` del
      principal. Renderizar DESPUÉS de `EndFrame()` hacía que el submit de la
      externa en el mismo queue desincronizara el fence del device principal,
      produciendo un write-after-read hazard en el buffer del grid (líneas chunk
      en posiciones random, clipeadas al scissor del viewport). Mover
      `m_TestWindow->RenderFrame()` ANTES de `m_Backend->EndFrame()` en `OnRender`
      elimina el glitch por completo.

> **Nota honesta**: esto probablemente lo mitiga (más margen en el ring) pero es un
> parche, no la causa raíz. La causa raíz estructural se resuelve en Fase 3.

### 14.4 Fase 2 — Desacoplar ventanas del pool/estado compartido

- [x] 2.1 Cada `SwapchainTarget` crea **su propio `VkCommandPool`** en vez de recibir el del main (hoy el ctor recibe `m_CommandPool` del device).
- [x] 2.2 El main device conserva su pool para sí. Ningún reset/alloc de una ventana puede afectar a otra.
- [x] 2.3 Verificar con el usuario: grid sin glitch con la ventana externa abierta (Fase 1 + 2 + orden de render corregido). **Confirmado por el usuario: "ahi anda bien".**

> Elimina la clase de bugs donde `vkResetCommandBuffer`/alloc de la externa
> interfiere con el pool que el principal usa para sus buffers de frame.

### 14.5 Fase 3 — Ring de frames LÓGICO compartido (decisión: Opción A — formalizar lo existente)

**Lección aprendida de Fase 1+2 (2026-09-02):** el fix definitivo del bug del grid
fue mover el render de la ventana externa **ANTES de `EndFrame()`** del principal.
Esto confirma que el principio correcto es: **todas las ventanas renderizan dentro
del mismo frame lógico, y el frame counter avanza UNA vez al final, después de
todos los submits.**

**Decisión (2026-09-02, Opción A):** el fix de orden + `MAX_FRAMES_IN_FLIGHT = 3`
**YA implementa el patrón FrameRing de la industria** (un fence por slot de frame,
esperado al inicio y señalizado en el submit del frame lógico, que cubre TODAS las
ventanas). La industria (Vulkan Samples, The Forge, Granite, bgfx, Unreal, Unity)
NO duplica el anillo de fences a nivel de editor — lo formaliza en la capa de
sincronización del backend (nuestro `VulkanDevice`). Por eso NO se crea un segundo
`FrameRing` con fences duplicados en el editor: sería redundante, añadiría riesgo de
regresión y no aporta robustez (el grid solo se dibuja en la principal; cada ventana
externa dibuja su propia UI/escena con su propio `SwapchainTarget`).

**Cómo escala a N ventanas (el fence del principal cubre a las externas):** todas
las externas hacen submit ANTES del submit del principal en el MISMO queue (FIFO).
Cuando el submit del principal N termina (fence[N] se señaliza), todo lo anterior en
el queue —las externas— ya terminó. Así, 3 frames después `BeginFrame(N)` espera
`fence[N]` y garantiza que TODAS las ventanas del frame N terminaron de leer sus
buffers. Un solo contador de frame lógico + fences por slot = sincronización
correcta para N ventanas, sin locks, sin waits extra, sin `WaitIdle` por frame.

#### 3.1 Regla de orden (ya aplicada en el fix)
- [x] 3.1.1 Toda ventana externa renderiza ANTES de `EndFrame()` del principal
      (dentro del mismo `BeginFrame`/`EndFrame`).
- [x] 3.1.2 El frame counter del device avanza UNA vez por frame lógico, después
      de que todas las ventanas grabaron y submitearon sus comandos.

#### 3.2 Exponer fences en el RHI (multiplataforma) — HECHO
- [x] 3.2.1 `RHIFence` en `engine/include/LeirEngine/RHI/RHI.h`:
      ```cpp
      struct RHIFence { Handle handle = 0; bool IsValid() const { return handle != 0; } };
      ```
- [x] 3.2.2 API en `engine/include/LeirEngine/RHI/RenderBackend.h`:
      ```cpp
      virtual RHIFence CreateFence(bool signaled = true) = 0;
      virtual void DestroyFence(RHIFence fence) = 0;
      virtual void WaitFence(RHIFence fence, uint64_t timeoutNs = UINT64_MAX) = 0;
      virtual void ResetFence(RHIFence fence) = 0;
      ```
- [x] 3.2.3 Implementación Vulkan (`VulkanBackend.cpp`): `VkFence` con
      `VK_FENCE_CREATE_SIGNALED_BIT` según `signaled`.
- [x] 3.2.4 Stubs D3D12/WebGPU: `CreateFence` devuelve `{}` (inválido), resto no-op.
      Listo para implementar `ID3D12Fence` cuando se retome D3D12.

#### 3.3 FrameRing duplicado en el editor — DESCARTADO (Opción A)
- [x] 3.3.1 Decisión: NO se crea un `FrameRing` RHI-neutral con fences propios duplicados.
      El `VulkanDevice` interno (fence por slot + `GetCurrentFrameIndex()`) ya es el
      FrameRing de industria; formalizarlo arriba sería redundante y arriesgado.
- [x] 3.3.2 El slot lógico del editor = `GetCurrentFrameIndex()` del backend (estable
      durante todo el frame lógico porque avanza UNA vez en `EndFrame`).

#### 3.4 Migrar recursos dinámicos al slot lógico — YA están sincronizados por el backend
- [x] 3.4.1 Grid vertex/UBO buffers: usan `GetCurrentFrameIndex()` (slot del frame lógico).
- [x] 3.4.2 `RenderPipeline` UBOs: idem.
- [x] 3.4.3 `GizmoRenderer` buffers: idem.
- [x] 3.4.4 `UIRenderer` vertex buffers: idem.
- [x] 3.4.5 `MAX_FRAMES_IN_FLIGHT = 3` en `VulkanDevice` y `SwapchainTarget` (ring de 3 slots).

#### 3.5 Command pool por ventana + grabación paralela
- [x] 3.5.1 Cada ventana con su propio `VkCommandPool` (Fase 2).
- [x] 3.5.2 Cada ventana graba su propio `GCommandGraph` en su propio command buffer
      (ya lo hace) → listo para grabación en worker threads.
- [x] 3.5.3 El fence del frame lógico (del principal) garantiza que el slot no se
      reescribe hasta que TODAS las ventanas del frame anterior completaron (FIFO).

#### 3.6 Verificación final
- [x] 3.6.1 Ventana externa abierta, grid sin glitch en Vulkan (Fase 1+2+orden).
- [x] 3.6.2 Apertura de 2 ventanas externas (una UI + una con viewport 3D/grid) sin glitch.
      Implementado: `m_TestWindow2` (segunda `UIWindowExternal` con UI verde) creada en
      OnInit, renderizada ANTES de `EndFrame()`, teardown en OnShutdown. Smoke test:
      `TestWinBody2` recibe hover, crashLog delta=0, shutdown limpio. **Confirmado por
      el usuario (2026-09-02): "anda todo perfecto"** — grid sin glitch con 2 ventanas.
- [ ] 3.6.3 Medir FPS antes/después (no debe bajar; puede subir en CPU-bound).
- [x] 3.6.4 Build + smoke test limpio (crashLog delta=0).

### 14.6 Lo que NO se va a hacer (por honestidad técnica)

- **`vkDeviceWaitIdle` por frame** — mataría el rendimiento.
- **`VK_KHR_timeline_semaphore`** — no resuelve este bug y rompe la portabilidad del RHI.
- **Triplicar el viewport RT** — innecesario y costaría ~32MB extra de GPU.
- **FrameRing duplicado en el editor** (Opción B) — redundante con el fence por slot
  del backend; riesgo de regresión sin beneficio. La industria formaliza el ring en
  la capa de sincronización, no lo re-implementa en el editor.

### 14.7 Pendiente futuro (fuera de Fase 3): desacople del frame rate de las externas

Con el modelo de frame lógico único, TODAS las ventanas quedan acopladas al ritmo de
la principal (si la principal va a 144hz y una externa está en un monitor 60hz con
vsync, el `acquire` de la externa puede bloquear ~16ms y frenar el frame lógico).
La solución de los editores grandes (Unity/Godot): la principal marca el ritmo y las
externas presentan con **mailbox** (o sin vsync) para no bloquear. Es un ajuste del
`present mode` de cada `SwapchainTarget` (el ctor ya recibe `vsync`), NO de la
sincronización de recursos. Se implementa cuando se necesite.

### 14.8 Fix crash AboutWindow (Help → About) — iterator invalidation en EventQueue

**Síntoma (2026-09-03)**: al hacer Help → About LeirEngine, la app crasheaba con
`0xC0000005`. Crash log:
```
EventQueue::Process → dispatch PointerEvent → _Func_class<void,PointerEvent const&>::operator() → 0xC0000005
```

**Causa raíz**: `EventQueue::Process()` iteraba los vectores de hooks
(`m_PointerHooks`, etc.) **directamente** (`for (auto& [id,h] : m_PointerHooks)`).
El About se crea DENTRO del callback del menú (un PointerEvent): `new AboutWindow` →
`Show()` → `ConnectToInputSystem()` → `AddPointerHook()` → `emplace_back` → el vector
se reasigna MIENTRAS `Process()` lo itera → iterador inválido → use-after-free al
invocar el siguiente hook. Las otras ventanas externas se crean en `OnInit` (fuera del
dispatch), por eso no crasheaban.

**Fix (raíz)**: `EventQueue::Process()` itera **snapshots** de los hooks
(`auto hooks = m_PointerHooks;`). Registrar/remover hooks durante el dispatch toma
efecto el próximo frame (comportamiento estándar de observer). Costo negligible
(copiar 3-5 `std::function` por frame). Aplica a Key/Pointer/Char/Scroll.

**Bug de layout descubierto durante la verificación**: el OK del About no se veía.
`UIWindowExternal::Show()` creaba la ventana nativa a **320×240 físico FIJO**,
ignorando `m_WindowSize` — el `SetSize({360,280})` del About nunca llegaba al GLFW
window, así que con contentScale 1.25 el canvas lógico quedaba 256×192 y el contenido
bajo y≈200 (el OK en y=210..240) quedaba fuera del área visible. **Fix**: `Show()`
crea la ventana a `m_WindowSize × scale` del monitor primario (mismo patrón HiDPI de
`CoreApplication`); el default 320×240 se conserva para las ventanas que no llaman
`SetSize`. `Close()` en el callback del OK queda como estaba (es correcto).

**Verificado**: build limpio + smoke (crashLog delta=0). `About LeirEngine created
(450x350)`, `HitTest: AboutOK`, click OK → cierra sin crash, eventos vuelven al
canvas del editor. Confirmado por el usuario.

### 14.9 Fix post-verificación (2026-09-03): X de ventanas externas + crash al re-abrir Help

El usuario confirmó 14.8 pero reportó dos bugs nuevos:

**(1) La X (botón de cerrar del OS) no cerraba las ventanas externas** (About + las
2 test windows). **Causa raíz**: `UIWindowExternal::Show()` nunca registraba
`glfwSetWindowCloseCallback` — GLFW solo seteaba el close flag y, como las ventanas
externas no se pollean (`CoreApplication` solo chequea el del main window), el X no
hacía nada. **Fix**: `glfwSetWindowCloseCallback` → `RequestClose()` (solo setea
`m_CloseRequested`) → `RenderFrame()` lo procesa y llama `Close()` **fuera del
callback de GLFW** (destruir el native window/swapchain/canvas dentro del callback
sería reentrante). La destrucción real ocurre en el frame siguiente.

**(2) Crash al re-clickear "Help" tras cerrar el About por OK**:
```
UICanvas::SetFocus → XConsole::Trace → FormatArg → std::basic_string ctor → strlen → 0xC0000005
```
**Causa raíz**: `UIContextMenu::RebuildItems()` borra las filas del menú
(`RemoveChild(row); delete row;`) sin limpiar el foco/hover del canvas. Al abrir el
About, el click en "About LeirEngine" deja `m_FocusElement` = esa `CtxItem`. Al
re-abrir Help, `OpenMenu` → `OpenAt` → `RebuildItems` libera la fila enfocada, y el
siguiente `SetFocus(target)` tracea `m_FocusElement->GetName()` → **dangling pointer
→ strlen sobre memoria reciclada** → crash. (El "click en cualquier otro lado primero"
lo enmascaraba porque `ClearFocus()` de área vacía nulleaba el foco antes del rebuild.)
**Fix**: `UIContextMenu::ClearCanvasRefs()` + `Contains()` (recursivo, incluye
submenús) — antes de liberar filas, si el foco/hover del canvas pertenece al árbol del
menú, se limpia (`ClearHoverAndFocus`). Mismo patrón que `DockManager`/
`UITreeView::ClearHoverAndFocus` ya usaban; `UIContextMenu` era el hueco.

**(3) Guard de About duplicado**: con el About ya visible, Help → About ahora hace
`BringToFront()` en vez de crear otra ventana (evita leak del window anterior).

**Verificado**: build limpio + smoke (crashLog delta=0, stderr vacío). Confirmado por
el usuario ("funciona perfecto"). Commits `…`/`…`.

### 14.10 Fix: minimizar una ventana externa congelaba el editor

**Síntoma (2026-09-03)**: al minimizar cualquiera de las ventanas externas, el editor
(programa principal) dejaba de actualizarse y de recibir eventos.

**Causa raíz**: al minimizar, GLFW dispara el framebuffer-size callback con 0×0 →
`SwapchainTarget::MarkResized()` → `m_NeedsResize = true`. En el siguiente frame,
`RenderFrame()` → `BeginFrame()` → `RecreateSwapchain()` → `glfwGetFramebufferSize`
devuelve **0×0** → el `while (w==0||h==0) glfwWaitEvents();` **bloquea esperando
eventos que nunca llegan** (la ventana minimizada no genera eventos) → hilo principal
congelado. Aun sin resize, `vkAcquireNextImageKHR(..., UINT64_MAX)` sobre una
swapchain minimizada también bloquearía (sin imágenes presentables).

**Fix (patrón de industria: skip render cuando está iconified)**:
- `SwapchainTarget::BeginFrame()`: `if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
  return false;` — no se adquiere ni se presenta mientras la ventana está minimizada;
  el callback de resize se vuelve a disparar al restaurar.
- `SwapchainTarget::RecreateSwapchain()`: guard defensivo — si la ventana está
  iconified con tamaño 0×0, se mantiene `m_NeedsResize` y se retorna sin el `while`
  bloqueante.

**Verificado**: build limpio + smoke (crashLog delta=0, sin entradas nuevas). El test
real de minimizar/restaurar lo hace el usuario.

---

## 11. Notas / Referencias

- El plan de `TODO_DOCKING.md` Fase 2 ya mencionaba: `SwapchainTarget` por ventana,
  input por ventana, `DockManager::FloatPanel` — esto lo completa y lo concreta.
- El viewport 3D ya vive en un `RenderTexture` compartido → las ventanas solo muestrean UI.
- A futuro: capa de windowing propia (borderless, sombras, snap) — anotado, no ahora.
- SVG renderer futuro para iconos (ver TODO_UI_MENU_BAR.md).
- **macOS/Linux**: hoy solo se compilan en CI (GitHub Actions, CI de 3 OS). El plan de
  portabilidad queda contemplado al final: la abstracción RHI-neutral de `SwapchainTarget`
  + input multi-ventana es idéntica en Linux/macOS (GLFW igual). La validación local de
  mac se hace vía CI hasta que el dev tenga un Mac. MoltenVK ya cubre el surface Vulkan
  en macOS (GLFW crea el surface), así que el external window debería funcionar igual.
- **Orden (decidido 2026-08-31)**: completar Vulkan primero (input multi-ventana → render
  real → AboutWindow → internal → detach de paneles), después D3D12 + WebGPU vía interfaz
  RHI-neutral, y finalmente validar/ajustar macOS/Linux vía CI.
