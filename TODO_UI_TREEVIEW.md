# UITreeView — TODO

Estado: plan aprobado (2026-08-22). Implementar `UITreeView` + `UITreeViewItem` virtualizados, con scrollbars, selección, expand/collapse, drag&drop, edición inline y eventos.

> Referencia de patrones verificados en el codebase:
> - `Enabled` → `SetItemEnabled`/`IsItemEnabled` (sigue `SetFooEnabled`/`IsFooEnabled` de `ScrollView.h:42`/`UITextArea.h:56`; no existe `SetEnabled` genérico).
> - `Selected` → `SetSelected`/`IsSelected` (distinto de `UIElement::IsActive` visibilidad y de `DockPane::GetActiveIndex`).
> - Colores → `UIElement::SetColor`/`GetColor` (background), `UITextInput::SetTextColor`, `UIButton::SetColors(normal,hover,pressed)` — hover/selección leídos por `UIRenderer`.
> - Eventos → `SetOnX(std::function)` (`UIButton::SetOnClick`, `UIScrollbar::SetOnScroll`, `UIFloatInput::SetOnValueChanged`, `DockManager::SetOnLayoutChanged`).
> - Scrollbars → `UITextArea.cpp:13` / `ScrollView.cpp:9` (`SetClip(true)`, `OwnsChild`, `GetContentSize`/`GetViewportSize`/`SyncScrollbars` con `TopLeft` absoluto + `std::round`).
> - Drag → `UICanvas::CapturePointer` (`UIScrollbar.cpp:163`, `UIDragFloatInput.cpp:76`, `DockManager.cpp:402`).

## Concepto

- `UITreeView : public UIElement` con scroll vertical + horizontal (como `UITextArea`/`ScrollView`), `SetClip(true)`, virtualizado para miles de items.
- `UITreeViewItem : public UIPanel` (fila `Row`: flecha + `UILabel`). Árbol lógico `m_TreeParent` / `m_TreeChildren`; el `UITreeView` aplana solo las ramas expandidas a `m_FlatVisible`.
- Row height fijo **20px**, indent por nivel **16px** (configurable). Flecha `▶`/`▼` solo si tiene hijos.
- Selección full-width: background desde `cr.x` hasta `cr.x+viewport.w`, hover y selección via `SetColor`/`GetColor` leídos por `UIRenderer`.
- Drag&drop para anidar (Onto) y reordenar (Below con línea), ghost translúcido + highlight.
- Edición inline con `UITextInput` overlay centrado al presionar **F2** (cuando `IsEditable()==true`).

## Arquitectura

```
UITreeView (UIElement, Clip)
├── m_VScrollbar : UIScrollbar(true)   \"TreeViewVScrollbar\"  (own)
├── m_HScrollbar : UIScrollbar(false)  \"TreeViewHScrollbar\" (own)
├── m_EditInput  : UITextInput* | nullptr (overlay edición, IsOverlayLayer)
└── items lógicos:
    m_Roots: vector<UITreeViewItem*>          // roots del árbol
    m_FlatVisible: vector<UITreeViewItem*>    // DFS solo expandidos (cache, dirty flag)
    m_SelectedItems: vector<UITreeViewItem*>
    m_HoveredItem, m_DropTarget, m_DragItem (+ DropMode {None,Onto,Below})
    m_ScrollOffset, m_Indent=16, m_MultipleSelection=false, m_Editable=false
    m_CachedMaxWidth (invalida en SetText/SetFont/SetExpanded)

UITreeViewItem (UIPanel, Row)
├── m_Label: UILabel* (own)
├── m_TreeParent, m_TreeChildren
├── m_ItemEnabled=true, m_Selected=false, m_Expanded=true
├── m_Indent, m_CachedTextWidth
└── colores: m_SelectionColor {0.30,0.50,1.0,0.40}, m_HoverColor {0.20,0.20,0.20,1},
             m_TextColor {1,1,1,1}, m_TextHover, m_TextSelection, m_ArrowColor
```

## API

### UITreeViewItem (`engine/include/LeirEngine/UI/UITreeViewItem.h`)

- `void SetText(const std::string& t); std::string GetText() const;`
- `void SetItemEnabled(bool e); bool IsItemEnabled() const;` // false = grisado {0.5,0.5,0.5,1}, no seleccionable, `OnPointerDown` → false
- `void SetSelected(bool s); bool IsSelected() const;`
- `void SetExpanded(bool e); bool IsExpanded() const;`
- `int GetDepth() const;` // nivel indent
- `UITreeViewItem* GetTreeParent() const; const std::vector<UITreeViewItem*>& GetTreeChildren() const;`
- `void AddTreeChild(UITreeViewItem* child); void RemoveTreeChild(UITreeViewItem*); void InsertTreeChildAt(UITreeViewItem*, int index);`
- `void SetIndent(float px); float GetIndent() const;`
- `void SetSelectionColor(Vector4 c); Vector4 GetSelectionColor() const;`
- `void SetHoverColor(Vector4 c); Vector4 GetHoverColor() const;`
- `void SetTextColor(Vector4 c); Vector4 GetTextColor() const;`
- `void SetTextHoverColor(Vector4 c); Vector4 GetTextHoverColor() const;`
- `void SetTextSelectionColor(Vector4 c); Vector4 GetTextSelectionColor() const;`
- `void SetArrowColor(Vector4 c); Vector4 GetArrowColor() const;`
- `void SetFont(Font* f); Font* GetFont() const;`
- `bool OwnsChild(const UIElement*) const override;` // m_Label
- `Vector2 GetMinSize() const override;` // {60,20}

### UITreeView (`engine/include/LeirEngine/UI/UITreeView.h`)

- `UITreeView(); ~UITreeView() override;`
- `void SetMultipleSelectionEnabled(bool e); bool IsMultipleSelectionEnabled() const;` // default false
- `void SetEditable(bool e); bool IsEditable() const;` // default false, sigue `UITextInput::SetEditable`
- `int GetSelectedIndex() const; void SetSelectedIndex(int idx);` // índice en flat visible, -1 = none
- `UITreeViewItem* GetSelectedItem() const; void SetSelectedItem(UITreeViewItem* item);`
- `std::vector<UITreeViewItem*> GetSelectedItems() const; void SetSelectedItems(const std::vector<UITreeViewItem*>& items);`
- `int GetItemCount() const;` // flat visible count
- `UITreeViewItem* GetItemAt(int visibleIndex) const;`
- `void AddItem(UITreeViewItem* item, UITreeViewItem* parent = nullptr);`
- `void RemoveItem(UITreeViewItem* item);`
- `void ClearItems();`
- `void SetIndent(float px); float GetIndent() const;` // propaga a items, invalida layout
- `void SetSelectionColor(Vector4 c); Vector4 GetSelectionColor() const;`
- `void SetHoverColor(Vector4 c); Vector4 GetHoverColor() const;`
- `void SetTextColor(Vector4 c); Vector4 GetTextColor() const;`
- `void SetTextHoverColor(Vector4 c); Vector4 GetTextHoverColor() const;`
- `void SetTextSelectionColor(Vector4 c); Vector4 GetTextSelectionColor() const;`
- `void SetArrowColor(Vector4 c); Vector4 GetArrowColor() const;`
- `void SetVerticalScrollbarEnabled(bool e); bool IsVerticalScrollbarEnabled() const;`
- `void SetHorizontalScrollbarEnabled(bool e); bool IsHorizontalScrollbarEnabled() const;`
- `void SetFont(Font* f); Font* GetFont() const;`
- `Vector2 GetContentSize() const override; Vector2 GetViewportSize() const;`
- `void SetScrollOffset(Vector2 o); Vector2 GetScrollOffset() const;`
- `bool OwnsChild(const UIElement*) const override;` // m_VScrollbar, m_HScrollbar, m_EditInput (si existe)
- Eventos (patrón `SetOnX`):
  - `void SetOnSelectedIndexChanged(std::function<void(int)> cb);`
  - `void SetOnSelectedItemChanged(std::function<void(UITreeViewItem*)> cb);`
  - `void SetOnSelectedItemsChanged(std::function<void(const std::vector<UITreeViewItem*>&)> cb);`
  - `void SetOnItemExpanded(std::function<void(UITreeViewItem*)> cb);`
  - `void SetOnItemCollapsed(std::function<void(UITreeViewItem*)> cb);`
  - `void SetOnItemDragged(std::function<void(UITreeViewItem* draggedItem, UITreeViewItem* newParent, int newIndex)> cb);`
  - `void SetOnItemDoubleClicked(std::function<void(UITreeViewItem*)> cb);`
  - `void SetOnItemRenamed(std::function<void(UITreeViewItem*, const std::string& oldText, const std::string& newText)> cb);`
- Input: `bool OnPointerDown(const Vector2& pos) override; void OnPointerMove(const Vector2&) override; bool OnPointerUp(const Vector2&) override; bool OnScroll(float delta) override; bool OnKeyDown(int key) override;` // F2
- `void OnLayoutComputed() override;`

## Detalles de implementación

### Hit-test / Hover full-width (punto 5 del usuario)
- Cada fila ocupa **ancho completo** del viewport (`cr.x` → `cr.x+viewport.w`), no solo el texto. `UITreeView::OnPointerDown` hace hit contra `rowRect = {cr.x, cr.y + idx*rowH - scrollY, cr.x+viewport.w, rowH}`. Aunque el mouse esté en el espacio vacío a la derecha del texto o sobre el indent de un child desplazado, el hover/selección se aplica. `IsItemEnabled()==false` → grisado y `return false` (no selecciona).

### Double-click
- Detección como `UITextInput.cpp:62` (dos `OnPointerDown` en ≤15 frames y `|posDiff|≤3`), pero a nivel de fila. Al detectar doble-click en fila con `IsItemEnabled()==true`, dispara `SetOnItemDoubleClicked(item)` y no inicia drag.

### Virtualización (miles de items)
- `m_FlatVisible` se reconstruye solo si dirty (expand/collapse, Add/Remove, SetText). `GetContentSize()` usa `flatVisible.size()*rowH` y `m_CachedMaxWidth` (máximo de `depth*indent + 12 + textWidth`).
- `OnLayoutComputed()`: `first = clamp(floor(scrollY/rowH),0,n-1)`, `last = clamp(ceil((scrollY+viewportH)/rowH)-1, first, n-1)`. Solo esos índices `SetActive(true)` + `ComputeLayout`; resto `SetActive(false)`. `UIRenderer::RenderElement` y `UICanvas::HitTestRecursive` ya cullinguean `!IsActive()`.
- Horizontal: `textX = cr.x + depth*indent + 12 + 4 - scrollX`; si `textX+textW < cr.x` o `textX > cr.x+viewportW` no genera quad de texto (pero la fila background full-width sí, clippeada por scissor).

### Expand / Flecha
- Flecha `UILabel` con `▶` (colapsado) / `▼` (expandido), hit `12×12` a `x = cr.x + depth*indent`. Click en flecha → `SetExpanded(!IsExpanded())` + eventos, sin seleccionar si ya seleccionado; click fuera de flecha pero en fila → selección.

### Selección
- Single: click → `ClearSelection` + `SetSelected(true)` + disparar `SelectedIndex/Item/ItemsChanged`.
- Multiple (`IsMultipleSelectionEnabled()==true`): `Ctrl` toggle, `Shift` rango por `visibleIndex` entre `m_LastSelectedIndex` y `clickedIndex`. `SetSelectedItems` valida `IsItemEnabled`.
- Drag de selección múltiple mueve a **todos juntos** (vector `m_SelectedItems` capturado en `m_DragStart`).

### Drag & Drop
- `OnPointerDown` en fila `IsItemEnabled` → `m_DragItem` + `CapturePointer(this)` (patrón `UIScrollbar.cpp:163`), deadzone 4px (`DockManager.cpp:425`).
- `OnPointerMove` con `m_Dragging==true`: ghost `UILabel` overlay (`IsOverlayLayer=true`, alpha 0.6) en `pos` del mouse con texto del `m_DragItem` (si múltiple, `N items`). Hit-test determina `m_DropTarget` + `DropMode`:
  - Mitad superior de la fila → `Onto` (anidar como hijo, highlight target `hoverColor*1.2`).
  - Franja inferior `4px` → `Below` (insertar después, línea horizontal `2px` `selectionColor`).
- `OnPointerUp`: si `m_Dragging`, reparent `RemoveTreeChild` + `AddTreeChild(newParent, newIndex)` para cada dragged item (preservando orden), dispara `SetOnItemDragged` por cada uno, limpia ghost/highlight/línea, `ReleasePointer` (auto).

### Edición inline (F2, `IsEditable`)
- Solo si `IsEditable()==true` y `GetSelectedItem()!=nullptr && IsItemEnabled()`.
- `OnKeyDown` con `key == 291` (GLFW F2) → crea `m_EditInput = new UITextInput()` hijo overlay, `SetText(item->GetText())`, `SetRect` = `rowRect` (mismo `x,y,w,h`, centrado vertical), `SetActive(true)`, `UICanvas::SetFocus(m_EditInput)`, `SelectAll`.
- Commit: `Enter`/`OnBlur` → si texto cambió y `IsItemEnabled`, `item->SetText(new)` + `SetOnItemRenamed(old,new)`, destruye input, foco vuelve al tree. Cancel: `Escape` → destruye sin callback. Click fuera del input → blur→commit (como `UIFloatInput.cpp:76`).

### UIRenderer
- Nuevo dispatch para `UITreeViewItem` en `UIRenderer.cpp:393` (como `UIButton`/`UILabel`): background full-width `cr.x → cr.x+viewport.w` con `selectionColor`/`hoverColor`/`normal`, texto vía `Font::LayoutText`, flecha. Selección y hover via `GetColor`/`GetTextColor` leídos por estado.

## Fases / Checkboxes

### Fase 1 — Infra + Item básico
- [x] `engine/include/LeirEngine/UI/UITreeViewItem.h` + `engine/src/UI/UITreeViewItem.cpp` (texto, `SetItemEnabled`/`IsItemEnabled`, `SetSelected`/`IsSelected`, `SetExpanded`/`IsExpanded`, depth, indent, colores, `GetMinSize`)
- [x] `engine/include/LeirEngine/UI/UITreeView.h` + `engine/src/UI/UITreeView.cpp` esqueleto (roots, flat cache, `AddItem`/`RemoveItem`/`ClearItems`, `OwnsChild`)
- [x] Registrar en `engine/CMakeLists.txt`
- [x] Build verifica headers (LeirEngine + LeirEngineEditor compilan)

### Fase 2 — Scroll + virtualización + layout
- [x] `SetClip(true)` en `UITreeView`, `m_VScrollbar`/`m_HScrollbar` como hijos (patrón `UITextArea.cpp:13`), `GetContentSize`/`GetViewportSize`/`GetMaxScroll`, `OnLayoutComputed` con `AnchorSet::TopLeft` absoluto + `std::round`, `SyncScrollbars`, `OnScroll` (wheel vertical, Shift+wheel horizontal)
- [x] Virtualización: `m_FlatVisible` cache + `m_CachedMaxWidth`, rango `first/last` visible, `SetActive(true/false)` por fila, cull horizontal
- [x] `UIRenderer` via `UIPanel` (background) + `UILabel` (texto/flecha) — full-width background `cr.x → cr.x+viewport.w`, scissor por `SetClip`
- [ ] Verificación: 10k items sin drop de FPS, `UIRenderer` no overflowa (pendiente visual del usuario con el panel TreeView)

### Fase 3 — Selección + hover full-width + eventos
- [x] Full-width hit-test por fila (aunque vacío a la derecha del texto o indent), `IsItemEnabled` grisado no seleccionable
- [x] Selección single + `IsMultipleSelectionEnabled` (Ctrl toggle, Shift rango), `GetSelectedIndex`/`Item`/`Items` + setters
- [x] Eventos `SetOnSelectedIndexChanged`/`SetOnSelectedItemChanged`/`SetOnSelectedItemsChanged`
- [x] Hover full-width (`SetHoverColor`/`SetTextHoverColor`) — `IsHovered` + `UpdateColors`

### Fase 4 — Expand / Flecha + double-click
- [x] Flecha `▶`/`▼` solo si `GetTreeChildren().size()>0`, hit `12×12`, toggle `SetExpanded` + `SetOnItemExpanded`/`SetOnItemCollapsed`
- [x] Double-click en fila → `SetOnItemDoubleClicked` (detectar ≤15 frames, ≤3px como `UITextInput.cpp:62`)

### Fase 5 — Drag & Drop (anidar + reorder)
- [x] `CapturePointer` en `OnPointerDown`, deadzone 4px, ghost translúcido (`IsOverlayLayer`, alpha 0.6) con texto del dragged (o `N items` si múltiple)
- [x] `DropMode {Onto, Below}`: mitad superior → highlight `Onto`, franja inferior 4px → línea `Below`
- [x] Reparent/reorder para selección múltiple (mover todos juntos, preservando orden), `SetOnItemDragged`
- [x] Limpieza ghost/highlight/línea en `OnPointerUp`

### Fase 6 — Edición inline F2
- [x] `SetEditable`/`IsEditable` en `UITreeView` (default false)
- [x] `OnKeyDown` F2 (291) → `UITextInput` overlay centrado sobre la fila, focus + `SelectAll`
- [x] Commit `Enter`/`Blur` → `SetOnItemRenamed`, Cancel `Escape`, validación `IsItemEnabled`
- [x] Click fuera → commit (vía `OnBlur`)

### Fase 7 — Integración + verificación
- [x] Demo panel `TreeViewDebugPanel` en `editor/src/UI` con roots/children, enabled false, múltiples selecciones, Big List 2000
- [x] Build MSVC limpio (LeirEngine + LeirEngineEditor), editor launch sin VUIDs/stderr
- [ ] ctest 2/2 (pendiente CI), test con miles de items — verificado local 2000 sin overflow

## Decisiones / Notas

- `SetItemEnabled` en vez de `SetEnabled` para no colisionar con `UIElement::IsActive` (visibilidad). `SetSelected` distinto de `IsActive` de dock.
- `RowH=20`, `Indent=16` confirmados por el usuario.
- Drag múltiple mueve todos los seleccionados juntos (confirmado).
- Double-click dispara evento attachable (confirmado).
- Hit-test full-width confirmado: click en espacio vacío del indent o a la derecha del texto selecciona la fila.

