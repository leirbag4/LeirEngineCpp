# TODO_CONSOLE_PERF.md — Rendimiento con mucho texto (consola/UI)

Fecha: 2026-08-06 · Estado: ⚠️ diagnóstico hecho, **pendiente de medir y optimizar**

## Síntoma (reportado por el usuario)

Cuanto más texto se junta en la consola, más caen los FPS (hasta ~10 FPS con mucho
texto). El texto que NO se ve en pantalla (fuera del viewport del ScrollView)
sigue influyendo en la performance.

## Cómo se dibuja el texto de la consola (flujo por frame)

```
CoreApplication::Run (Core/CoreApplication.cpp:138)
  ├── glfwPollEvents()
  ├── EventQueue::Process()
  ├── scene->OnUpdate(dt)
  ├── EditorApp::OnUpdate(dt)  (editor/src/main.cpp:491)
  │     ├── m_Canvas->SetScreenSize()
  │     ├── m_ConsolePanel->Refresh()          ← (tras el fix del flash, ANTES de UpdateLayout)
  │     ├── m_Canvas->UpdateLayout()           ← Layout completo de TODO el árbol, cada frame
  │     ├── UpdateViewportRenderTarget()
  │     ├── m_DockManager->Process()
  │     └── paneles Refresh()
  ├── InputManager::Update()
  └── EditorApp::OnRender()  (editor/src/main.cpp:566)
        └── m_UIRenderer->Render(cmd, canvas)  ← Recorre el árbol con GetComputedRect
```

## Hallazgo 1 — El LAYOUT mide el texto de TODAS las líneas, cada frame (fuera de rango también)

El renderer NO recalcula layout: solo usa `GetComputedRect()`
(`UIRenderer::RenderElement`, `UIRenderer.cpp:341`). El layout se computa en
`UICanvas::UpdateLayout()` → `ComputeLayout` → `ScrollView::OnLayoutComputed`
(`engine/src/UI/ScrollView.cpp:72`).

En `ScrollView::OnLayoutComputed`:
- `m_Content->ComputeLayout({availW, 8192.0f})` (ScrollView.cpp:88) → el content
  es un Column de labels → `ComputeColumnLayout` (`UIElement.cpp:227`).
- `ComputeColumnLayout` pide `child->GetNaturalSize()` a **cada** label hijo
  (UIElement.cpp:85 vía `GetNaturalSize()` → `GetMinSize()`).
- `UILabel::GetMinSize()` → `m_Font->MeasureText(m_Text, ...)` (**UILabel.cpp:20**)
  → recorre TODOS los chars del texto con lookups al glyph cache
  (`Font::MeasureText`, `Font.cpp:206`).

**Resultado**: con N líneas, se ejecuta `MeasureText` para las N líneas **cada
frame**, estén visibles o no. Es trabajo O(total de chars) por frame, sin culling
de ningún tipo en la fase de layout. 300 líneas × ~50 chars ≈ 15.000 lookups/frame
solo para medir.

Además `GetMaxScrollY()` → `GetContentSize()` (ScrollView.cpp:43 →
`UIElement::GetContentSize` → recorre TODOS los hijos, UIElement.cpp:82) se llama
en `OnLayoutComputed` y en el clamp (ScrollView.cpp:78-79). También O(children).

**Por qué "no se ve en pantalla pero igual frena"**: el layout NO culla; procesa
todas las líneas del content column aunque estén por arriba/abajo del viewport.

## Hallazgo 2 — El RENDER sí culla por label, pero dibuja 1 draw call POR GLYPH

`UIRenderer::RenderElement` (UIRenderer.cpp:338):
- **Culling**: si el elemento tiene clip (`ScrollView` con `SetClip(true)`), el
  rect del label se intersecta contra el clip y los labels enteros fuera del
  viewport se cortan con `return` (UIRenderer.cpp:360-364). ✅ El render sí
  descarta líneas fuera de rango.
- **Pero**: cada glyph visible → `BuildBatch` (UIRenderer.cpp:473-481) → cada
  quad es un `vkCmdDraw` separado con `vkCmdSetScissor` + bind de descriptor
  set (UIRenderer.cpp:239-269). Con ~20-30 líneas visibles × ~50 chars ≈
  1.000-1.500 draw calls solo de la consola, por frame. Es CPU-bound en Vulkan
  (draw call count), no GPU.

`UILabel::GetGlyphQuads()` usa quads cacheados (`m_GlyphQuads`, se llena en
`UILabel::Rebuild` con `LayoutText` — UILabel.cpp:40), así que NO se re-layoutan
los glyphs cada frame... a menos que el label esté dirty. PERO el Rebuild se
dispara en `OnLayoutComputed` (UILabel.cpp:24) que corre cada frame; el guard
`if (!m_Dirty) return` (UILabel.cpp:31) evita re-layout. Ojo: con el rebuild
de `ConsolePanel::RebuildLines` se crean labels NUEVOS cada vez que llega texto.

## Hallazgo 3 — Medición necesaria

Con el nuevo `DebugPanel` (X10/X50/X100) vamos a medir:
1. FPS con 10 / 50 / 100 / 300+ líneas acumuladas en la consola.
2. Confirmar cuál de los dos costos domina (¿layout O(chars) o draw calls O(glyphs visibles)?).
3. Separar las fases: pausar `UpdateLayout()` vs pausar render, para aislar el bottleneck.

## Fixes candidatos (a decidir tras medir)

### A) Cachear el tamaño natural del label (arregla Hallazgo 1)
`UILabel` ya cachea `m_GlyphQuads`; falta cachear `GetMinSize()`/`MeasureText`.
- Invalidar el cache cuando cambia `m_Text` (ya hay `MarkDirty()` en `SetText`),
  el `Font`, el `m_MaxWidth`, o el wrap.
- `GetMinSize()` devolvería `m_CachedSize` sin medir nada por frame.
- Esto convierte el layout de O(total chars) → O(1) por label.

### B) Batch de draw calls (arregla Hallazgo 2)
- Hoy cada quad = 1 `vkCmdDraw` con scissor + bind propio.
- Batch por textura (todas las fuentes usan 1 atlas) + mismo scissor → agrupar
  quads contiguos en un solo `vkCmdDraw` (el vertex buffer ya los tiene
  consecutivos; solo cambiar el count y el `firstVertex`).
- Idealmente: 1 draw por textura por capa (regular / viewport / debug), con
  `vkCmdSetScissor` solo cuando cambia el clip.

### C) Culling de layout por líneas visibles (más invasivo)
- Skip del `ComputeColumnLayout` de labels fuera del viewport (solo necesario si
  A no alcanza). Más frágil porque los labels fuera igual aportan al
  `GetContentSize` del scroll (scrollbar).

## Archivos clave

- `engine/src/UI/ScrollView.cpp` — OnLayoutComputed (mide contenido completo)
- `engine/src/UI/UIElement.cpp` — ComputeColumnLayout / GetContentSize / GetNaturalSize
- `engine/src/UI/UILabel.cpp` — GetMinSize → MeasureText (sin cache)
- `engine/src/UI/Font.cpp` — MeasureText / LayoutText (costo por char)
- `engine/src/UI/UIRenderer.cpp` — RenderElement (cull por label) + Flush (1 draw/quad)
- `editor/src/UI/ConsolePanel.cpp` — RebuildLines (crea labels nuevos por rebuild)
