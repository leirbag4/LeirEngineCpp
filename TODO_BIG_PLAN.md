# TODO Big Plan — LeirEngine Editor

Plan maestro modular. Cada item apunta a su `.md` con el detalle completo y sus propios
checkboxes. Este archivo solo agrega/agenda prioridades y el estado global.

Estado: **en planificación** (2026-08-26). Vamos fase por fase, probando cada una.
Actualizado 2026-08-28: Hybrid ECS Fases 0-3 COMPLETAS, P0 (hierarchy) completo, ContextMenu hecho,
P1 en curso.

---

## Arquitectura transversal — Hybrid ECS (Fases 0-3 COMPLETAS)

El motor entero (editor + runtime + web + mobile futuro) se rediseña a **escena amigable en la
superficie + ECS data-oriented invisible por detrás** (SIMD + multithreading). La API pública
(`CoreObject`, `AddChild/GetChildren/SetParent`, `AddComponent<T>`) **no cambia**. Detalle completo,
análisis de almacenamiento (sparse-set + owned groups SoA), SIMD por plataforma, fases y checkboxes en
→ **`TODO_HYBRID_ECS.md`**.

- [x] **Fase 0 — Refactor data-oriented del código actual** → `TODO_HYBRID_ECS.md` §10
  - [x] Fix render solo-roots (hijos con MeshRenderer invisibles).
  - [x] Registro de componentes por `type_index` (matar `dynamic_cast`).
  - [x] Caches de escena (renderables/cámaras/luces) — mata el O(N×dynamic_cast) por frame.
  - [x] Aislar el almacenamiento de Scene detrás de `ISceneStorage`.
- [x] **Fase 1 — Núcleo ECS custom COMPLETO y activo** (entity, pools sparse-set, journal, groups,
      systems, transform system con lossy-preserve, hierarchy tree unificado, **bridge Etapa A**).
  - [x] Núcleo base: Entity generacional + pools sparse-set + registro por type_index + journal + `Each`
        variadic → `tests/ECSTest.cpp` (ctest 3/3).
  - [x] Owned groups / query cache (`OwnedGroup<Ts...>`), journal-synced, iteración O(miembros).
  - [x] Hierarchy tree unificado (`HierarchyTree`) + Transform system (`LocalTransform`/`WorldTransform`
        con dirty-frontier y lossy-preserve exacto) → `ECSTest`.
  - [x] Systems pipeline (`ISystem`/`SystemPipeline` Fixed/Update/Render) + `CommandBuffer` (cambios
        estructurales diferidos) → `ECSTest`.
  - [x] **Bridge Etapa A**: `Scene` = implementación ECS (backing de transforms + `HybridComponent`s +
        lifecycle) — el editor/demos/web corren sobre el ECS; `ECSScene` y el storage OOP de
        componentes eliminados. Verificado por el usuario en d3d12/vulkan/webgpu.
- [x] **Fase 2 — SIMD + multithreading COMPLETA** (wrappers SIMD en Math, `MultiplySimd`, `JobSystem`,
      scheduler paralelo por dependencias de acceso, `CommandBuffer` en sync points, **render list
      vectorizado** con frustum cull SIMD, **incrementos 1-5** de componentes a data, benchmarks de
      escala §11) → `TODO_HYBRID_ECS.md` §5/§10/§11.
- [x] **Fase 3 — Tier advanced ECS COMPLETA** (API pública del `World` con **nombres propios**, demo
      renderless `ECSPublicDemo`, docs `docs/ecs-public-api.html` + `docs/hybrid-ecs.html`; el camino
      OOP sigue siendo el default documentado).

---

## Fundaciones del Hierarchy (P0)

- [x] **Fase 0.1 — Iconos del TreeView** → `TODO_UI_TREEVIEW.md`
  - Hecho: slot de icono + toggle `SetIconsEnabled`, cache por hash (`UITextureCache` con HiDPI),
    PNGs en `assets/icons/` (object3d/object2d/uielement).
- [x] **Fase 0.2 — HierarchyPanel real** → `TODO_HIERARCHY_SYSTEM.md`
  - Hecho: panel conectado a la `Scene`, raíces por familia, selección **multi** bidireccional
    (gizmo ↔ inspector), rename F2, drag&drop de 3 zonas, filtro de búsqueda, botón "+" con
    ContextMenu.

## Editor multipropósito (P1)

- [ ] **Familias de objetos + guard de parenting** → `TODO_HIERARCHY_SYSTEM.md` (⚠ parcial)
  - ✅ hecho: validación cross-family en el drag del hierarchy (editor).
  - ⏳ pendiente: `ObjectFamily` en `CoreObject` + `SetParent` rechaza cross-family (engine) +
    `UINode` para UI.
- [x] **ContextMenu** → `TODO_UI_CONTEXT_MENU.md`
  - Hecho: `UIContextMenu` (popup al click derecho/+, overlay, se cierra fuera/ESC); el botón "+"
    del HierarchyPanel lo abre con Object3D/… wired en `main.cpp`.
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

✅ Hechos: Fase 0.1 (iconos), Fase 0.2 (HierarchyPanel), ContextMenu, Hybrid ECS Fase 0/1/2/3.

Pendientes (orden sugerido):
1. Familias/guard en el engine (`ObjectFamily`/`UINode`) + guard cross-family en `SetParent`.
2. UIMenuBar + UIMenuBarItem (File/Save All, File/New Scene, Help/About).
3. Tabs de escenas (Godot-style) + vistas 2D/3D/UI + botones de toolbar.
4. Viewport 2D (cámara ortográfica + grid 2D + rulers + gizmos 2D).
5. Atoms + File Explorer de guardado.
6. Inspector avanzado (componentes colapsables/reordenables, Transform2D/3D).
7. Serialización + metadata (.mdata) + panel Contents.
8. Vista UI (layout/flex/anchoring).
9. UIDocuments + LXML (futuro).

---