# TODO UI Menu Bar — UIMenuBar + UIMenuBarItem + submenús

Barra de menú superior estilo WPF: `UIMenuBar` (contenedor horizontal) con una lista de
`UIMenuBarItem`s ("File", "Edit", "Help"…). Cada item abre un **dropdown** vertical con
sub-items; los sub-items pueden anidar **submenús** laterales con flechita. El dropdown y
los submenús reutilizan `UIContextMenu` (ya implementado, `TODO_UI_CONTEXT_MENU.md`).

---

## Concepto / estructura

```
UIMenuBar  (Row, ~28px, fondo oscuro, sibling del Toolbar, arriba de todo)
├── UIMenuBarItem "File"
│     └── UIContextMenu (dropdown, overlay, debajo del item)
│           ├── item "New Scene"   (Ctrl+N)
│           ├── item "Save Scene"  (Ctrl+S)
│           ├── separator
│           ├── item "Import"
│           │     └── UIContextMenu (submenú → a la derecha, flecha "›")
│           │           ├── item "Import Model…"
│           │           └── item "Import Texture…"
│           ├── separator
│           └── item "Exit"
└── UIMenuBarItem "Help"
      └── UIContextMenu
            └── item "About LeirEngine"
```

El dropdown de un `UIMenuBarItem` y cada submenú son `UIContextMenu` (overlay, hijos
del canvas). Se abren por click/hover, se cierran con click fuera / ESC / click en un item.

---

## Nomenclatura estándar (consistente con el resto de widgets)

| Patrón | Ejemplos existentes | Se usa en |
|---|---|---|
| `SetX(T)` / `GetX()` | `SetText`/`GetText`, `SetFont`/`GetFont`, `SetColor`/`GetColor`, `SetMinSize`/`GetMinSize` | Atributos |
| `SetX(bool)` / `IsX()` | `SetActive`/`IsActive`, `SetClip`/`IsClipEnabled`, `SetHitTestable`/`IsHitTestable`, `SetHovered`/`IsHovered` | Banderas |
| `SetOnEvent(cb)` | `UIButton::SetOnClick`, `ToolbarPanel::SetOnToolChanged` | Callbacks |
| `enum class` PascalCase | `LayoutMode`, `ButtonState`, `ButtonTextAlign`, `SizePolicy` | Estados |
| Eventos `bool` (true = consumido) | `OnPointerDown/Up`, `OnKeyDown`, `OnScroll`, `OnTextInput` | Input |
| `OwnsChild(const UIElement*)` | `UIContextMenu`, `MenuItem`, `ScrollView`, `UITextArea`, `UIScrollbar` | Teardown (double-free rule) |
| `GetMinSize() override` | todos los widgets | Layout |
| `OnLayoutComputed() override` | `ToolbarPanel`, `ScrollView`, `UITextArea` | Post-layout |
| `SetFont(Font*)` / `GetFont()` | `UIButton`, `UILabel`, `UIContextMenu::MenuItem`, `ToolbarPanel` | Tipografía |
| `SetText(const std::string&)` / `GetText()` | `UIButton`, `UILabel`, `UITextInput` | Texto |
| `SetPadding/SetSpacing` | `UIElement` base | Layout |
| `SetOverlayLayer(bool)` / `IsOverlayLayer()` | `UIContextMenu`, `DockDropOverlay`, `UIDebugOverlay` | Capa superior |
| Colores por estado | `SetColors(normal, hover, pressed)` (UIButton) | Estados hover/open |
| `SetTextColor(const Vector4&)` / `GetTextColor()` | `UIButton` | Color de texto |
| Idioma | **inglés** — PascalCase clases/métodos, camelCase params | todo |

---

## Flechita de submenú — DECISIÓN: PNG 13×13 via `UITextureCache` (futuro: SVG)

- **Usa la industria**: VS Code (codicon), Unity, Godot, WPF, Qt — glifos vectoriales en
  tipografía para micro-íconos de menú. Nosotros usamos **PNG 13×13** por ahora (mismo
  sistema que los iconos del Hierarchy en `UITextureCache`).
- **Futuro:** renderizador SVG propio para todos los iconos del editor, que sustituirá
  los PNGs y el sistema de texturas cache.
- **HiDPI:** el PNG se sube tal cual a la GPU (Linear sampler) → el sampler escala a
  cualquier DPI sin variantes 1x/2x/3x. En el cambio de DPI se recarga con la nueva
  `contentScale` (cache key distinta) si se activa `LEIR_ICON_CPU_UPSCALE`.
- **Cero assets en fuente:** no depende de glifos extra en el atlas (la fuente solo
  empaqueta ASCII 32-126).
- **Elipsis de diálogo** (`"Open Scene..."`): en la industria el `...` es parte del
  string (tres caracteres ASCII o un U+2026). Para portabilidad con fuentes sin
  U+2026, usamos tres puntos ASCII `"..."`.

Implementación: `UIImage*` (hijo del `MenuItem`) con `SetMinSize({13,13})` centrado
verticalmente en `OnLayoutComputed`. El `UIContextMenu` propaga el icono a todas las
filas con submenú via `SetSubMenuIcon(shared_ptr<Texture2D>)`, y recursivamente a
submenús anidados.

---

## API propuesta

### `UIMenuBar : UIPanel`

```cpp
class LEIR_API UIMenuBar : public UIPanel {
public:
    UIMenuBar();
    ~UIMenuBar() override;

    void SetFont(Font* font);                 // propaga a los items
    Font* GetFont() const;

    UIMenuBarItem* AddItem(const std::string& label);   // builder → crea, añade, devuelve
    UIMenuBarItem* AddItem(UIMenuBarItem* item);        // adopta uno existente
    void RemoveItem(UIMenuBarItem* item);               // remueve (no elimina)
    UIMenuBarItem* GetItem(int index) const;
    const std::vector<UIMenuBarItem*>& GetItems() const;
    int GetItemCount() const;

    void CloseMenus();                                 // cierra todos los dropdowns
    UIMenuBarItem* GetOpenItem() const;

    void SetOnItemOpened(std::function<void(UIMenuBarItem*)> cb);

    Vector2 GetMinSize() const override;
    bool OwnsChild(const UIElement* child) const override;   // los items
protected:
    void OnLayoutComputed() override;                      // Row de items
    bool OnPointerDown(const Vector2& pos) override;       // click fuera → cierra
};
```

### `UIMenuBarItem : UIPanel`

```cpp
class LEIR_API UIMenuBarItem : public UIPanel {
public:
    UIMenuBarItem(const std::string& label);
    ~UIMenuBarItem() override;

    void SetText(const std::string& text);
    const std::string& GetText() const;

    void SetFont(Font* font);
    Font* GetFont() const;

    void SetColors(const Vector4& normal, const Vector4& hover);
    void SetTextColor(const Vector4& color);
    const Vector4& GetTextColor() const;
    const Vector4& GetBgNormal() const;
    const Vector4& GetBgHover() const;

    UIContextMenu* GetMenu() const;                 // dropdown propio (owned)

    // Builder que delega al UIContextMenu interno
    void AddMenuItem(const std::string& label, std::function<void()> action);
    void AddMenuSeparator();
    void AddMenuDisabled(const std::string& label);
    void AddSubMenu(const std::string& label, UIContextMenu* subMenu);

    void OpenMenu();
    void CloseMenu();
    bool IsMenuOpen() const;

    void SetOnToggle(std::function<void(bool open)> cb);

    bool OnPointerDown(const Vector2& pos) override;    // toggle
    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;

    Vector2 GetMinSize() const override;
    bool OwnsChild(const UIElement* child) const override;
protected:
    void OnLayoutComputed() override;
};
```

---

## Extensión de `UIContextMenu` (submenús)

- `Item` gana `UIContextMenu* subMenu = nullptr`.
- `void AddSubMenu(const std::string& label, UIContextMenu* subMenu)` — **propaga
  `m_Font` y `m_SubMenuIcon`** al submenú (bug: antes el submenú se creaba después de
  `SetFont` y quedaba con font null → filas invisibles).
- `MenuItem` gana: `SetSubMenu(UIContextMenu*)`, `UIImage*` flecha PNG 13×13 (no
  hit-testable, `OwnsChild` lo incluye), `GetMinSize` suma la flecha, `OnLayoutComputed`
  centra la flecha verticalmente (Row layout es top-aligned).
- `UIContextMenu::SetSubMenuIcon(icon)` — propaga a todas las filas con submenú y
  recursivamente a submenús anidados.
- `MenuItem::OnPointerDown`: si tiene subMenu → abre submenú (no cierra el padre);
  si no → ejecuta acción y cierra todo el árbol (`CloseAllMenus`).
- `MenuItem::OnPointerEnter`: si tiene subMenu → abre submenú (hover-open).
- `MenuItem::OnPointerExit`: no cierra (el submenú se cierra por click fuera, ESC,
  o hover a otro row con submenú que cambia).
- El submenú se posiciona a la derecha del item, con la primera fila alineada
  verticalmente (compensa el top padding: `pos.y = cr.y - sub->GetPaddingTop()`).
- `OpenAt` setea `m_IgnoreOutsideClick = true` para que el mismo Press que abrió el
  menú no lo cierre (el hook global de "click fuera cierra" lo veía como "fuera").
- `CloseAllMenus()`: cierra este menú y todos los ancestros (usado cuando un item
  plano ejecuta una acción).
- Ownership: `UIContextMenu` es dueño de sus submenús (los elimina en el dtor;
  `OwnsChild` true para ellos) — respeta la double-free rule.

---

## Eventos y flujo

| Acción | Comportamiento |
|---|---|
| Click en `UIMenuBarItem` | Cerrado → abre dropdown debajo del item, cierra cualquier otro abierto en el bar. Abierto → cierra. |
| Click en otro item del mismo bar | Cierra el anterior, abre el nuevo (uno abierto a la vez). |
| Click fuera del bar + dropdown | Cierra (hook de EventQueue del `UIContextMenu`). |
| ESC | Cierra (hook existente del `UIContextMenu`). |
| Hover sobre submenu item | Abre submenú a la derecha. |
| Hover sale del submenu item | El submenú queda abierto (permite entrar a él); se cierra por click fuera, ESC, o hover a otro row con submenú (cambia). |
| Click en submenu item sin submenu | Ejecuta acción, cierra todo el árbol. |
| Click en submenu item con submenu | Abre/reposiciona submenú (no ejecuta acción). |

---

## Integración editor (main.cpp)

```
kMenuBarHeight = 28.0f   (nuevo)
kTopToolbarHeight = 30.0f
kBottomBarHeight  = 30.0f

Layout:
  UIMenuBar  → anchor {0,0,1,0}  offset {0,0,0,kMenuBarHeight}
  Toolbar    → anchor {0,0,1,0}  offset {0,kMenuBarHeight,0,kMenuBarHeight+kTopToolbarHeight}
  DockManager→ anchor Stretch    offset {0,kMenuBarHeight+kTopToolbarHeight,0,-kBottomBarHeight}
```

- En `OnInit`: crear `UIMenuBar`, `SetFont(m_FontSmall)`, `SetName("MenuBar")`,
  `m_Canvas->AddChild(m_MenuBar)` ANTES del Toolbar.
- Items: **File** → New Scene / Save Scene / Save All / Exit; **Help** → About.
  Por ahora los handlers loguean (los targets reales son `TODO_FILE_SYSTEM.md` /
  `TODO_VIEWPORT_VIEW_MODES.md`). El orden de layout se ajusta con las constantes.
- `OnShutdown`: borrar con `DeleteUiSubtree(m_MenuBar)` (respeta `OwnsChild`).

---

## Fases / Checkboxes

### Fase 1 — Widgets base (engine)
- [x] `UIMenuBar : UIPanel` (Row, ~28px, fondo oscuro, sibling del Toolbar).
- [x] `UIMenuBarItem : UIPanel` con `SetText`, hover highlight y toggle de dropdown.
- [x] `UIMenuBarItem::AddMenuItem/AddMenuSeparator/AddMenuDisabled/AddSubMenu` (builder).
- [x] `UIMenuBar::AddItem(label)` builder + `AddItem(item)` + `RemoveItem/GetItem(s)`.
- [x] Eventos: toggle on click, uno abierto a la vez, click fuera / ESC cierra.
- [x] Flechita de submenú **PNG 13×13 vía `UITextureCache`** (mismo sistema que los
      iconos del Hierarchy; futuro SVG). `UIImage` centrado en `OnLayoutComputed`.
- [x] `UIContextMenu` submenu support: `AddSubMenu`, hover open, click toggle, `CloseAllMenus`.
- [x] Font propagation: `AddSubMenu` + `OpenSubMenu` propagan `m_Font` y `m_SubMenuIcon`
      al submenú (bug: submenú creado después de `SetFont` quedaba invisible).
- [x] Outside-click ignore: `m_IgnoreOutsideClick` en `OpenAt` para que el mismo Press
      que abrió el menú no lo cierre.
- [x] Alineación vertical: submenú se posiciona con `cr.y - sub->GetPaddingTop()` para
      que la primera fila quede a la misma altura del item.
- [x] Ownership + `OwnsChild` correctos (items del bar, submenús del menú).
- [x] Doxygen docblocks completos (regla AGENTS.md).
- [x] Build LeirEngine + editor limpios.

### Fase 2 — Integración editor
- [x] Barra de menú arriba de todo (encima del Toolbar), layout con constantes (`kTopMenuBarHeight`=28).
- [x] **File → New Scene / Open / Save / Save All / Exit** (wired: Exit → `Quit()`; resto loguea — pendiente `TODO_FILE_SYSTEM.md`).
- [x] **Edit → Transform Tool** (submenú con flecha `arrow_right.png` → cambia el gizmo/toolbar).
- [x] **Help → About** (loguea backend via `GetBackendName`).
- [x] Items de diálogo usan `"..."` ASCII (no `…` U+2026) para portabilidad con fuentes sin extra glifos.
- [ ] **File → New Scene** real (abre tab "untitled N").
- [ ] **File → Save Scene** real (Ctrl+S, guarda escena activa según tipo).
- [ ] **File → Save All** real (guarda todas las escenas abiertas).
- [ ] **Help → About** como dialog (versión/backend).
- [ ] Verificación visual del usuario (submenús, flechas, hover, HiDPI).

---

## Decisiones / Notas

- Los atajos (Ctrl+S/N) son futuros (no hay hotkeys globales de menú aún).
- Dropdown + submenús reutilizan `UIContextMenu` (overlay, clamped al canvas).
- Flecha de submenú: **PNG 13×13 vía `UITextureCache`** (mismo sistema que los iconos del Hierarchy). Cargado desde `assets/icons/arrow_right.png`.
- **Futuro:** renderizador SVG propio para todos los iconos del editor (sustituirá los PNGs y el sistema de texturas cache). Se diseñará como un módulo `SVGRenderer` que renderiza paths vectoriales directamente en el batch de UI, con soporte HiDPI nativo (sin assets bitmap por escala).
- Estilo oscuro del tema actual; hover más claro; item abierto = estado "pressed".
- El submenú se abre a la derecha (industry standard); flip a izquierda si no cabe (futuro).
