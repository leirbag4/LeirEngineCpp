# TODO_UI_EVENT_FLOOD.md — Bug: consola se scrollea sola + glitches + FPS drop

Fecha: 2026-08-06 · Estado: ✅ arreglado (v1, verificado) · Bug reportado por el usuario

## Síntomas (reporte del usuario)

Al **mover el mouse** sobre la aplicación del editor:

1. El texto de la consola empieza a **scrollearse solo** hacia abajo y desaparece
   (clippeado por el `ScrollView`).
2. **Glitches rojos/verdes** por toda la pantalla.
3. El FPS cae de **60 → 20** y **nunca se recupera** (se queda clavado).
4. El **cierre** de la aplicación tarda bastante.

## Reproducción

El usuario tenía en `%APPDATA%\LeirEngine\settings.json`:

```json
"debug": { "ui_event_log": true, "show_overlay": true, "ui_outlines": false }
```

Con el log del editor redirigido, cada movimiento del mouse producía
**exactamente** esta secuencia:

```
[22:48:57.595] [info] [UIEvent] source=0 action=3 pos=(...) hover -> 'ConsoleScrollView'
[22:48:57.600] [trace] [Console] rebuilt 21 lines (info=true warning=true error=true)
```

## Causa raíz (diagnóstico)

### Cadena de fallo 1 — Rebuild por frame de la consola

1. Cada movimiento del mouse que cambiaba el hover emitía
   `[UIEvent] ... hover -> 'X'` a nivel **Info** (`XConsole::Println` en
   `UICanvas::ProcessPointerEvent`).
2. El ring buffer de `XConsole` retiene **solo Info/Warning/Error**
   (Trace/Debug se descartan — decisión de diseño, ver `TODO_UI_CONSOLE.md`).
   Al ser Info, **entraba al ring buffer** y bumpeaba `GetVersion()`.
3. `ConsolePanel::Refresh()` (corre cada frame) detectaba el cambio de
   `GetVersion()` y **reconstruía todas las líneas** (destroy + recreate de
   hasta 300 `UILabel`, + `GetMessages()` copiando el buffer).
4. El auto-follow (`offset.y >= maxScrollY - 1` antes del rebuild) mantenía el
   scroll al fondo → el texto "se scrolleaba solo hacia abajo".
5. El churn de destroy/create de cientos de labels **cada frame** tiraba el FPS
   de 60 a 20. Y como el rebuild en sí también corría *durante* los frames de
   mouse-move, **nunca se recuperaba**.

El log confirmaba el ritmo: `rebuilt 21 lines` en **cada** pointer event, con
20 → 58 líneas en ~0.5s de sweep.

### Cadena de fallo 2 — Overflow del UIRenderer → glitches

1. Con la consola creciendo (auto-follow + rebuild constante) el número de quads
   del frame superaba `m_MaxVertices = 8192`.
2. El `Flush()` viejo, ante overflow, hacía **clear de todos los arrays y
   `return`** sin dibujar nada.
3. Pero el overlay render pass usa `LOAD_OP_LOAD` + layout **UNDEFINED**
   (`VulkanDevice::BeginSwapchainOverlay`, `clearValueCount = 0`) → la swapchain
   mostraba **memoria sin inicializar** = los glitches rojos/verdes por toda la
   pantalla.
4. Cada overflow además imprimía `PrintWarning("UIRenderer: overflow")` (nivel
   **Warning** = retenido) → bumpeaba `GetVersion()` → más rebuilds → **feedback
   loop** que mantenía el FPS bajo y hacía el cierre lento (churn de alloc +
   `vkDeviceWaitIdle`).

## Cambios aplicados

### 1. `engine/src/UI/UICanvas.cpp` — UIEvent a nivel Trace

Los banners `[UIEvent]` (antes `XConsole::Println` = **Info**) pasaron a
`XConsole::Trace` (**Debug**). Son logs *de depuración*: no deben contaminar el
ring buffer de la consola ni bumpear `GetVersion()`.

Los 10 call-sites (todos siguen tras `if (LeirSettings::Get().debug.ui_event_log)`
o `if (trace)`, es decir, siguen activables con el setting):

| Línea | Evento |
|---|---|
| 99 | `captured '{name}'` (routing a elemento con capture) |
| 107 | `Release -> captured '{name}' ended` |
| 133 | `hover -> '{name}'` |
| 164 | `Press ... tried=[...] handled='...' focus='...' capture='...'` |
| 172 | `Press ... hit=null (empty area), focus cleared` |
| 187 | `Release ... hit='...' handled='...'` |
| 191 | `Release ... hit=null` |
| 216 | `Focus change: 'a' -> 'b'` |
| 232 | `Text '{cp}' -> focus '{name}'` |
| 244 | `Key {k} -> focus '{name}'` |

Los `[Canvas] ...` (`ProcessPointerEvent`, `HitTest`, `Press target`, `Release
target`, `Scroll`, `SendTextInput`, `SendKeyDown`, `Captured`, `ReleaseCapture`,
`Focus change`) **ya eran** `Trace` — no se tocaron.

### 2. `engine/src/UI/UIRenderer.cpp` — overflow no destructivo

`Flush()` (línea ~169) ante `totalVerts > m_MaxVertices` ahora **trunca** en
lugar de clear+return:

- Se conserva **siempre** el viewport (`vpCount * 4` vértices fijos).
- Se descartan quads **regulares desde el final** (`m_Vertices`/`m_QuadTextures`/
  `m_QuadClips` pop) hasta que todo quepa.
- Si aún sobra, se descartan quads del **debug overlay** (el último recurso; el
  contenido más prescindible).
- El buffer NUNCA se desborda → el framebuffer del overlay pass siempre se
  pinta completo → se acabaron los glitches de memoria sin inicializar.
- El aviso es `XConsole::Debug("UIRenderer: overflow, truncating {} -> {} verts")`
  (nivel Debug = no retenido → no retroalimenta el loop).

- `m_MaxVertices` subido de **8192 → 65536** en el ctor (`UIRenderer::UIRenderer`).

### 3. `engine/include/LeirEngine/Core/Log.h` — doc-comment

El doc-comment ya aclaraba que el ring buffer retiene solo Info/Warning/Error
(no requería cambio; se dejó tal cual). Semántica: **Trace/Debug son debug-only
por diseño** — si algo se quiere ver en la consola del editor debe ser
Info/Warning/Error, y viceversa: un log que solo sirve para depurar UI debe ser
Trace/Debug.

## Verificación (build + corrida)

- Build completo (preset `windows-debug`, DLL + editor) sin errores.
- Corrida del editor con sweep del mouse (script PowerShell `SetCursorPos`,
  180 movimientos ~2.2s sobre la ventana, `settings.json` con
  `ui_event_log: true`):

| Métrica | Antes (reportado) | Después (sweep) |
|---|---|---|
| `[Console] rebuilt` | 1 por cada pointer event | **1 total** (el inicial, 20 líneas) |
| `[info] [UIEvent]` | ~1 por frame | **0** (63 eventos, todos `[trace]`) |
| `UIRenderer: overflow` | continuos (warnings) | **0** |
| stderr / VUID validation | — | **vacío** |
| Líneas de stdout en corrida | flooding | 414 |
| FPS | 60 → 20 sin recuperar | sin rebuild por frame → no hay churn |

- `kill` del proceso midió 10ms (proceso sano al cerrar; el cierre lento venía
  del churn, ya eliminado).

## Notas / aprendizajes

- **Regla de oro**: todo log de UI/depuración debe ser `Trace`/`Debug`. Solo
  Info/Warning/Error deben ser *mensajes reales* del sistema (shader compilado,
  device creado, error de carga, etc.) porque son los únicos que llegan a la
  consola del editor.
- El `GetVersion()` + rebuild lazy de `ConsolePanel` es correcto; el bug era la
  **fuente** de los mensajes Info, no el panel.
- Un aviso de overflow en nivel **Warning** dentro de un `Flush()` crea un
  feedback loop cuando el propio contenido overflow causa el aviso. Los avisos
  internos del renderer deben ser Debug.

## Roadmap (pendientes conocidos, ajenos a este bug)

- Pooling de labels en `ConsolePanel` (reusar nodos en vez de destroy/recreate
  — mitigaría el churn si vuelve a haber rebuilds frecuentes por mensajes reales).
- Wrap de líneas largas en la consola (hoy truncado a `maxWidth`).

> Los bugs reportados después de este flood (scrollbars/scroll invertidos, flash al
> redimensionar splitters, caída de FPS con mucho texto) fueron corregidos — ver
> `TODO_UI_SCROLLBARS.md`, `TODO_UI_CONSOLE.md`, `TODO_UI_OPTIMIZATIONS.md`.
