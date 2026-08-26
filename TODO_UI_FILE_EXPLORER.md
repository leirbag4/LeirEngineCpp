# TODO UI File Explorer — ventana genérica de guardar/abrir

Ventana modal genérica (estilo explorador de archivos) que se usa en todo el programa:
guardar/abrir `.atom`, escenas, assets, etc. Se abre centrada, con barra de título y
descripción, treeview de carpetas a la izquierda y contenido a la derecha, con botones
**Cancel** / **Save** (o Open según contexto).

---

## Concepto

- **Ventana modal centrada** en el medio del editor, sobre un overlay oscuro.
- **Barra de título** configurable (título + descripción) con botón de cerrar (X).
- **Izquierda**: treeview con la estructura de carpetas del proyecto (lo que muestra
  "Contents" — ver `TODO_FILE_SYSTEM.md`).
- **Derecha**: contenedor con los archivos de la carpeta seleccionada a la izquierda:
  - Modo **lista** (simple, con iconos) — arrancar con esto.
  - Modo **grid** (iconos grandes) — a futuro.
- **Botones abajo**: `Cancel` y `Save` (o `Open` según el caso de uso).
- **Reutilizable**: se le cambia título, descripción, acción, filtro de extensión y
  "nuevo archivo" (nombre editable al guardar).

---

## Fases / Checkboxes

### Fase 1 — Widget base
- [ ] `UIFileExplorer` (editor) — modal sobre el canvas (overlay oscuro, centrado).
- [ ] Barra de título + descripción + botón X (Cerrar).
- [ ] Panel izquierdo: `UITreeView` con las carpetas del proyecto (sin archivos).
- [ ] Panel derecho: listado simple de archivos de la carpeta seleccionada (icono + nombre),
      con `UIScrollbar`/`ScrollView`.
- [ ] Botones `Cancel` / `Save` abajo (derecha), que cierran la ventana.
- [ ] Filtro de extensión configurable (p.ej. solo `.atom`, solo `.scene3d`, todo).
- [ ] Al guardar: campo de **nombre del archivo** editable (prellenado con default).

### Fase 2 — Integración con Atoms
- [ ] "Convert to Atom" abre el explorador para elegir nombre + carpeta y guardar el `.atom`
      (ver `TODO_ATOM.md`).
- [ ] "Open Atom"/instanciar: abre el explorador en modo Open para cargar un `.atom`.
- [ ] El explorador es el mismo componente para escenas (`Save Scene As`) y otros assets.

### Fase 3 — Modo grid (futuro)
- [ ] Vista grid con iconos grandes por tipo de archivo.
- [ ] Selector lista/grid en la barra de la ventana.

---

## Decisiones / Notas

- La ventana es **genérica y reutilizable** (título/descripción/acción/filtro configurables).
- El treeview de la izquierda usa el mismo `UITreeView` con iconos (carpetas).
- Se centra en el medio del editor y es modal (bloquea la interacción de atrás).
- A futuro: drag&drop de archivos, doble click para abrir/instanciar.

---