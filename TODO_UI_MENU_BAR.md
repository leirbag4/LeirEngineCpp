# TODO UI Menu Bar

Barra de menú superior estilo WPF: `UIMenuBar` con una lista de `UIMenuBarItem`s. Hoy no
existe ningún menú/popup en el codebase.

---

## Concepto

- **`UIMenuBar`**: contenedor horizontal arriba de todo el editor (sibling del Toolbar /
  del DockManager), con la lista de items.
- **`UIMenuBarItem`**: cada entrada de la barra (p.ej. "File", "Help"). Al hacer click abre
  un **dropdown** con sub-items (cada sub-item es una acción).
- Estructura tipo WPF:
  ```
  UIMenuBar
  ├── UIMenuBarItem "File"
  │     ├── item "New Scene"       (Ctrl+N)
  │     ├── item "Save Scene"      (Ctrl+S)
  │     ├── item "Save All"
  │     └── item "Exit"
  └── UIMenuBarItem "Help"
        └── item "About"
  ```
- Los sub-items del dropdown usan `UIContextMenu` (ver `TODO_UI_CONTEXT_MENU.md`).

---

## Fases / Checkboxes

### Fase 1 — Widgets base
- [ ] `UIMenuBar : UIPanel` (Row, altura ~28px, fondo oscuro).
- [ ] `UIMenuBarItem : UIPanel`/`UIButton` con `SetText`, hover highlight, y un
      `std::vector<MenuItem>` (label + `std::function<void()>` + opcional shortcut + disabled).
- [ ] Al click en un `UIMenuBarItem`, abre un dropdown (lista vertical overlay) con los
      sub-items; click en un sub-item ejecuta la acción y cierra; click fuera / ESC cierra.
- [ ] `UIMenuBar::AddItem(UIMenuBarItem*)` (o builder por API).

### Fase 2 — Integración en el editor
- [ ] Barra de menú arriba de todo (encima del Toolbar).
- [ ] **File → New Scene** (abre tab "untitled N", ver `TODO_VIEWPORT_VIEW_MODES.md`).
- [ ] **File → Save Scene** (Ctrl+S, guarda la escena activa según su tipo: `.scene2D`/
      `.scene3D`/`.uidoc` — ver `TODO_FILE_SYSTEM.md`).
- [ ] **File → Save All** (guarda todas las escenas abiertas).
- [ ] **Help → About** (dialog con versión/backend).

---

## Decisiones / Notas

- Los atajos (Ctrl+S/N) son futuros (el editor no tiene aún hotkeys globales de menú).
- El dropdown reutiliza `UIContextMenu`.
- Estilo oscuro del tema actual; hover más claro.

---