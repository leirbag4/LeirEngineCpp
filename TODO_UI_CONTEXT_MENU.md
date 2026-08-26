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
- [ ] `UIContextMenu : UIPanel` (Column, padding, fondo oscuro, overlay).
- [ ] `AddItem(label, callback)` / `AddSeparator()` / `AddItemDisabled(label)`.
- [ ] `OpenAt(canvasPos)` — se posiciona en el cursor y se activa; `Close()`.
- [ ] Click fuera del menú o ESC → `Close()`.
- [ ] Item hover highlight (más claro) y al click ejecuta la acción.

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