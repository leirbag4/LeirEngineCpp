# UI Input System — TODO

## Bugs Reportados

1. Click en DragInput no enfoca ni permite tipear (no se ve caret, no se ve texto insertado)
2. Backspace/Delete no borran caracteres
3. Cursor siempre al final, no se puede posicionar con click
4. No hay caret visual
5. Color de texto hardcodeado en UIRenderer

## Fase 0 — Hotfix: click + teclado básico

Objetivo: que el DragInput reciba foco, se pueda tipear y borrar.

### F0.1 — UITextInput::OnKeyDown

**Archivos:** `engine/include/LeirEngine/UI/UITextInput.h`, `engine/src/UI/UITextInput.cpp`

- [ ] Declarar `bool OnKeyDown(int key) override` en UITextInput.h
- [ ] Implementar en UITextInput.cpp:
  - `Key::Backspace` → `DeleteChar()` (borra antes del cursor)
  - `Key::Delete` → borra después del cursor (nuevo helper `DeleteForward()`)
  - `Key::Left` → `m_CursorPos = max(0, m_CursorPos - 1)`
  - `Key::Right` → `m_CursorPos = min(len, m_CursorPos + 1)`
  - `Key::Home` → `m_CursorPos = 0`
  - `Key::End` → `m_CursorPos = (int)m_Text.size()`
  - Retornar `true` si se manejó

### F0.2 — Sacar `m_Focused` duplicado de UIFloatInput

**Archivos:** `engine/include/LeirEngine/UI/UIFloatInput.h`, `engine/src/UI/UIFloatInput.cpp`

- [ ] Eliminar `bool m_Focused = false;` de UIFloatInput.h (línea 27)
- [ ] En UIFloatInput.cpp: todas las referencias a `m_Focused` pasan a usar el heredado de UITextInput
- [ ] `OnFocus()`: sacar `m_Focused = true` propio, dejar solo `UITextInput::OnFocus()`
- [ ] `OnBlur()`: chequear `UITextInput::OnBlur()` o usar el field heredado

### F0.3 — UIFloatInput::OnKeyDown reenviar al padre

**Archivos:** `engine/src/UI/UIFloatInput.cpp`

- [ ] `UIFloatInput::OnKeyDown`: si no es Enter, llamar `UITextInput::OnKeyDown(key)` y retornar su resultado
- [ ] Así backspace/delete/arrows funcionan incluso en FloatInput

### F0.4 — Verificar que event hooks estén conectados

**Archivo:** `engine/src/UI/UICanvas.cpp`

- [ ] Confirmar que `ConnectToInputSystem()` registra hooks de Key y Char
- [ ] Confirmar que `SendKeyDown` / `SendTextInput` se llaman correctamente
- [ ] Compilar + testear

---

## Fase 1 — Caret + color + click-position

### F1.1 — API de color + caret tracking en UITextInput

**Archivos:** `engine/include/LeirEngine/UI/UITextInput.h`, `engine/src/UI/UITextInput.cpp`

- [ ] Agregar `void SetTextColor(const Vector4& c)` / `const Vector4& GetTextColor() const`
- [ ] Agregar `float GetCursorX(Font* font) const` — calcula la posición X del caret sumando advances de `m_Text[0..m_CursorPos)`
- [ ] Agregar `int GetCharIndexAtX(Font* font, float localX) const` — devuelve el índice del caracter en la posición X dada (para click-to-position)
- [ ] Agregar `float m_CaretTimer = 0` + `bool IsCaretVisible() const` (parpadeo ~0.5s on/off)
- [ ] Llamar `m_CaretTimer += deltaTime` desde algún lado (o en `OnFocus/OnBlur` resetear)

### F1.2 — Click-to-position cursor

**Archivo:** `engine/src/UI/UITextInput.cpp`

- [ ] `OnPointerDown`: usar `GetCharIndexAtX(font, localX)` para setear `m_CursorPos`
  - `localX = pos.x - (cr.x + 4.0f)` (el +4 es el padding del input)
- [ ] `OnPointerMove` cuando focused: actualizar cursor position por arrastre

### F1.3 — Caret rendering en UIRenderer

**Archivo:** `engine/src/UI/UIRenderer.cpp`

- [ ] En bloque `UITextInput`, después de dibujar el texto:
  - Si `input->IsCaretVisible()`, dibujar un quad de 1px de ancho, alto = lineHeight, color blanco
  - Posición: `cr.x + 4.0f + input->GetCursorX(font)`, `cr.y + (cr.w - lineH) * 0.5f`
- [ ] Usar `input->GetTextColor()` en vez del Vector4{1,1,1,1} hardcodeado

### F1.4 — Caret blinking timer

**Archivo:** Se necesita un `Update(deltaTime)` en UITextInput o manejar el timer desde el canvas/application

- [ ] Opción simple: `UITextInput::OnFocus()` resetea timer, `OnBlur()` apaga caret
- [ ] Contador por frames en vez de tiempo real: `m_CaretCounter = (m_CaretCounter + 1) % 60`, visible cuando `< 30`
- [ ] Esto evita tener que pasar deltaTime al widget

---

## Fase 2 — Selección de texto

### F2.1 — Selection state

**Archivos:** `engine/include/LeirEngine/UI/UITextInput.h`, `engine/src/UI/UITextInput.cpp`

- [ ] Agregar `int m_SelectionStart = -1` (−1 = sin selección)
- [ ] Agregar helpers:
  - `bool HasSelection() const`
  - `std::string GetSelectedText() const`
  - `void DeleteSelection()` — borra el rango seleccionado y ajusta m_CursorPos
  - `int GetSelBegin() const` / `int GetSelEnd() const` — min/max de selectionStart y cursorPos
- [ ] `InsertChar`: si hay selección, llamar `DeleteSelection()` antes de insertar
- [ ] `DeleteChar`: si hay selección, llamar `DeleteSelection()` en vez de borrar un caracter

### F2.2 — Selection por shift+arrow

**Archivo:** `engine/src/UI/UITextInput.cpp`

- [ ] En `OnKeyDown`: si shift está presionado y se mueve el cursor, setear `m_SelectionStart` si no estaba seteado, o mantenerlo
- [ ] Si shift NO está presionado, limpiar `m_SelectionStart = -1`

### F2.3 — Selection rendering

**Archivo:** `engine/src/UI/UIRenderer.cpp`

- [ ] En bloque `UITextInput`, antes de dibujar el texto:
  - Si `input->HasSelection()`, dibujar un quad azul semitransparente ({0.3, 0.5, 1.0, 0.4}) desde la X del selection start hasta la X del selection end

### F2.4 — Selection por click-drag

**Archivo:** `engine/src/UI/UITextInput.cpp`

- [ ] `OnPointerDown`: setear `m_SelectionStart = m_CursorPos` (inicio de selección)
- [ ] `OnPointerMove`: actualizar `m_CursorPos` via `GetCharIndexAtX`, y la selección se extiende entre `m_SelectionStart` y `m_CursorPos`

---

## Fase 3 — UITextArea (multiline)

### F3.1 — Clase UITextArea

**Archivos nuevos:** `engine/include/LeirEngine/UI/UITextArea.h`, `engine/src/UI/UITextArea.cpp`

- [ ] `UITextArea` hereda de `UITextInput`
- [ ] Override `GetMinSize()` — al menos 100x60
- [ ] Override `OnTextInput` — acepta `\n` (Enter → inserta newline)
- [ ] Override `OnKeyDown` — Enter inserta `\n`, Up/Down navega entre líneas
- [ ] Almacena líneas virtualmente (el texto plano tiene `\n`)
- [ ] `GetCursorLine()` / `GetCursorCol()` — calcula línea/columna del cursor
- [ ] Scroll offset vertical: cuando el cursor scrolea fuera del área visible, desplazar el contenido

### F3.2 — Layout multiline con Font::LayoutText

**Archivo:** `engine/src/UI/UITextArea.cpp`

- [ ] Usar `Font::LayoutText(text, maxWidth)` para obtener los quads de todas las líneas
- [ ] `GetCharIndexAtX` adaptado para multiline (determinar primero qué línea, luego qué columna)
- [ ] `GetCursorX` para multiline: posición X dentro de la línea actual

### F3.3 — UITextArea rendering

**Archivo:** `engine/src/UI/UIRenderer.cpp`

- [ ] Agregar `dynamic_cast<UITextArea*>` en el render loop
- [ ] Renderizar el texto con scroll offset vertical
- [ ] Caret en la línea activa
- [ ] Selección en multiline

### F3.4 — Scrollbars

**Archivo:** `engine/include/LeirEngine/UI/UITextArea.h`, `engine/src/UI/UITextArea.cpp`

- [ ] Agregar scrollbar vertical (usar UIImage o UIPanel como track + thumb)
- [ ] Scrollbar horizontal (opcional, solo cuando hay líneas más anchas que el viewport)
- [ ] Sincronizar scroll offset con la posición del caret

---

## Fase 4 — DebugTextPanel

### F4.1 — Crear DebugTextPanel

**Archivos nuevos:** `editor/src/UI/DebugTextPanel.h`, `editor/src/UI/DebugTextPanel.cpp`

- [ ] Hereda `Leir::UIPanel`
- [ ] Contiene:
  - `UITextInput` normal (placeholder: "escribe algo...")
  - `UITextArea` multiline (placeholder: "multiline...")
  - `UIFloatInput` numérico
  - `UILabel` que muestra en tiempo real el contenido de cada input
  - Labels con `m_CursorPos`, `m_SelectionStart`, focused element name
- [ ] Nombre empieza con "Debug" para que renderice en capa debug overlay
- [ ] `Refresh()` actualiza los labels cada frame

### F4.2 — Integrar en editor

**Archivo:** `editor/src/main.cpp`

- [ ] Incluir `DebugTextPanel.h`
- [ ] Crear instancia en `OnInit()`, agregar al root, setear font
- [ ] Llamar `m_DebugTextPanel->Refresh()` en `OnUpdate()`

---

## Estado general

| Fase | Estado |
|------|--------|
| F0.1 — UITextInput::OnKeyDown | ✅ |
| F0.2 — Sacar m_Focused duplicado | ✅ |
| F0.3 — UIFloatInput reenviar al padre | ✅ |
| F0.4 — Verificar event hooks | ✅ |
| F1.1 — API color + caret tracking | ✅ |
| F1.2 — Click-to-position | ✅ |
| F1.3 — Caret rendering | ✅ |
| F1.4 — Caret blinking | ✅ |
| F2.1 — Selection state | ✅ |
| F2.2 — Shift+arrow selection | ✅ |
| F2.3 — Selection rendering | ✅ |
| F2.4 — Click-drag selection | ✅ |
| F3.1 — UITextArea class | ✅ |
| F3.2 — Layout multiline | ✅ |
| F3.3 — UITextArea rendering | ✅ |
| F3.4 — Scrollbars | ⏳ (vertical scroll dependiente de ScrollView integration) |
| F4.1 — DebugTextPanel | ✅ |
| F4.2 — Integrar en editor | ✅ |
