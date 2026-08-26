# TODO File System — serialización, metadata, Contents, LXML

Sistema de archivos del proyecto: serialización de escenas/UIDocuments/Atoms, metadatos
`.mdata`, panel "Contents" y (a futuro) LXML.

---

## Decisiones confirmadas

- **Formatos por ahora JSON** (debuggeable, ya tenemos `nlohmann/json`):
  - `.scene3D` — escena 3D.
  - `.scene2D` — escena 2D.
  - `.uidoc` — documento de UI (UIDocument / UINodes).
  - `.atom` — prefab/Atom (ver `TODO_ATOM.md`).
- **Metadatos `.mdata`**: archivo con el **mismo nombre al lado del original** + `.mdata`
  al final (estilo Unity/Godot), p.ej. `cube.scene3d.mdata`.
- **Contents**: panel dockeable ("Contents") que lista los archivos del proyecto usando el
  treeview + iconos (ver `TODO_UI_TREEVIEW.md` Fase 8).
- **LXML (Leir XML)**: layout declarativo estilo HTML / Unity UI Toolkit — **a futuro**.

---

## Fases / Checkboxes

### Fase 1 — Metadatos `.mdata`
- [ ] Helper `AssetMetadata` (editor): cargar/crear `.mdata` al lado del asset.
- [ ] Campos base: `{ "type": "...", "version": 1, "guid": "uuid", "dependencies": [...] }`.
- [ ] Crear `.mdata` automáticamente al guardar un asset nuevo.
- [ ] Leer `.mdata` al listar en Contents (para el icono/tipo).

### Fase 2 — Serialización de escenas
- [ ] `.scene3D`: nombre, cámara activa, lista de objetos (name, uuid, family, transform
      local pos/rot/scale, parent uuid, componentes con sus props, atom refs).
- [ ] `.scene2D`: igual + sorting/order por objeto 2D.
- [ ] Cargar/guardar escenas activas desde los tabs (ver `TODO_VIEWPORT_VIEW_MODES.md`).
- [ ] JSON schema simple y versionado (campo `version`).

### Fase 3 — UIDocument (`.uidoc`)
- [ ] `UIDocument` = archivo de UI (UINodes + layout/flex/anchoring) — ver
      `TODO_VIEWPORT_VIEW_MODES.md` (vista UI).
- [ ] Serialización del árbol de UINodes a `.uidoc` (JSON).
- [ ] Instanciar un `.uidoc` en una escena (componente `UIViewport`/referencia).

### Fase 4 — Atoms (`.atom`) — ver TODO_ATOM.md
- [ ] Guardar un subtree como `.atom` (vía el File Explorer, `TODO_UI_FILE_EXPLORER.md`).
- [ ] Instanciar un `.atom` en la escena (semántica prefab: instancias independientes,
      hijos editables por instancia, `#BFCFFF`).

### Fase 5 — Panel "Contents"
- [ ] Panel dockeable "Contents" (tab "Contents").
- [ ] Lista los archivos del proyecto con el treeview (carpetas expandibles + archivos
      con icono por tipo: scene3d/scene2d/uidoc/atom/png/audio/...).
- [ ] Doble click en un `.scene*`/`.uidoc` → abrir en un tab (ver `TODO_VIEWPORT_VIEW_MODES.md`).
- [ ] Drag de un asset al hierarchy → instanciar (después).

### Fase 6 — LXML (futuro)
- [ ] **LXML (Leir XML)**: documento de texto plano para layout declarativo de UI
      (estilo HTML / Unity UI Toolkit). Guardado para más adelante.
- [ ] Parser + runtime que instancie el árbol de UIElements desde el documento.

---

## Decisiones / Notas

- Todo JSON por ahora (fácil de debuggear); binario con cereal se evalúa después.
- El `.mdata` se ubica al lado del asset original (mismo nombre + `.mdata`).
- "Contents" reusa el treeview + iconos; el grid con iconos grandes es futuro.

---