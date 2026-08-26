# TODO Inspector — componentes colapsables y reordenables

El Inspector muestra los componentes del objeto seleccionado, estilo Unity: cada componente
en su propia sección **colapsable** (flecha) y **reordenable** (drag para cambiar de
posición), con el **Transform siempre arriba y fijo** (no se puede mover).

---

## Estado actual

- `InspectorTransformPanel` (`editor/src/UI/InspectorTransformPanel.{h,cpp}`) muestra
  pos/rot/scale del objeto seleccionado con `UIDragFloatInput`s, hardcodeado al Cube.
- Se requiere: componentes dinámicos (los que tenga el objeto), colapsables, reordenables,
  y un **Transform2D** para Object2D/UI (además del Transform3D actual).

---

## Fases / Checkboxes

### Fase 1 — Secciones colapsables
- [ ] Widget `ComponentHeader` (o reusar un header genérico): flecha `> / v` + nombre del
      componente + toggle collapse/expand (por componente, estado persistido en el editor).
- [ ] Cada componente del objeto seleccionado se muestra en su sección, con su propio
      contenido (por ahora: Transform, y luego los demás con sus props).
- [ ] **Transform3D siempre arriba y fijo** (no colapsable por defecto, no reordenable).

### Fase 2 — Reordenar componentes
- [ ] Drag del header de un componente para cambiar su posición (CapturePointer + deadzone,
      patrón del treeview/dock).
- [ ] Reordenar el `m_Components` del `CoreObject` (agregar API `MoveComponent`/reorder al
      engine o manejar en el panel — engine es más limpio).
- [ ] Restricción: Transform no se puede mover de arriba.

### Fase 3 — Transform2D
- [ ] `Transform2D` (o variante en el panel) para Object2D/UI: Position XY, Rotation Z,
      Scale XY (con `UIDragFloatInput`).
- [ ] Se muestra el Transform correspondiente según la familia del objeto (3D vs 2D).

### Fase 4 — Componentes con props
- [ ] Render de props por tipo de componente (MeshRenderer: mesh/material; Light: type/color/
      intensity; Camera: fov/near/far; RigidBody/Collider/... según se agreguen).
- [ ] (después) edición de props → escribir al componente.

---

## Decisiones / Notas

- El Inspector reusa `UIDragFloatInput` y el patrón de collapsible del `UITreeView` (flecha).
- El orden de los componentes se persiste con el objeto (serialización — ver
  `TODO_FILE_SYSTEM.md`).
- Transform es fijo arriba, igual que Unity.

---