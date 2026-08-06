#pragma once
#include "LeirEngine/Core/Export.h"
#include <any>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Leir {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
};

struct LogMessage {
    LogLevel level;
    std::string text;
    std::string time; // "HH:MM:SS.mmm" local wall clock
};

// Thread-safe console logger. Own implementation, no external logging dependency.
//
// Levels: Trace < Debug < Info < Warning < Error. Messages below the current SetLevel()
// are discarded entirely (Trace/Debug are "silent by default"). Emitted messages go to
// stdout (Info/Warning/Debug/Trace) and stderr (Error). Only Info/Warning/Error are
// retained in the internal ring buffer (GetMessages) that feeds the editor Console
// panel — Trace/Debug are debug-only diagnostics and would evict useful messages.
//
// Formatting supports `{}` and specs used across the codebase: `{:.Nf}`, `{:Nd}`, `{:0Nd}`.
class LEIR_API XConsole {
public:
    template <typename... Args>
    static void Println(const char* fmt, Args&&... args)
    {
        Log(LogLevel::Info, Format(fmt, argv(std::forward<Args>(args)...)));
    }
    template <typename... Args>
    static void Println(const std::string& fmt, Args&&... args)
    {
        Log(LogLevel::Info, Format(fmt, argv(std::forward<Args>(args)...)));
    }

    template <typename... Args>
    static void PrintWarning(const char* fmt, Args&&... args)
    {
        Log(LogLevel::Warning, Format(fmt, argv(std::forward<Args>(args)...)));
    }
    template <typename... Args>
    static void PrintWarning(const std::string& fmt, Args&&... args)
    {
        Log(LogLevel::Warning, Format(fmt, argv(std::forward<Args>(args)...)));
    }

    template <typename... Args>
    static void PrintError(const char* fmt, Args&&... args)
    {
        Log(LogLevel::Error, Format(fmt, argv(std::forward<Args>(args)...)));
    }
    template <typename... Args>
    static void PrintError(const std::string& fmt, Args&&... args)
    {
        Log(LogLevel::Error, Format(fmt, argv(std::forward<Args>(args)...)));
    }

    template <typename... Args>
    static void Trace(const char* fmt, Args&&... args)
    {
        Log(LogLevel::Trace, Format(fmt, argv(std::forward<Args>(args)...)));
    }
    template <typename... Args>
    static void Trace(const std::string& fmt, Args&&... args)
    {
        Log(LogLevel::Trace, Format(fmt, argv(std::forward<Args>(args)...)));
    }

    template <typename... Args>
    static void Debug(const char* fmt, Args&&... args)
    {
        Log(LogLevel::Debug, Format(fmt, argv(std::forward<Args>(args)...)));
    }
    template <typename... Args>
    static void Debug(const std::string& fmt, Args&&... args)
    {
        Log(LogLevel::Debug, Format(fmt, argv(std::forward<Args>(args)...)));
    }

    static void SetLevel(LogLevel level);
    static LogLevel GetLevel();

    // Snapshot of the internal ring buffer (for the editor Console panel).
    static std::vector<LogMessage> GetMessages();
    // Monotonic counter bumped on every emitted message and on Clear(). Lets a
    // UI panel detect new messages without snapshotting the buffer each frame.
    static uint64_t GetVersion();
    static void Clear();

private:
    static void Log(LogLevel level, std::string&& text);

    static std::string Format(const std::string& fmt, std::vector<std::any>&& args);

    static std::vector<std::any> argv() { return {}; }

    template <typename... Args>
    static std::vector<std::any> argv(Args&&... args)
    {
        std::vector<std::any> v;
        v.reserve(sizeof...(args));
        (v.emplace_back(std::forward<Args>(args)), ...);
        return v;
    }
};

} // namespace Leir
