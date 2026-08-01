# TODO: Sistema de docking profesional (tabs + anidamiento)

Milestone Fase 1 (ventana única). El multi-window (ventanas flotantes del SO) es Fase 2
de este documento. Estado: **Fase 1-3 completas**. Teardown de Vulkan verificado
(`VUID-vkDestroyDevice-device-05137` resuelto, ver `TODO.md`).

---

## Objetivo

Editor con docking estilo Unity / Visual Studio / Godot: paneles con pestañas, grupos
anidados (tab-merge y splits), drag & drop con zonas de drop, y persistencia del layout
en `settings.json`. Los paneles actuales (Hierarchy, Viewport, Inspector, paneles debug)
pasan a ser dockeables.

## Diseño

### Modelo de datos = árbol de UI

Cada nodo del árbol de dock es también un `UIElement`/`UIPanel`, así hereda layout,
hit-test, input y render del UI system existente.

```
DockPanel        → { id, title, content: UIElement*, closeable }  // contenido real
DockNode         → base (UIPanel)
├── DockSplitNode → { orientation H|V, children[], ratios[] }     // split VS
└── DockPane      → { tabs[], activeIndex }                        // grupo con tabs
                   Column: [DockTabBar][content host]
DockTabBar       → Row de pestañas (label + close), drag desde pestaña
DockManager      → raíz del árbol + operaciones + drag&drop + serialización
DockDropOverlay  → feedback visual (ghost + zona destacada) en la capa overlay
```

- **Contenido**: cada `DockPanel` envuelve el subtree real (`InspectorTransformPanel`,
  `UIViewportPanel` del viewport, paneles debug). Moverse = **reparentar** (`AddChild`
  ya des-parenta solo). El pane muestra solo el tab activo (`SetActive`).
- **Layout de splits**: `DockSplitNode::ComputeLayout` posiciona hijos por `ratios[]`
  con splitters de 6px. Necesita `UIElement::ComputeLayout` **virtual** (el layout
  estándar reparte `Fill` por igual).
- **Drag & drop**: `DockTab::OnPointerDown` activa el tab y captura al `DockManager`
  (patrón `CapturePointer`). El manager computa el nodo bajo el cursor con
  `FindNodeAt` (recorrido manual del árbol, inmune al overlay) y la zona según la
  posición dentro del rect del pane (centro 50% = Tab; bordes = split). El overlay se
  dibuja en la capa overlay (nueva flag `SetOverlayLayer`).

### Operaciones del árbol

| Operación | Descripción |
|---|---|
| **Split (borde)** | Insertar `DockSplitNode` `[nuevoPane, target]` según zona, reemplazando a target en su contenedor (o raíz). |
| **Tab-merge (centro)** | `targetPane->AddTab(panel)` (contenido reparenteado, tab activo). |
| **Close** | Sacar el panel del pane; si el pane queda vacío se colapsa; si el split padre queda con 1 hijo, se colapsa recursivamente. |
| **Collapse** | Reemplazar un split con su único hijo restante (recursivo). |

- Paneles core (Hierarchy/Viewport/Inspector) **no** closeables en Fase 1 (sin menú para
  reabrirlos). Los debug sí.
- Ownership: `DockManager` es dueño de los `DockPanel` (`unique_ptr`); los nodos de UI se
  borran al salir del árbol.

### Drag & drop (flujo)

1. `DockTab::OnPointerDown` → activa tab, registra press en el manager, captura pointer.
2. `DockManager::OnPointerMove`: superado umbral ~4px → `m_Dragging`; actualiza hover
   node/zone (FindNodeAt) y posiciona el overlay (ghost + highlight).
3. `DockManager::OnPointerUp`: aplicar split/merge o cancelar; ocultar overlay; release.
   Fuera de cualquier pane → cancelar (Fase 1) / flotar (Fase 2).

### Gestos (drop sobre el pane del propio tab)

- **Reorder (tab bar del mismo pane, ≥2 tabs)**: `DockPane::ReorderTabTo(panel, pos)`
  reinserta el tab por X contra los centros de los tabs hermanos
  (`UIElement::InsertChildAt` + `DockTabBar::InsertTab` + `DockPane::InsertTab`). El
  highlight de zona se oculta durante el gesto (solo ghost).
- **Split en pane propio compartido (bordes, ≥2 tabs)**: el guard de `SplitPane`
  (`target->Contains(panel)`) solo corto-circuita cuando `GetTabCount() <= 1` (self-drop
  puro). Con ≥2 tabs, los bordes parten la columna y el tab arrastrado pasa a la nueva
  zona; los hermanos se quedan.
- **Center en el pane propio**: solo enfoca el tab (no-op).

---

## Fases

### Fase 0 — Prereqs de engine ✅
- `UIElement::ComputeLayout` → `virtual` (los nodos de dock necesitan control total).
- `UIElement::SetOverlayLayer(bool)` + `UIRenderer` enruta al batch superior por flag
  (se eliminó la heurística por prefijo "Debug"; `UIDebugOverlay` usa el flag explícito).
- `DockSplitter`: splitter ratio-based (arrastrar ajusta `ratios[]` del split padre,
  clamps por ratio mínimo 0.05). Reemplaza al `UISplitter` del editor.

### Fase 1 — Núcleo del dock (engine, `UI/Dock/`) ✅
- `DockPanel`, `DockNode`, `DockSplitNode`, `DockPane`, `DockTabBar`/`DockTab`.
- `DockManager`: operaciones del árbol + drag & drop + `FindPaneAt`/`ComputeZone` +
  serialización JSON (nlohmann).
- `DockDropOverlay`.
- `engine/CMakeLists.txt`: nuevos fuentes.

### Fase 2 — Migración del editor + persistencia ✅
- `main.cpp`: reemplazar `ApplyPanelLayout`/`UISplitter`/anchos por `DockManager` raíz
  (Stretch, offset `0,0–0,-30` para dejar la BottomBar).
- Layout default:
  - Split horizontal: `[Hierarchy] | [split vertical: [Viewport] | [TestPanel|CameraTestPanel|DebugTextPanel|TextAreaDebugPanel (tabs)]] | [Inspector]`
  - Ratios iniciales: 0.17 / 0.66 / 0.17; vertical 0.8/0.2.
- Registro de paneles (contenido = subtrees existentes); `Refresh()` de los debug sigue
  igual en `OnUpdate` (+ `m_DockManager->Process()` para closes diferidos).
- `UpdateViewportRenderTarget` sigue leyendo `m_ViewportPanel->GetComputedRect()`.
- `inViewport` (EditorCamera): caminar ancestros del hovered hasta `m_ViewportPanel`.
- `settings.json`: sección `dock` (árbol serializado). Save en drag-end/close/shutdown;
  load al iniciar; si falta/inválido → layout default.

### Fase 3 — Verificación ✅
- Build engine + editor ✅ (ambos targets OK).
- Persistencia round-trip ✅ (salir/reabrir; ratios estables tras fix de
  `AddNode`/`NormalizeRatios`; verificado en runtime).
- Tab-merge ✅, splits 4 direcciones ✅, anidamiento ✅, close tabs debug ✅,
  drag sin romper capture ✅, clamps de ratios ✅.
- **Reorder de tabs en el mismo pane** ✅ (drop sobre la tab bar del pane propio con ≥2
  tabs; `ReorderTabTo` reinserta por X). Verificado manualmente.
- **Split en el pane propio compartido** ✅ (borde de un pane con ≥2 tabs parte la
  columna; guard de `SplitPane` ahora solo aplica a self-drop puro `GetTabCount() <= 1`).
  Verificado manualmente.
- Pendiente: resize de ventana, HiDPI, viewport RT + camera sync (verificado en sesiones
  previas), docs AGENTS.md (verificado). Teardown de Vulkan ✅ (2 corridas limpias con
  validation layers; fixes en `Material::RecreatePipeline` y liberación de subtrees de
  contenido en `OnShutdown` — ver `TODO.md`).

### Fase 2 del milestone — Multi-window (flotantes), NO incluida en esta iteración
- Refactor de `VulkanDevice` a swapchain por ventana (`SwapchainTarget`; device/queues
  compartidos). Ventajas: la escena 3D ya vive en un `RenderTexture` compartido → las
  ventanas flotantes solo renderizan UI muestreando el RT.
- Input por ventana (eventos etiquetados con `GLFWwindow*`, ruteo por canvas) y content
  scale por ventana.
- `DockManager::FloatPanel` / re-dock al cerrar la ventana; persistir posiciones.

---

## Referencias de código

- `engine/include/LeirEngine/UI/Dock/*.h` / `engine/src/UI/Dock/*.cpp` — implementación
  del dock (DockManager, DockSplitNode, DockPane, DockTabBar/DockTab, DockSplitter,
  DockDropOverlay, DockPanel, DockNode).
- `engine/include/LeirEngine/UI/UIElement.h` / `engine/src/UI/UIElement.cpp` — layout
  (Row/Column reparten `Fill` por igual → ratios requieren `ComputeLayout` virtual).
- `engine/src/UI/UIRenderer.cpp` — capas (regular → viewport → overlay) por flag
  `IsOverlayLayer`; render del `DockTab` (título + close "x").
- `editor/src/main.cpp` — migración: `RegisterPanel`, `BuildDefaultLayout`/`LoadLayout`,
  `SetOnLayoutChanged` (persistencia), `OnShutdown` (serializa + destruye el dock),
  `UpdateViewportRenderTarget`, hover viewport por ancestros.
- `engine/src/UI/UIDebugOverlay.cpp` — overlay explícito (`SetOverlayLayer(true)`).
- `engine/include/LeirEngine/Core/Settings.h` — sección `dock.layout` (JSON string).
