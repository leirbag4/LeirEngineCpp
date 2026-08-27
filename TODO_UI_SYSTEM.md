# TODO UI System — Layout model & roadmap

Documento de referencia del sistema de layout de UI de LeirEngine, **actualizado a día de hoy
(2026-08-27)** con el fix del core (`parentOffset`). Incluye el mapa con el modelo flex de HTML,
las features pendientes de analizar y la visión del futuro lenguaje declarativo **LXML**.

---

## 1. Visión

Desde el inicio del proyecto el objetivo es un modelo de UI **moderno estilo web (HTML/CSS)**:
elementos anidados, anchors, flex (Row/Column), y a futuro un **lenguaje declarativo propio
(LXML — Leir XML)** parecido a HTML para construir la UI desde markup en lugar de C++.

El camino elegido (correcto y estándar de la industria — Unity UXML, Godot `.tscn`, Qt QML):
primero el **runtime en C++** (los primitivos de layout), y el lenguaje declarativo **encima**
serializando la configuración del runtime. Un markup no se puede diseñar bien sin un modelo
runtime estable.

Este documento describe el runtime actual (sección 2-6), el fix del core (sección 7), el mapa
con CSS (sección 8), las features a analizar (sección 9) y el LXML (sección 10).

---

## 2. El modelo: config vs resultado

Cada `UIElement` tiene **dos rects** con responsabilidades separadas:

| Miembro | Rol | Mutado por el runtime |
|---|---|---|
| `m_Rect` (`anchor` + `offset`) | **Configuración** del elemento (relativa al padre/available size) | **NUNCA** |
| `m_ComputedRect` | **Resultado** del layout (rect absoluto en pantalla) | Recalculado cada frame |

- `AnchorSet` — 9 combinaciones de anclaje (TopLeft, Stretch, ...): qué punto del elemento se
  amarra a qué punto del padre.
- `OffsetSet` — offsets relativos al anchor (izquierda/arriba/derecha/abajo).
- `pivot` — punto de rotación/escala (no participa del layout).

La regla de oro: **el layout es derivado (padre→hijo), nunca acumulado**. El runtime nunca
escribe en `m_Rect.offset`; la posición absoluta sale del `parentOffset` que cada padre pasa
a sus hijos y se suma a `m_ComputedRect`.

---

## 3. Modos de layout (`LayoutMode`)

| Modo | Comportamiento | Equivalente CSS |
|---|---|---|
| `Free` | Cada hijo se posiciona por su anchor/offset dentro del área disponible del padre (sin flujo). | `position: absolute` |
| `Row` | Hijos en fila horizontal; tamaños según `SizePolicy`; `spacing` entre ellos. | `display: flex; flex-direction: row` |
| `Column` | Hijos en columna vertical; igual que Row pero en el eje vertical. | `display: flex; flex-direction: column` |

Los modos `Row`/`Column` **re-asignan** el offset de cada hijo cada frame (con `=`) para
posicionarlos en el flujo; `Free` deja que cada hijo se posicione por su anchor.

---

## 4. `SizePolicy` (cómo mide el flujo Row/Column a cada hijo)

| Policy | Tamaño en el eje principal |
|---|---|
| `Fixed` | `GetMinSize()` (tamaño fijo del hijo). |
| `Content` | `GetContentSize()` (el contenido real: texto, etc.), mínimo `GetMinSize()`. |
| `Fill` | Reparte equitativamente el espacio sobrante entre todos los `Fill`. |
| `Grow` | `GetMinSize()` + parte proporcional del sobrante (más que `Fill`, crece con el espacio). |

`GetMinSize()`/`GetContentSize()` son virtuales — cada widget define su tamaño natural
(UIButton, UILabel, UITreeView, ...).

---

## 5. Propagación de posición (el mecanismo actual)

`UIElement::ComputeLayout(availableSize, parentOffset)`:

```cpp
virtual void ComputeLayout(const Vector2& availableSize,
    const Vector2& parentOffset = Vector2(0.0f, 0.0f));

// Cada modo suma parentOffset a su propio m_ComputedRect:
void UIElement::ComputeFreeLayout(const Vector2& availableSize, const Vector2& parentOffset)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);
    m_ComputedRect.x += parentOffset.x;
    m_ComputedRect.y += parentOffset.y;
    for (auto* child : m_Children)
        child->ComputeLayout(childSize, {m_ComputedRect.x, m_ComputedRect.y});
}
```

- `Free` pasa su `m_ComputedRect.xy` (absoluto) como `parentOffset` a cada hijo.
- `Row`/`Column` suman `parentOffset` a su propio `m_ComputedRect` pero **hornean** la posición
  del padre en el offset de cada hijo con `=` (los hijos quedan absolutos).
- Overrides del dock (`DockSplitNode`, `DockManager`, `DockDropOverlay`) reciben `parentOffset`
  y la suman a su `m_ComputedRect`; sus hijos usan offsets absolutos → `parentOffset` por
  defecto `{}`.

Garantiza que **los nietos hereden la cadena absoluta completa**: labels/inputs dentro de Rows
dentro de paneles (ej. `UIDragFloatInput` dentro de `UITestPanel`) quedan posicionados
correctamente.

---

## 6. Render / hit-testing

- `UIRenderer` dibuja usando `m_ComputedRect` (absoluto).
- `UICanvas::HitTestRecursive` usa los mismos `m_ComputedRect` → **lo que se ve == lo que se
  clickea** (la escala solo cambia la resolución física, no la semántica lógica).

---

## 7. El fix del core (`+=` → `parentOffset`) — registro

**Bug** (`TODO_COMPUTE_FREE_LAYOUT_FIX.md`): `ComputeFreeLayout` mutaba el offset de cada hijo
con `+=` (sumaba la posición absoluta del padre **cada frame**), contaminando permanentemente
`m_Rect.offset` (que es config). Cualquier hijo con anchor fijo / no re-asignado (un `Stretch`)
**acumulaba** la posición del padre frame a frame → los elementos "volaban" hacia abajo/derecha
(el `HierarchyPanel` lo destapó; `DockManager`/`DockDropOverlay` tenían workarounds).

**Causa histórica**: el `+=` se agregó el 2026-07-29 (`2a9f440`) como fix rápido para la
propagación en layouts anidados (era del `UIDragFloatInput`/`UITestPanel`); funcionaba por
accidente para los widgets que re-asignaban offsets, y nunca se revisó.

**Fix (2026-08-27)**: propagación por **parámetro** `parentOffset` sumado a `m_ComputedRect`,
sin tocar `m_Rect.offset`. La config quedó estable → los hijos con anchor fijo ya no acumulan,
y el round-trip de serialización (importante para LXML) es fiel.

---

## 8. Mapa con el modelo web (HTML/CSS)

| LeirEngine | CSS |
|---|---|
| `LayoutMode::Row` / `Column` | `flex-direction: row / column` |
| `SizePolicy::Fixed` | ancho/alto explícito |
| `SizePolicy::Content` | `width: max-content` |
| `SizePolicy::Fill` / `Grow` | `flex-grow: 1` / `flex-grow: N` |
| `AnchorSet` (9 anclas) | `position: absolute` + `top/left/right/bottom` |
| `m_Padding`, `m_Spacing` | `padding`, `gap` (parcial) |
| `GetMinSize()` / `GetContentSize()` | `min-width/min-height`, contenido |

**Lo que YA existe es un flex simplificado** (dirección + grow), complementado con anchors
(positioning absoluto). No es flexbox completo.

---

## 9. Features pendientes de analizar (roadmap de layout)

Lista abierta — para evaluar antes/después del LXML. Cada una puede implementarse en el
runtime o resolverse solo en el markup.

### Flex (estilo CSS)
- [ ] **`justify-content`** — alineación del eje principal (flex-start/end, center,
      space-between/around/evenly).
- [ ] **`align-items` / `align-self`** — alineación del eje cruzado por contenedor / por hijo.
- [ ] **`flex-grow` / `flex-shrink` / `flex-basis`** — el modelo completo de flex-grow hoy es
      `Fill`/`Grow` (parcial); falta shrink/basis.
- [ ] **`flex-wrap`** — envolver a la siguiente línea cuando se desborda.
- [ ] **`gap`** — separación uniforme entre hijos (hoy `m_Spacing` es global del contenedor).
- [ ] **`flex-direction` dinámica / `order`** — reordenar hijos en el flujo sin cambiar el árbol.

### Grid
- [ ] **`display: grid`** — grid explícito (columnas/filas/tracks/gap), separado del flex.
      Evaluar si hace falta para el editor (paneles tipo Contents, vistas 2D) o alcanza con
      flex + anchors.

### Layout avanzado
- [ ] **Auto-layout de paneles** (política de tamaño por defecto de hijos en Row/Column:
      hoy `Fixed` por default; evaluar `Content` como default más "web").
- [ ] **Overflow / scroll por layout** (hoy `ScrollView`/`UITreeView`/`UITextArea` lo resuelven
      internamente con un viewport clipeado).
- [ ] **Porcentajes en `SizePolicy`** (`width: 50%` del padre).

---

## 10. LXML (Leir XML) — visión

Lenguaje declarativo futuro (P4 del plan, ver `TODO_FILE_SYSTEM.md`) para construir la UI
desde markup similar a HTML. Idea de forma (no es diseño final):

```xml
<Row width="fill" gap="4" padding="6">
    <Label text="Position" />
    <DragFloat value="x" />
    <Button text="Apply" flexGrow="1" />
</Row>
```

Principios que ya están garantizados por el modelo actual:
- LXML serializa la **configuración** (`LayoutMode`, `SizePolicy`, `AnchorSet`, `OffsetSet`,
  `GetMinSize` de widgets), nunca los rects computados.
- Con el fix del `+=`, la config es **estable** → round-trip save/load limpio (sin drift).
- Cuando se diseñe LXML hay que decidir: **exponer los primitivos actuales tal cual**, o
  **extender el runtime al flex completo** (sección 9) para que el markup sea más expresivo.

---

## 11. Archivos clave

- `engine/include/LeirEngine/UI/UIElement.h` — config/resultado, `ComputeLayout(parentOffset)`.
- `engine/include/LeirEngine/UI/Rect2D.h` — `AnchorSet`, `OffsetSet`, `Rect2D::GetRect`.
- `engine/src/UI/UIElement.cpp` — `ComputeFreeLayout/RowLayout/ColumnLayout`.
- `engine/src/UI/Dock/{DockSplitNode,DockManager,DockDropOverlay}.cpp` — overrides del dock.
- `TODO_COMPUTE_FREE_LAYOUT_FIX.md` — registro del bug/fix del core.
- `TODO_FILE_SYSTEM.md` — donde vive la etapa LXML.