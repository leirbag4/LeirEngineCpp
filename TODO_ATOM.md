# TODO Atom — sistema de prefabs (Atoms)

Un **Atom** es un "prefab" del motor (como los prefabs de Unity): un pedazo del hierarchy
que se guarda como asset `.atom` y se puede instanciar en la escena. Se convierte desde el
context menu del hierarchy, se edita en modo **Isolate** o **Open Separately**, admite
**nested atoms** y las instancias son independientes (semántica Unity prefab).

---

## Decisiones confirmadas

- Los Atoms se guardan como **`.atom`** (assets) dentro de "Contents" (ver `TODO_FILE_SYSTEM.md`).
- Al convertir un subtree a Atom se abre el **File Explorer** para ponerle nombre y carpeta
  (ver `TODO_UI_FILE_EXPLORER.md`).
- Texto del item en el hierarchy → color `#BFCFFF`, **colapsado a propósito**, **sin flecha**
  (por más que tenga hijos).
- Doble click en un Atom → **Isolate mode**: solo el subtree del Atom se ve en el hierarchy,
  con su material/colores tal cual; el resto de la escena se ve **más oscuro (overlay)**.
- Salir del isolate: doble click en el viewport (parte overlay/vacía) o **ESC**.
- **Nested atoms** soportados (un Atom puede contener otro).
- Context menu en un Atom → "Unpack Atom" (deja de ser prefab, reaparece la flecha),
  "Open Separately" (abre un tab nuevo con el Atom + hijos, fondo vacío).
- Instancias independientes: editar los hijos de una instancia no afecta a otras (por ahora
  no hay "Apply to prefab" — se define después).

---

## Fases / Checkboxes

### Fase 1 — Flag y visual
- [ ] `AtomComponent` (o flag `IsAtom()`) en `CoreObject`.
- [ ] Al convertir: texto `#BFCFFF`, colapsado, **sin flecha** (el tree no muestra la flecha
      aunque tenga hijos — flag por item o override de `RebuildLabels`).
- [ ] Deshacer: "Unpack Atom" quita el flag → texto normal + flecha reaparece.

### Fase 2 — Convertir / guardar (.atom)
- [ ] Context menu "Convert to Atom" → abre `UIFileExplorer` (Save, filtro `.atom`, nombre
      editable) → serializa el subtree a `.atom` (JSON, ver `TODO_FILE_SYSTEM.md`).
- [ ] El subtree original se marca como instancia del `.atom` (guardar `atomAssetPath` + guid).
- [ ] Cargar/instanciar un `.atom`: crear el subtree desde el asset (profundidad ilimitada,
      componentes incluidos).

### Fase 3 — Isolate mode
- [ ] Doble click en un Atom → aislar: el hierarchy muestra solo el subtree del Atom;
      overlay oscuro sobre el resto del editor (capa de oscurecimiento sobre el viewport/hierarchy).
- [ ] ESC o doble click en vacío → volver al hierarchy normal.
- [ ] En isolate, el material/colores de los objetos se ven tal cual (el oscurecimiento es solo
      del fondo/entorno).

### Fase 4 — Open Separately
- [ ] Context menu "Open Separately" → nuevo tab de escena (fondo vacío) mostrando solo el
      Atom + hijos (nombre del tab = nombre del Atom).
- [ ] Mismo comportamiento que un tab normal (se cierra con la cruz).

### Fase 5 — Nested atoms
- [ ] Un Atom dentro de otro: el item hijo con flag Atom también colapsado/sin flecha en el
      padre.
- [ ] Al aislar el padre, los nested atoms se muestran como items colapsados (doble click
      aísla al nested).

### Fase 6 — Instancias independientes
- [ ] Al editar los hijos de una instancia, no afecta a otras instancias (deep-copy por
      instancia al instanciar).
- [ ] (Futuro) "Apply to Atom" / "Revert" estilo Unity — se define después.

---

## Decisiones / Notas

- El color `#BFCFFF` es solo el texto del item (distinto del icono de familia).
- Los hijos del Atom **siguen existiendo en la escena** (no se borran), solo se ocultan en el
  tree normal.
- El serializado `.atom` es JSON (mismo árbol que una escena pero sin la raíz de la escena).
- El isolate usa un overlay oscuro (capa translucent sobre el viewport + hierarchy).

---