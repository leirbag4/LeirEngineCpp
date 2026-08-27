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
- [ ] `SetMultipleSelectionEnabled(true)` en el tree.
- [ ] **Hierarchy → escena**: `SetOnSelectedItemsChanged` → `m_TransformGizmo.SetSelectedItems(...)`
      + `m_InspectorTransformPanel.SetTargetObject(...)` (para el primero / multi→ último click).
- [ ] **Escena → Hierarchy**: al cambiar `m_TransformGizmo.GetSelected()`, resaltar los items
      correspondientes (`SetSelectedItems`). Sync en `OnUpdate` del editor.
- [ ] Al seleccionar de distinta familia → **cambiar la vista** del viewport (3D/2D/UI) —
      ver `TODO_VIEWPORT_VIEW_MODES.md`.
- [ ] Deseleccionar al clickear vacío.

### Fase 4 — Rename inline + drag&drop
- [ ] `SetEditable(true)` + `SetOnItemRenamed` → `obj->SetName(newName)` (el item ya actualiza su texto).
- [ ] **Drag&drop (Onto)** → reparent: `SetOnItemDragged` → `child->SetParent(newParent)`
      (respetando familia — el guard del engine + validación del drag rechazan cross-family).
- [ ] **Drag&drop (Below = reordenar hermanos)**: requiere reordenar `m_Children` de CoreObject
      (agregar `InsertChildAt`/reorder al engine, chico) — opcional para esta fase.
- [ ] Al reparentar, actualizar el mapa CoreObject↔Item y el refresco.

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

---