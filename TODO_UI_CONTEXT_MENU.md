# TODO UI Context Menu

Menú contextual (`UIContextMenu`) que se abre con click derecho (botón secundario) sobre un
elemento, posicionado en el cursor, overlay, y se cierra con click fuera o ESC. Hoy no existe
ningún menú/popup en el codebase.

---

## Concepto

- **`UIContextMenu`**: lista vertical de items (overlay, aparece en la posición del cursor).
- Cada item = `UIContextMenuItem { label, std::function<void()> action, bool disabled, separador? }`.
- Al click en un item → ejecuta la acción y cierra el menú.
- Click fuera del menú o ESC → cierra sin acción.
- Se usa en el **Hierarchy** (click derecho sobre items) y luego en el viewport/Contents.
- El dropdown de `UIMenuBar` reutiliza este widget (ver `TODO_UI_MENU_BAR.md`).

---

## Fases / Checkboxes

### Fase 1 — Widget base
- [x] `UIContextMenu : UIPanel` (Column, padding, fondo oscuro, overlay).
- [x] `AddItem(label, callback)` / `AddSeparator()` / `AddItemDisabled(label)`.
- [x] `OpenAt(canvasPos)` — se posiciona en el cursor y se activa; `Close()`.
- [x] Click fuera del menú o ESC → `Close()` (hooks de `EventQueue` con flag "alive").
- [x] Item hover highlight (más claro) y al click ejecuta la acción.

### Fase 1b — Integración botón "+" del Hierarchy (2026-08-27)
- [x] El botón "+" del header abre el `UIContextMenu` (agregado al canvas lazy, debajo del botón).
- [x] Items: **Object3D** (crea un Cube con mesh/material a 0,0,0 — wired por el editor),
      **Object2D** (no-op por ahora), **UIElement** (disabled — pendiente `UINode`).

### Fase 1c — Submenús (2026-08-31, `TODO_UI_MENU_BAR.md`)
- [x] `UIContextMenu::AddSubMenu(label, subMenu)` — submenú anidado con flecha PNG.
- [x] `Item.subMenu` campo; el menú es dueño de sus submenús (dtor los elimina).
- [x] `MenuItem::SetSubMenu(owner, sub)` — agrega `UIImage` flecha `arrow_right.png` 13×13
      (no hit-testable, `OwnsChild` lo incluye, `GetMinSize`/`ApplyWidthLimit` reservan
      el ancho), centrado verticalmente en `OnLayoutComputed`.
- [x] `MenuItem::OnPointerDown` con submenú → toggle del submenú (nunca acción del padre).
- [x] `MenuItem::OnPointerEnter` con submenú → abre (hover-open, estándar de la industria).
- [x] Submenú posicionado a la derecha del row, clamped al canvas; primera fila alineada
      con el row (`cr.y - GetPaddingTop()`).
- [x] Cierre: click fuera / ESC cierran el árbol completo (`CloseAllMenus`); hover a otro
      row con submenú cambia; el row plano al salir deja el submenú abierto (se cierra
      por fuera/ESC/cambio).
- [x] `m_IgnoreOutsideClick` en `OpenAt`: el mismo Press que abrió el menú no lo cierra.
- [x] Font/icon propagation: `AddSubMenu` + `OpenSubMenu` propagan `m_Font` y
      `m_SubMenuIcon` al submenú (submenú creado después de `SetFont` renderiza bien).
- [x] `Font.cpp` NO se tocó: la flecha es PNG vía `UITextureCache` (futuro: SVG).
      Los strings de diálogo usan `"..."` ASCII (no `…` U+2026, que no está en el atlas).

### Fase 2 — Integración Hierarchy
- [ ] `OnPointerDown` con botón **secundario** (derecho) sobre un item del hierarchy →
      abrir `UIContextMenu` con:
  - (objeto normal) "Convert to Atom" → `TODO_ATOM.md`.
  - (atom) "Unpack Atom", "Open Separately".
- [ ] (después) click derecho en el viewport y en "Contents" con sus propias acciones.

---

## Decisiones / Notas

- Es un widget overlay (capaz de tapar el dock); se cierra solo con click fuera/ESC.
- El hover/estado de los items usa el patrón de `UIButton`/`UILabel`.
- El menú se posiciona clampeado al canvas (no se sale de la ventana).

---