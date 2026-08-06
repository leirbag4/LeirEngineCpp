# Log System — TODO

> **Objetivo:** reemplazar spdlog por un sistema de logging propio (`XConsole`), sin dependencias
> externas, con soporte futuro para una consola dockeable en el editor.

## Estado actual

| Hito | Estado |
|------|--------|
| Diseño + documentación | ✅ |
| `XConsole` (Core/Log.h + Core/Log.cpp) | ✅ |
| Mini-formatter runtime propio (sin fmt/spdlog) | ✅ |
| Ring buffer en memoria (para consola futura) | ✅ |
| Sinks: stdout (info/warn/trace/debug) + stderr (error) | ✅ |
| Quitar spdlog de CMake (dependencies + engine) | ✅ |
| Migrar call-sites `spdlog::` → `XConsole::` | ✅ |
| Verificar build + corrida | ✅ |
| `ConsolePanel` dockeable (filtros Info/Warn/Error) | ✅ implementado (ver `TODO_UI_CONSOLE.md`) |
| Niveles extra en consola UI (Trace/Debug toggle) | ⏳ opcional (requiere `SetRetainLevel` en `XConsole`) |

---

## Motivación

- **Código propio siempre:** el engine debe depender lo menos posible de bibliotecas de terceros.
  spdlog era la única dependencia "menor" (todo lo demás —Vulkan, Jolt, GLFW— es infraestructura
  real). El logging es fácil de hacer bien a mano.
- **Una API unificada:** antes cada archivo llamaba `spdlog::info(...)`, `spdlog::error(...)`, etc.
  Ahora todo pasa por `XConsole::Println / PrintWarning / PrintError / Trace / Debug`.
- **Consola de editor:** el objetivo a mediano plazo es una consola dockeable estilo Unity con 3
  botones de filtro (Info / Warning / Error). El ring buffer interno ya guarda todos los mensajes
  emitidos para alimentar ese panel.
- **Aislar el backend:** si algún día se reintroduce spdlog (o se cambia de destino de salida),
  solo se toca `Log.cpp`; ningún call-site cambia.

---

## Conceptos clave (niveles vs. sinks)

Estos dos conceptos se confunden seguido y son la base del diseño:

### Nivel (verbosity) — filtro de "cuánto detalle emito"

Escalera: `Trace < Debug < Info < Warning < Error`.

- Si el nivel global (`XConsole::SetLevel`) está en `Info`, los mensajes `Trace` y `Debug` se
  **descartan en el origen**: ni salen a consola ni entran al ring buffer.
- Son "silenciosos por defecto": se usan subiendo el nivel mientras se debuggea algo puntual.
- Ejemplos de uso en el repo:
  - `Trace` (por evento, muy ruidoso): hit-tests, capture de puntero, double-click, drag moves.
  - `Debug` (diagnóstico de desarrollo): reservado para futura instrumentación.
  - `Info` (`Println`): operación normal — "Editor initialized", "Image loaded: X (512x512)".
  - `Warning` (`PrintWarning`): algo raro pero no fatal — "Settings not found, creating defaults".
  - `Error` (`PrintError`): falló una operación — "Failed to load image", "Vulkan: {msg}".

### Sink — destino de salida

Un mismo mensaje emitido puede ir a varios destinos:

- **stdout** — Info/Warning/Trace/Debug (la terminal que se abre al correr el editor, que es app
  de subsistema consola).
- **stderr** — Error (se distingue de los demás en el flujo de la terminal).
- **Ring buffer en memoria** — siempre se guarda (capado a 1000 mensajes, con timestamp). Es el
  que alimentará al `ConsolePanel` del editor.

> **Resumen:** "trace" no es algo aparte del log de consola ni de archivo. Es el **nivel más
> detallado** dentro del mismo sistema. La consola de UI (a futuro) mostrará Info/Warning/Error con
> sus 3 botones de filtro, y Trace/Debug no aparecen ahí salvo que se suba el nivel y se quiera
> verlos (categoría extra opcional).

---

## Cómo lo hacen los motores profesionales

Patrón universal: **UNA sola API de logging + varios sinks** (nunca dos clases).

| Motor | API | Sinks típicos |
|-------|-----|---------------|
| Unity | `Debug.Log / LogWarning / LogError` | Console window del editor + `Editor.log` |
| Unreal | `UE_LOG(Category, Verbosity, ...)` | Output Log + `Saved/Logs/*.log` rotativo + consola OS |
| Godot | `print() / push_warning() / push_error()` | Panel Output |

- Los **niveles** (Verbose/VeryVerbose en Unreal, Trace/Debug nuestro) existen para filtrar ruido.
- La "consola del editor" **nunca es un segundo logger**: es un panel de UI suscrito al stream de
  logs (sink custom con ring buffer) + filtros por tipo.
- El sistema de "console commands" (p. ej. `Stat FPS` en Unreal) es **otra cosa**: un intérprete
  de comandos de texto, separado del logging.

Nuestro `XConsole` sigue exactamente ese patrón.

---

## API pública (`engine/include/LeirEngine/Core/Log.h`)

```cpp
namespace Leir {

enum class LogLevel : uint8_t { Trace, Debug, Info, Warning, Error };

struct LogMessage {
    LogLevel level;
    std::string text;
};

class LEIR_API XConsole {
public:
    // Formatos: literales o strings, con placeholders tipo fmt (ver sección Formatter).
    template <typename... Args> static void Println(const char* fmt, Args&&... args);        // Info
    template <typename... Args> static void Println(const std::string& fmt, Args&&... args);
    template <typename... Args> static void PrintWarning(const char* fmt, Args&&... args);   // Warning
    template <typename... Args> static void PrintWarning(const std::string& fmt, Args&&... args);
    template <typename... Args> static void PrintError(const char* fmt, Args&&... args);     // Error → stderr
    template <typename... Args> static void PrintError(const std::string& fmt, Args&&... args);
    template <typename... Args> static void Trace(const char* fmt, Args&&... args);          // silencioso por defecto
    template <typename... Args> static void Trace(const std::string& fmt, Args&&... args);
    template <typename... Args> static void Debug(const char* fmt, Args&&... args);
    template <typename... Args> static void Debug(const std::string& fmt, Args&&... args);

    static void SetLevel(LogLevel level);
    static LogLevel GetLevel();

    static std::vector<LogMessage> GetMessages();   // copia del ring buffer (para la consola UI)
    static void Clear();
};

}
```

Uso:

```cpp
XConsole::Println("Editor initialized");
XConsole::Println("Viewport resized to {}x{} ({}x{} physical)", w, h, fw, fh);
XConsole::PrintWarning("Settings file '{}' not found, creating with defaults", m_Path);
XConsole::PrintError("Failed to load image: {}", path);
XConsole::Trace("[Canvas] HitTest: {} (prev hover: {})", a, b);
XConsole::SetLevel(XConsole::LogLevel::Trace);
```

### Mapeo desde spdlog

| spdlog | XConsole |
|--------|----------|
| `spdlog::info` | `XConsole::Println` |
| `spdlog::warn` | `XConsole::PrintWarning` |
| `spdlog::error` | `XConsole::PrintError` |
| `spdlog::critical` | `XConsole::PrintError` (colapsado en Error) |
| `spdlog::trace` | `XConsole::Trace` |
| `spdlog::debug` | `XConsole::Debug` |
| `spdlog::set_level(spdlog::level::X)` | `XConsole::SetLevel(XConsole::LogLevel::X)` |

---

## Implementación (`engine/src/Core/Log.cpp`)

### Formatter runtime propio

Sin fmt ni spdlog. Variádico con `std::any` + `std::ostringstream`:

- Los placeholders `{}` y `{:<spec>}` se sustituyen en runtime (los formatos pueden ser strings
  no literales, p. ej. los de `UICanvas`).
- Especificadores soportados (los que hoy usa el repo + generales):
  - `{}` — genérico (`ostringstream`, bool → `true`/`false`, char → carácter, `const char*`/`char*`/`std::string` → texto)
  - `{:.Nf}` / `{:.2f}` — float con N decimales (usa `std::fixed`)
  - `{:Nd}` / `{:02d}` / `{:04d}` — int con ancho y padding de ceros opcional
- Tipos de argumento soportados: `int`, `unsigned`, `long`, `unsigned long`, `long long`,
  `unsigned long long` (cubre `size_t` en x64), `float`, `double`, `char`, `bool`,
  `const char*`, `char*` (arrays de char, p. ej. `VkPhysicalDeviceProperties::deviceName`),
  `std::string`.
- Tipo desconocido o placeholder sin argumento → `<?>` (no crashea).

Implementación de referencia (la lógica de `Format`/`FormatArg` vive en `Log.cpp`):

```cpp
template <typename... Args>
std::vector<std::any> XConsole::argv(Args&&... args) {
    std::vector<std::any> v;
    v.reserve(sizeof...(args));
    (v.emplace_back(std::forward<Args>(args)), ...);
    return v;
}
```

### Thread-safety

- Todo el estado (nivel global, ring buffer) vive en `function-local static` dentro de `Log.cpp`
  (Meyers singleton) → no hay problemas de orden de inicialización de estáticos.
- `std::mutex` protege nivel + ring buffer + salida. Jolt corre en multithread; los logs de otros
  hilos no corromperán estado.

### Salida

- Formato por línea: `[HH:MM:SS.mmm] [level] mensaje`
- Info/Warning/Trace/Debug → `stdout` (`fputs` + `fflush`)
- Error → `stderr`
- El mensaje también entra al ring buffer (cap `kMaxMessages = 1000`, descarta el más viejo).

### Por qué `std::any` y no `std::variant`

`std::variant` con 10 alternativas tiene reglas de resolución de overload frágiles (un `char[256]`
podría ambigüear entre `const char*` y `bool`, un `fs::path` rompería el compile). `std::any`
decae el tipo (arrays → puntero, etc.) y permite `any_cast<T>` exacto por intento en `FormatArg`.
Cualquier tipo no soportado se resuelve como `<?>` en runtime en vez de romper el build.

---

## Migración (hecha)

- **94 call-sites** en **20 archivos** migrados con reemplazo mecánico (script PowerShell,
  reemplazo literal, sin regex, preservando encoding UTF-8 sin BOM):
  - 37 `spdlog::info(` → `XConsole::Println(`
  - 28 `spdlog::trace(` → `XConsole::Trace(`
  - 17 `spdlog::error(` → `XConsole::PrintError(`
  - 7 `spdlog::warn(` → `XConsole::PrintWarning(`
  - 5 `spdlog::critical(` → `XConsole::PrintError(`
  - 2 `spdlog::set_level(spdlog::level::trace/info)` → `XConsole::SetLevel(XConsole::LogLevel::Trace/Info)`
- `#include <spdlog/spdlog.h>` → `#include "LeirEngine/Core/Log.h"` en cada archivo migrado.
- Los headers públicos **no** incluían spdlog (solo `.cpp`), así que la API pública no cambió.

### Archivos migrados

```
editor/src/main.cpp, editor/src/UI/UIDragFloatInput.cpp
engine/src/Core/CoreApplication.cpp, engine/src/Core/Settings.cpp
engine/src/Physics/PhysicsWorld.cpp
engine/src/Rendering/Image.cpp, Material.cpp, RenderPipeline.cpp, Shader.cpp,
  Texture2D.cpp, VulkanDevice.cpp
engine/src/UI/Font.cpp, UICanvas.cpp, UIFloatInput.cpp, UIRenderer.cpp,
  UITextInput.cpp, UITextArea.cpp
engine/src/UI/Dock/DockManager.cpp, DockSplitter.cpp
examples/PhysicsDemo/main.cpp
```

### CMake

- `dependencies/CMakeLists.txt`: eliminado `FetchContent_Declare(spdlog ...)` y `spdlog` de
  `FetchContent_MakeAvailable(...)`.
- `engine/CMakeLists.txt`: eliminado `spdlog::spdlog` de `target_link_libraries`, agregado
  `src/Core/Log.cpp`.
- Los headers de spdlog pueden quedar como basura en `build/windows-debug/_deps/spdlog*`; se
  pueden borrar (no se usan más).

---

## Consola del editor (implementado — `ConsolePanel`)

Diseño objetivo (estilo Unity) — **todo lo listado ya está implementado** (ver
`TODO_UI_CONSOLE.md` + `TODO_UI_EVENT_FLOOD.md`):

- ✅ Panel dockeable (registrado como panel del `DockManager` como los demás).
- ✅ En `Refresh()` (por frame) lee `XConsole::GetMessages()` y renderiza con `UILabel` de solo lectura
  (detección de mensajes nuevos vía `GetVersion()`, rebuild perezoso por frame — no snapshot cada frame).
- ✅ 3 botones de filtro: **Info / Warning / Error** (toggle por tipo, cada uno con su color:
  Info blanco, Warning amarillo, Error rojo).
- ✅ Colores por nivel en el texto.
- ✅ `Clear` botón → `XConsole::Clear()`.
- ✅ Scrollbar + auto-follow + timestamps (wheel/scroll, se re-sincroniza en rebuild).
- ⏳ Opcional: toggle extra "Show Trace/Debug" para cuando se sube el nivel (requiere
  `SetRetainLevel` en `XConsole`; hoy Trace/Debug se descartan del ring buffer).
- ⏳ Roadmap restante de la consola (ver `TODO_UI_CONSOLE.md`): pooling de labels, wrap de líneas
  largas, pausa/freeze, búsqueda/filtro por texto, doble-click copiar, `CommandLine`.

---

## Verificación

1. `cmake --preset=windows-debug` (reconfigurar tras sacar spdlog).
2. `cmake --build build/windows-debug` → sin errores.
3. Correr el editor: los logs aparecen en la terminal con el nuevo formato
   `[HH:MM:SS.mmm] [level] mensaje`; errores van a stderr.
4. `Select-String spdlog` sobre `src/` → solo debe aparecer en docs (`AGENTS.md`, `TODO.md`, este archivo).

---

## Historial

- **XConsole v1** — implementación propia reemplazando spdlog. `Log.h`/`Log.cpp`, formatter
  runtime, ring buffer 1000, stdout/stderr, migración de 94 call-sites, spdlog eliminado de CMake.
