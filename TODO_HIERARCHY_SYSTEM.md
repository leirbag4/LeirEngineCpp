# TODO Hierarchy System

Sistema de jerarquía del editor conectado a la `Scene` de LeirEngine. Reemplaza el panel
placeholder "Hierarchy" actual (`editor/src/main.cpp:453-468`) por un panel real que muestra
los objetos de la escena agrupados por **familia** (Object3D / Object2D / UI), con selección
bidireccional (gizmo ↔ inspector), renombrado inline, drag&drop de reparenting y soporte de
iconos por tipo (ver `TODO_UI_TREEVIEW.md` Fase 8).

Referencia del motor: `CoreObject` (`engine/.../Core/CoreObject.h`) — Transform, jerarquía
padre/hijos, componentes; `Object3D`/`Object2D` heredan de él; `Scene` (`engine/.../Scene/Scene.h`)
es dueña de `m_Objects` (lista plana) y la jerarquía es ortogonal (`GetParent()`/`GetChildren()`).

---

## Concepto

- Un **solo panel Hierarchy** muestra la escena activa, con **3 grupos de raíces** (uno por
  familia), cada uno con su propio árbol:
  ```
  [Object3D]
    └─ Camera
    └─ Light
    └─ Cube
        └─ Child
  [Object2D]
    └─ ... (raíces Object2D)
  [UI]
    └─ ... (raíces UI / UINode)
  ```
- **No se mezclan familias** (no hay `Object3D → UIElement`). Múltiples raíces por familia OK.
- Cada item lleva el **icono de su familia** (`#BFBFFF` Object3D, `#FF94C9` Object2D,
  `#96F1A2` UI) — ver `TODO_UI_TREEVIEW.md` Fase 8.
- Los objetos de tipo `UIElement` en la escena se modelan con **`UINode : CoreObject`** que
  envuelve un `UIElement` root (decisión confirmada: Opción b, sin refactor del framework UI).

---

## Fases / Checkboxes

### Fase 1 — Familias + guard de parenting (engine)
- [ ] `enum class ObjectFamily { Object3D, Object2D, UI }` en `CoreObject` (o `virtual GetFamily()`).
- [ ] `CoreObject::SetParent` / `AddChild` **rechazan cross-family** (con `XConsole::PrintWarning`,
      no crash). Confirma que `SetParent(worldPositionStays)` se mantiene para misma-familia.
- [ ] `UINode : CoreObject` (envuelve un `UIElement` root + layout/flex/anchoring). Ver
      `TODO_VIEWPORT_VIEW_MODES.md` para la vista UI.
- [ ] Build + tests engine (sin romper la escena actual).

### Fase 2 — HierarchyPanel (editor)
- [x] `editor/src/UI/HierarchyPanel.{h,cpp}`: envuelve un `UITreeView` + mapea
      `CoreObject* ↔ UITreeViewItem*` (mapa en el panel, sin tocar el engine).
- [x] **Poblar**: caminar `scene->GetObjects()`, raíces = `GetParent()==nullptr`, recursar
      `GetChildren()`. Item = texto `GetName()` + icono de familia.
- [x] **3 grupos de raíces** por familia (Object3D / Object2D / UI), colapsables, todos
      expandidos por defecto.
- [x] **Refresh** de la escena: reconstruir cuando cambia la estructura (conteo/firma) y
      sincronizar nombres. Detectar mutations sin re-Crear todo cada frame.
- [x] Reemplazar el placeholder en `main.cpp` (tab no-cerrable "Hierarchy"). Font + refresh
      en `OnUpdate`.

### Fase 3 — Selección (multi) + sync
- [x] `SetMultipleSelectionEnabled(true)` en el tree.
- [x] **Hierarchy → escena**: `SetOnSelectedItemsChanged` → `m_TransformGizmo.SetSelected(...)`
      + `m_InspectorTransformPanel.SetTargetObject(...)` (primario = Object3D más reciente).
- [x] **Escena → Hierarchy**: al cambiar `m_TransformGizmo.GetSelected()`, resaltar los items
      (`SetSelectedObjects`). Sync en `OnUpdate` del editor (con guards anti-loop).
- [ ] Al seleccionar de distinta familia → **cambiar la vista** del viewport (3D/2D/UI) —
      ver `TODO_VIEWPORT_VIEW_MODES.md` (P1; hoy el gizmo/inspector son Object3D-only).
- [x] Deseleccionar al clickear vacío (en el tree — core `UITreeView::OnPointerDown` — y en
      el viewport).

### Fase 4 — Rename inline + drag&drop
- [x] `SetEditable(true)` + `SetOnItemRenamed` → `obj->SetName(newName)` (el item ya actualiza su texto).
- [x] **Drag&drop (Onto)** → reparent: `SetOnItemDragged` → `child->SetParent(newParent)`
      (respetando familia — el guard del engine + validación del drag rechazan cross-family).
- [x] **Drag&drop (Below = reordenar hermanos)**: `CoreObject::InsertChildAt` (engine) +
      reordenar `m_Children`; el panel reordena los roots de familia vía `Scene::MoveObject`.
- [x] **3 zonas por fila (Kendo/estándar)**: borde superior = insertar ANTES (`DropMode::Above`),
      centro = nest (`Onto`), borde inferior = insertar DESPUÉS (`Below`). Índices **post-remoción**
      (el tree remueve primero) → reordenar en cualquier dirección es correcto. Callback del drag
      pasa a `(items, targetItem, mode)`.
- [x] Al reparentar, actualizar el mapa CoreObject↔Item y el refresco. **Sin flicker**: en drops
      sobre objetos reales se salta el rebuild (el tree ya refleja el cambio); en drops sobre
      raíces de familia se rebuilda (el tree re-sincroniza). **Fix crash**: `ClearItems` desprende
      TODOS los items + `RebuildAll` limpia el hover/focus del canvas (use-after-free).

### Fase 5 — "Convert to Atom" / preview de families
- [ ] Integrar `TODO_ATOM.md` (Convert to Atom, Unpack, Isolate, Open Separately).
- [ ] Icono/color especial para atoms (`#BFCFFF`) + colapsados sin flecha.
- [ ] Integrar `TODO_UI_CONTEXT_MENU.md` (click derecho en items).

---

## Decisiones / Notas

- **Selección MULTI** confirmada (no single).
- **Familias**: guard en el engine Y validación en el drag del editor (ambos confirmados).
- **UIElement en escena = UINode** (Opción b) confirmada.
- El panel Hierarchy reusa el `UITreeView` virtualizado (miles de objetos sin problema).
- Los tabs de escenas (Godot-style) afectan qué escena muestra el Hierarchy — ver
  `TODO_VIEWPORT_VIEW_MODES.md`.
- **Fase 2 requirió un fix del core de layout**: el `HierarchyPanel` (Free + Stretch)
  exponía la acumulación de `ComputeFreeLayout` (`child->m_Rect.offset += parent.xy` cada
  frame → los elementos volaban). Se arregló de raíz con el parámetro `parentOffset`
  (ver `TODO_COMPUTE_FREE_LAYOUT_FIX.md`). El panel quedó simple (Stretch), sin workaround.
- **Paso 2.5 (2026-08-27)**: el panel es Column con un header arriba (`#55555E` lineal —
  ver nota de color abajo) con botón "+" (placeholder del `UIContextMenu` futuro) + input
  de filtro (`Fill` → sigue al splitter). **El filtrado es Godot-style y vive en el CORE**:
  `UITreeView::SetFilter` + flags `SetTreeFiltered`/`SetFilterExcluded` — sin rebuild, sin
  parpadeo, selección/expansión conservadas. **Los colores de UI son LINEALES** (el RTV
  `UNORM_SRGB` encoda a sRGB al guardar): un literal `#55555E` (0.333 lineal) se muestra
  ~#9C9CA4; para un gris así se usa el valor lineal del `TreeViewDebugPanel` `{0.08,0.08,0.10}`.

---