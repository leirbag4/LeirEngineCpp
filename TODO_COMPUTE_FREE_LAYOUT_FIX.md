# TODO ComputeFreeLayout `+=` accumulation fix

Bug del core de UI resuelto el **2026-08-27**. Este documento queda como registro del
bug, la causa raíz, el fix aplicado y la verificación / deuda restante.

---

## El bug (síntoma)

Elementos de UI que viven en un contenedor con layout **Free** y que se posicionan con
un **anchor/offset fijo** (seteado una vez, no re-asignado cada frame) se van **volando
hacia abajo/derecha** frame a frame apenas se hacen visibles.

Ejemplos reales:
- El `HierarchyPanel` (Fase 0.2): el `UITreeView` con anchor `Stretch` se escapaba hacia
  abajo en cuanto el panel se mostraba.
- Históricos ya workaroundeados: `DockManager` (el dock bajo la toolbar en `y=30` se
  deslizaba 30px/frame) y `DockDropOverlay` (el ghost del drag se desplazaba hacia abajo).
- El bug viejo del `UITreeView` ("flying / conglomerate of text" al inicio / colapsar)
  tenía la misma causa de fondo (offsets que acumulaban) aunque se parcheó de otra forma.

## Causa raíz (el core)

`UIElement::ComputeFreeLayout` propagaba la posición absoluta del padre **mutando el
offset del hijo**:

```cpp
// ANTES (roto)
child->m_Rect.offset.left  += m_ComputedRect.x;
child->m_Rect.offset.top   += m_ComputedRect.y;
child->m_Rect.offset.right += m_ComputedRect.x;
child->m_Rect.offset.bottom+= m_ComputedRect.y;
```

`m_Rect.offset` está pensado como un valor **relativo al anchor** (la configuración).
El `+=` lo contaminaba permanentemente con la posición del padre **cada frame**:
- Los hijos que re-asignan su offset absoluto cada frame (Row/Column, el viewport/items
  del TreeView, ScrollView) se "auto-corrigen" (el doble conteo transitorio se pisa).
- Los hijos que NO re-asignan (anchor fijo, Stretch) acumulan indefinidamente → vuelan.

Row/Column no sufrían esto porque re-asignan el offset del hijo con `=` cada frame.

## El fix (aplicado)

La propagación de posición ahora viaja por un **parámetro** `parentOffset` que se SUMA
a `m_ComputedRect` (no a `m_Rect.offset`):

```cpp
// AHORA (UIElement.h / UIElement.cpp)
virtual void ComputeLayout(const Vector2& availableSize,
    const Vector2& parentOffset = Vector2(0.0f, 0.0f));

void UIElement::ComputeFreeLayout(const Vector2& availableSize, const Vector2& parentOffset)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);
    m_ComputedRect.x += parentOffset.x;
    m_ComputedRect.y += parentOffset.y;
    for (auto* child : m_Children) {
        if (!child->IsActive()) continue;
        child->ComputeLayout(childSize, {m_ComputedRect.x, m_ComputedRect.y});
    }
}
```

- `m_Rect.offset` queda **estable** (relativo al anchor). La posición absoluta sale
  solo de `parentOffset`.
- Free pasa `parentOffset = {m_ComputedRect.x, m_ComputedRect.y}` a los hijos.
- Row/Column suman `parentOffset` a su propio `m_ComputedRect` pero **no** lo pasan a
  sus hijos (ya hornean la posición absoluta en el offset del hijo con `=`).
- Overrides de `ComputeLayout` actualizados: `DockSplitNode`, `DockManager`,
  `DockDropOverlay` (misma firma + `parentOffset` en `m_ComputedRect`; sus hijos usan
  offsets absolutos → parentOffset por defecto `{}`).

Resultado: los widgets con offsets absolutos (TreeView/ScrollView/dock) siguen
funcionando igual (re-asignan cada frame; el doble transitorio se pisa). Los hijos con
anchor fijo ahora quedan **estables** (se acabó la acumulación).

## Archivos tocados

- `engine/include/LeirEngine/UI/UIElement.h` — firma de `ComputeLayout` + `Compute*Layout`.
- `engine/src/UI/UIElement.cpp` — dispatch + Free/Row/Column con `parentOffset`.
- `engine/include/LeirEngine/UI/Dock/{DockSplitNode,DockManager,DockDropOverlay}.h`
  y `.cpp` — overrides de `ComputeLayout` con `parentOffset`.
- `editor/src/UI/HierarchyPanel.*` — **revertido** el workaround manual (OnLayoutComputed
  que fijaba el rect absoluto): ahora usa `AnchorSet::Stretch()` directamente, que es lo
  correcto una vez arreglado el core.

## Checkboxes

- [x] Diseñar el fix (parámetro `parentOffset`, sin mutar `m_Rect.offset`).
- [x] `ComputeLayout` + `ComputeFreeLayout` con `parentOffset` (quitar el `+=`).
- [x] `ComputeRowLayout` / `ComputeColumnLayout` con `parentOffset` en `m_ComputedRect`.
- [x] Actualizar overrides: `DockSplitNode`, `DockManager`, `DockDropOverlay`.
- [x] Simplificar `HierarchyPanel` (Stretch, sin workaround).
- [x] Build + ctest 2/2 + editor arranca/cierra limpio.
- [x] **Verificación visual del usuario**: Hierarchy sin volar; dock/tabs/splitters OK;
      drag&drop del dock OK; TreeViewDebugPanel OK; scrollviews (console) OK; outlines.
      (Verificado por el usuario el 2026-08-27: "funciona bien".)
- [x] (Deuda saldada 2026-08-27) Migrados a **offsets relativos + parentOffset**:
      `UITreeView` (viewport, items, scrollbars, edit input, ghost, drop indicator),
      `UITreeViewItem` (arrow/icon/text labels), `ScrollView` (viewport, content,
      scrollbars), `UIScrollbar` (thumb), `UITextArea` (scrollbars). Con esto ya NO
      existen dobles transitorios en el pase de layout. Los overrides del dock
      (`DockManager`/`DockSplitNode`/`DockDropOverlay`) siguen usando offsets
      absolutos + parentOffset por defecto `{}`, pero eso es correcto: esos overrides
      reemplazan el pase Free y posicionan a sus hijos **una vez por frame** (sin doble
      conteo). Verificado: build limpio, ctest 2/2, editor arranca/cierra OK.