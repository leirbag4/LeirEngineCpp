# TODO Big Plan — LeirEngine Editor

Plan maestro modular. Cada item apunta a su `.md` con el detalle completo y sus propios
checkboxes. Este archivo solo agrega/agenda prioridades y el estado global.

Estado: **en planificación** (2026-08-26). Vamos fase por fase, probando cada una.

---

## Fundaciones del Hierarchy (P0)

- [ ] **Fase 0.1 — Iconos del TreeView** → `TODO_UI_TREEVIEW.md`
  - Slot de icono a la izquierda del texto, toggle `SetIconsEnabled`, registry hash-cache,
    PNGs 12×12 en `assets/icons/` (object3d/object2d/uielement por ahora).
- [ ] **Fase 0.2 — HierarchyPanel real** → `TODO_HIERARCHY_SYSTEM.md`
  - Panel conectado a la `Scene`, raíces separadas por familia (Object3D/Object2D/UI),
    selección **multi** bidireccional (gizmo ↔ inspector), rename F2, drag&drop.

## Editor multipropósito (P1)

- [ ] **Familias de objetos + guard de parenting** → `TODO_HIERARCHY_SYSTEM.md`
  - `ObjectFamily` en `CoreObject`, `SetParent` rechaza cross-family (engine) + validación
    en el drag del hierarchy (editor). UINode para UI.
- [ ] **ContextMenu** → `TODO_UI_CONTEXT_MENU.md`
  - `UIContextMenu` (popup al click derecho, overlay, se cierra fuera/ESC).
- [ ] **UIMenuBar + UIMenuBarItem** → `TODO_UI_MENU_BAR.md`
  - WPF-style: barra arriba con File/Save All, File/New Scene, File/Save Scene, Help/About.
- [ ] **Tabs de escenas (Godot-style)** → `TODO_VIEWPORT_VIEW_MODES.md`
  - Tabs arriba del viewport con nombre de escena, cruz para cerrar (≥1), botón "+" para
    nueva escena ("untitled N", renombrar con Ctrl+S).
- [ ] **Vistas 2D / 3D / UI + botones de toolbar** → `TODO_VIEWPORT_VIEW_MODES.md`
  - Botones 2D/3D/UI a la derecha de Global/Local; auto-seleccionan por tab/objeto,
    override manual permitido. Cada escena/tab tiene su vista.
- [ ] **Viewport 2D** → `TODO_VIEWPORT_VIEW_MODES.md`
  - Cámara ortográfica + grid 2D + rulers a los costados + gizmos 2D (traslación/rotación/escala).
- [ ] **Vista UI (layout)** → `TODO_VIEWPORT_VIEW_MODES.md`
  - Edición de layout/flex/anchoring; UINodes aislados con overlay sobre escena 3D/2D.

## Atoms (P2)

- [ ] **Sistema de Atoms (prefabs)** → `TODO_ATOM.md`
  - Convert to Atom / Unpack / Isolate mode / Open Separately / Nested. Color texto `#BFCFFF`,
    colapsado sin flecha. Instancias independientes (semántica Unity prefab).
- [ ] **Serialización de Atoms (`.atom`)** → `TODO_ATOM.md` + `TODO_UI_FILE_EXPLORER.md`
  - Guardar/abrir `.atom` como asset en "Contents", vía el File Explorer.

## Inspector (P2)

- [ ] **Componentes colapsables + reordenables** → `TODO_INSPECTOR.md`
  - Flecha para colapsar, drag para reordenar. **Transform siempre arriba y fijo**.
- [ ] **Transform2D / Transform3D** → `TODO_INSPECTOR.md`

## Serialización + archivos (P3)

- [ ] **Formatos `.scene2D` / `.scene3D` / `.uidoc` (JSON)** → `TODO_FILE_SYSTEM.md`
- [ ] **Sistema de metadatos `.mdata`** → `TODO_FILE_SYSTEM.md`
  - Archivo `.mdata` al lado del asset (mismo nombre + `.mdata`), estilo Unity/Godot.
- [ ] **Panel "Contents"** → `TODO_FILE_SYSTEM.md`
  - Panel dockeable "Contents": lista de archivos del proyecto con el treeview + iconos.
- [ ] **File Explorer (ventana genérica Save/Open)** → `TODO_UI_FILE_EXPLORER.md`
  - Barra de título, treeview izquierda (carpetas), derecha archivos (lista o grid), Cancel/Save.

## UI Documents + LXML (P4 / futuro)

- [ ] **UIDocument (`.uidoc`) + vista UI** → `TODO_FILE_SYSTEM.md`
- [ ] **LXML (Leir XML) — layout declarativo estilo HTML/Unity UI Toolkit** → `TODO_FILE_SYSTEM.md`
  - Guardado para más adelante.

---

## Próximos pasos (orden sugerido)

1. Fase 0.1 — Iconos del TreeView (autocontenido, desbloquea todo).
2. Fase 0.2 — HierarchyPanel real (selección multi, familias, rename, drag).
3. Familias/guard en CoreObject + UINode.
4. ContextMenu + UIMenuBar.
5. Tabs de escenas + vistas 2D/3D/UI.
6. Atoms + File Explorer de guardado.
7. Inspector avanzado.
8. Serialización + metadata + Contents.
9. UIDocuments + LXML (futuro).

---