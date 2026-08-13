#include "CrashDiagnostics.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <typeinfo>

// ---- Platform-specific crash/failure infrastructure -------------------------
// Only _WIN32 is wired up today. macOS/Linux branches are skeletons with TODO
// comments so the system is ready to expand per platform later (see
// CRASH_DIAGNOSTICS.md).

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <crtdbg.h>
#include <dbghelp.h>
// NOTE: dbghelp is ALSO linked from CMake (editor/CMakeLists.txt, if(WIN32)).
// The pragma below is MSVC-only; the CMake link covers MinGW (CI).
#pragma comment(lib, "dbghelp.lib")
#endif // _WIN32

namespace CrashDiagnostics {

namespace {

// Log file the diagnostics append to. Debug builds default to a temp file in
// the project's build dir so it's easy to find while developing.
#if defined(_WIN32)
const char* gDiagLogPath = "C:/Users/gabri/AppData/Local/Temp/opencode/crash_diagnostics.log";
#else
const char* gDiagLogPath = "/tmp/leir_crash_diagnostics.log";
#endif

void LogDiag(const char* msg)
{
#if defined(_WIN32)
    FILE* f = nullptr;
    if (fopen_s(&f, gDiagLogPath, "a") == 0) {
#else
    FILE* f = fopen(gDiagLogPath, "a");
    if (f) {
#endif
        fputs(msg, f);
        fputc('\n', f);
        fclose(f);
    }
}

void LogDiagf(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
#ifdef _WIN32
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
#else
    vsnprintf(buf, sizeof(buf), fmt, args);
#endif
    va_end(args);
    LogDiag(buf);
}

// std::terminate handler: dump the exception type + what() before aborting.
void OnTerminate()
{
    LogDiag("[terminate] std::terminate called");
    if (std::current_exception()) {
        try {
            std::rethrow_exception(std::current_exception());
        } catch (const std::exception& e) {
            LogDiagf("[terminate] exception type='%s' what='%s'",
                typeid(e).name(), e.what());
        } catch (...) {
            LogDiag("[terminate] exception of unknown type");
        }
    } else {
        LogDiag("[terminate] no active exception");
    }
    abort();
}

#if defined(_WIN32)

// MSVC/CRT "invalid parameter" handler (e.g. bad fprintf/fopen args).
void OnInvalidParam(const wchar_t* expression, const wchar_t* function,
                    const wchar_t* file, unsigned int line, uintptr_t /*reserved*/)
{
    LogDiagf(
        "[invalid_parameter] expression='%ls' function='%ls' file='%ls' line=%u",
        expression ? expression : L"(null)",
        function ? function : L"(null)",
        file ? file : L"(null)", line);
    abort();
}

// Symbolized stack walk of the whole process via DbgHelp. Logs the current
// stack (the call chain that reached this handler). The "where" header line
// is caller-supplied so it works for bad_alloc AND the SEH handler.
void LogStackWalk(const char* header)
{
    HANDLE proc = GetCurrentProcess();
    static bool symInit = false;
    if (!symInit) {
        symInit = SymInitialize(proc,
            "C:\\projects\\leir_engine\\build\\windows-debug\\engine\\Debug;"
            "C:\\projects\\leir_engine\\build\\windows-debug\\editor\\Debug",
            TRUE) != FALSE;
    }

    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);

    LogDiagf("%s frames=%u", header, (unsigned)frames);

    for (USHORT i = 0; i < frames; ++i) {
        DWORD64 addr = (DWORD64)stack[i];
        DWORD64 disp = 0;
        char symName[256] = "?";
        char modName[64] = "?";
        IMAGEHLP_MODULE64 mi{};
        mi.SizeOfStruct = sizeof(mi);
        if (SymGetModuleInfo64(proc, addr, &mi))
            snprintf(modName, sizeof(modName), "%s", mi.ModuleName);
        char symBuf[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* si = reinterpret_cast<SYMBOL_INFO*>(symBuf);
        si->SizeOfStruct = sizeof(SYMBOL_INFO);
        si->MaxNameLen = 256;
        if (SymFromAddr(proc, addr, &disp, si))
            snprintf(symName, sizeof(symName), "%s+0x%llx", si->Name, disp);
        LogDiagf("   #%u %s!%s", (unsigned)i, modName, symName);
    }
}

// SEH (access violation / heap / any native exception) handler. set_terminate
// only covers C++ exceptions; a plain AV or a CRT-raised code would otherwise
// fall straight through to WER with no log (that is exactly how the old
// 5s-close double-free and the D3D12 teardown crash went undiagnosed). Log the
// fault + a stack walk, then terminate the process immediately (no WER dialog,
// no seconds-long dump collection).
LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* ep)
{
    char where[256];
    snprintf(where, sizeof(where), "[SEH] unhandled exception code=0x%08X at 0x%p",
        (unsigned)(ep ? ep->ExceptionRecord->ExceptionCode : 0),
        ep ? ep->ExceptionRecord->ExceptionAddress : nullptr);
    LogStackWalk(where);
    return EXCEPTION_EXECUTE_HANDLER; // handled here: log + clean exit, no WER
}

// MSVC CRT debug allocation hook: catches (>512MB) allocations process-wide
// (engine DLL + editor share the debug heap) and logs a symbolized stack walk
// BEFORE bad_alloc aborts, pointing at the call site that asks for the huge
// buffer. (Exe-side operator new overrides can't see engine-DLL allocations,
// which is why the CRT hook is used instead.)
int MyAllocHook(int allocType, void* /*userData*/, size_t size, int blockType,
                long requestNumber, const unsigned char* filename, int lineNumber)
{
    static bool inHook = false;
    if (size > (size_t)512 * 1024 * 1024) {
        LogDiagf(
            "[alloc-hook] size=%zu allocType=%d blockType=%d request=%ld file='%s' line=%d",
            size, allocType, blockType, requestNumber,
            filename ? reinterpret_cast<const char*>(filename) : "(null)", lineNumber);
        if (!inHook) {
            inHook = true;
            LogStackWalk("[bad_alloc]");
            inHook = false;
        }
    }
    return TRUE;
}
#endif // _WIN32

} // namespace

void Init()
{
#if defined(_WIN32)
    std::set_terminate(&OnTerminate);
    _set_invalid_parameter_handler(&OnInvalidParam);
    _CrtSetAllocHook(&MyAllocHook);
    SetUnhandledExceptionFilter(&OnUnhandledException);
#elif defined(__APPLE__)
    // TODO(macOS): register std::signal handlers (SIGSEGV/SIGABRT) + a
    // backtrace()/execinfo stack dump to ~/Library/Logs/LeirEngine/. The
    // terminate handler below already covers uncaught exceptions.
    std::set_terminate(&OnTerminate);
#else
    // TODO(Linux): register std::signal handlers (SIGSEGV/SIGABRT) + a
    // backtrace()/backtrace_symbols_fd(2) dump to /tmp/leir_crash_diagnostics.log.
    std::set_terminate(&OnTerminate);
#endif
}

} // namespace CrashDiagnostics