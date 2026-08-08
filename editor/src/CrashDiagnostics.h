#pragma once

// Crash/failure diagnostics for the editor. The implementation is fully
// platform-specific and lives in the .cpp behind #ifdef branches; this header
// is the single portable entry point the rest of the code (and main()) calls.
// See CRASH_DIAGNOSTICS.md for how the system works and how to extend it to
// macOS/Linux.
namespace CrashDiagnostics {

// Installs the platform hooks once at startup:
//   Windows: terminate handler, MSVC CRT invalid_parameter handler, debug CRT
//            alloc hook (catches >512MB allocations before bad_alloc), and a
//            DbgHelp-symbolized stack walk that writes to a crash log.
//   macOS/Linux: currently a terminate handler only; extend per platform.
void Init();

} // namespace CrashDiagnostics