# TODO Viewport View Modes — 2D / 3D / UI + tabs de escenas

El editor tiene hoy **un solo viewport 3D** (`UIViewportPanel` + `RenderTexture`). El objetivo
es soportar **3 modos de vista** (3D / 2D / UI), **tabs de escenas estilo Godot** arriba del
viewport, y **botones 2D/3D/UI** en la toolbar. Cada escena/tab tiene su propia vista.

---

## Decisiones confirmadas

- Botones 2D/3D/UI a la derecha de Global/Local en la toolbar. **Auto-seleccionan** según el
  tab activo y el objeto seleccionado; **override manual permitido** (preview, p.ej. top-down).
- Tabs de escenas arriba del viewport: nombre de la escena, cruz para cerrar (siempre ≥1),
  botón "+" para nueva escena ("untitled 0/1/2…", renombrar con Ctrl+S).
- Cada escena/tab tiene su vista: `.scene3D` → 3D, `.scene2D` → 2D, `.uidoc` → UI.
- Al seleccionar un objeto de otra familia, la vista cambia (Godot-style): Object3D → 3D,
  Object2D → 2D, UI → vista UI aislada (con overlay sobre la escena 3D/2D opaca).

---

## Fases / Checkboxes

### Fase 1 — Botones 2D/3D/UI (toolbar)
- [ ] Extender `ToolbarPanel` con 3 botones/labels 2D/3D/UI a la derecha de Global/Local.
- [ ] Estado seleccionado (resaltado) refleja la vista actual; los demás habilitados.
- [ ] `SetOnViewChanged(std::function<void(ViewMode)>)` → el editor cambia la vista.
- [ ] El editor setea el estado del botón según tab/objeto seleccionado.

### Fase 2 — Tabs de escenas (Godot-style)
- [ ] Widget `SceneTabBar` (nuevo, arriba del viewport) — distinto del DockTabBar:
      tabs con nombre de escena, cruz para cerrar, botón "+".
- [ ] `SceneManager` multicambia: varios `Scene`/`UIDocument` abiertos; uno activo.
- [ ] Al cambiar de tab: la vista cambia (botón correspondiente) y el Hierarchy muestra esa escena.
- [ ] Al cerrar: si era la última, crear una vacía ("untitled N").
- [ ] "+" → nueva escena "untitled N" (Ctrl+S la renombra/guarda).

### Fase 3 — Viewport 2D
- [ ] Cámara **ortográfica** 2D + proyección ortográfica (aspect del viewport).
- [ ] **Grid 2D** (nuevo, estilo cuadrícula con ejes) — reuse de la técnica del grid 3D.
- [ ] **Rulers** a los costados (reglas con unidades) — nuevo widget overlay.
- [ ] **Gizmos 2D**: traslación/rotación/escala 2D (nuevo `TransformGizmo2D`), en el plano XY.
- [ ] Background distinto (gris más claro o checker) para diferenciar del 3D.

### Fase 4 — Vista UI (layout)
- [ ] Viewport UI: edita el árbol de `UINode`/UIElements (layout, flex, anchoring).
- [ ] `UINode : CoreObject` envuelve un `UIElement` root (ver `TODO_HIERARCHY_SYSTEM.md`).
- [ ] Doble click en un UINode/UIDocument dentro de una escena 3D/2D → vista UI aislada con
      **overlay oscuro** sobre el 3D/2D; doble click fuera o ESC → vuelve a la vista anterior.
- [ ] Muestra los límites del layout, anchoring handles, y el sistema flex ya existente.

### Fase 5 — Instanciar escenas/UIDocuments en tabs
- [ ] Doble click en un asset `.scene*`/`.uidoc` en "Contents" → abre un tab (o selecciona el
      existente si ya está abierto).
- [ ] Escenas anidadas (una escena contiene otra) — icono propio en hierarchy, hijos no
      visibles, doble click abre/selecta el tab (ver `TODO_FILE_SYSTEM.md`).

---

## Decisiones / Notas

- El tab "Viewport" del dock pasa a ser el **visor de la escena activa**; su título debería
  reflejar el nombre de la escena (o los tabs de escenas viven arriba del viewport).
- Los 3 modos comparten el mismo `UIViewportPanel` (cambia qué se renderiza/edita dentro).
- El botón de vista permite preview manual; el default es el tipo de la escena/tab.

---