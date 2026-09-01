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
