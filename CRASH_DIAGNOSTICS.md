# Crash Diagnostics System

Crash/failure reporting for the editor. A single portable entry point
(`CrashDiagnostics::Init()`) that installs platform-specific hooks. The system
was extracted from an inline block in `editor/src/main.cpp` so it is isolated
in its own files with exactly **one** `#ifdef` per platform, ready to expand to
macOS/Linux without touching app code.

## Files

```
editor/src/CrashDiagnostics.h     # public API: CrashDiagnostics::Init()
editor/src/CrashDiagnostics.cpp   # implementation, per-platform #ifdef branches
editor/CMakeLists.txt             # adds CrashDiagnostics.cpp + links dbghelp (WIN32)
```

## API

```cpp
namespace CrashDiagnostics {
void Init();   // call once at editor startup (main.cpp)
}
```

`Init()` is a no-op on unsupported setups beyond a terminate handler. App code
never touches platform details.

## What each platform installs

### Windows (`_WIN32`) — fully wired
- **terminate handler** (`std::set_terminate`): dumps the active exception's
  type + `what()` to the crash log, then `abort()`.
- **CRT invalid-parameter handler** (`_set_invalid_parameter_handler`): catches
  bad CRT calls (e.g. a bad `fprintf`/`fopen` arg) and logs it before `abort()`.
- **Debug CRT allocation hook** (`_CrtSetAllocHook`): process-wide (engine DLL +
  editor share the debug heap), logs any allocation >512 MB **before**
  `bad_alloc` and runs a symbolized DbgHelp stack walk to locate the call site.
  (Exe-side `operator new` overrides can't see engine-DLL allocations, which is
  why the CRT hook is used instead of global `operator new`.)
- **Stack walk** (`DbgHelp`: `SymInitialize`/`SymGetModuleInfo64`/`SymFromAddr`):
  prints `module!function+0xoffset` per frame.

### macOS (`__APPLE__`) — skeleton
- terminate handler only.
- TODO: `std::signal` handlers (`SIGSEGV`/`SIGABRT`) + `backtrace()`/execinfo
  dump to `~/Library/Logs/LeirEngine/`.

### Linux — skeleton
- terminate handler only.
- TODO: `std::signal` handlers (`SIGSEGV`/`SIGABRT`) + `backtrace()`/
  `backtrace_symbols_fd(2)` dump to `/tmp/leir_crash_diagnostics.log`.

## Crash log location

- Windows: `C:/Users/gabri/AppData/Local/Temp/opencode/crash_diagnostics.log`
- macOS/Linux: `/tmp/leir_crash_diagnostics.log` (default; adjust per platform)

The log is append-only; each entry is one line. A `[bad_alloc]`/`[terminate]`
entry followed by `#N module!symbol` lines is a symbolized stack.

## Why DbgHelp is linked via CMake (Windows)

`CrashDiagnostics.cpp` still carries `#pragma comment(lib, "dbghelp.lib")` for
MSVC, but **MinGW GCC ignores the pragma** — that left the GitHub Actions
`windows-latest` runner (which uses MinGW via the `windows-ci-debug` Ninja
preset) with `undefined reference to __imp_SymInitialize` etc. The CMake
`if(WIN32) target_link_libraries(LeirEngineEditor PRIVATE dbghelp)` covers both
MSVC and MinGW. Keep the CMake link as the source of truth.

## Platform guard convention

Exactly one guard branch per platform file:

```cpp
#if defined(_WIN32)
    ...
#elif defined(__APPLE__)
    ...
#else
    ...
#endif
```

Keep app code (`main.cpp`) free of any `_WIN32`/`__APPLE__` — only
`CrashDiagnostics::Init()` is called there.

## Extending to a new platform

1. Add the branch in `CrashDiagnostics.cpp` (`#elif`/`#else`).
2. Install handlers inside `Init()`.
3. If the platform needs an extra library, link it conditionally in
   `editor/CMakeLists.txt` (e.g. `if(APPLE) ... endif()`), mirroring the
   `dbghelp` pattern above.
4. Update this file.
