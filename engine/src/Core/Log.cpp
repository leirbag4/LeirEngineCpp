#include "LeirEngine/Core/Log.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace Leir {

namespace {

std::mutex& GetMutex()
{
    static std::mutex m;
    return m;
}

LogLevel& GetLevelRef()
{
    static LogLevel lvl = LogLevel::Info;
    return lvl;
}

std::vector<LogMessage>& GetMessagesRef()
{
    static std::vector<LogMessage> msgs;
    return msgs;
}

constexpr size_t kMaxMessages = 1000;

const char* LevelName(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error: return "error";
    }
    return "unknown";
}

std::string Timestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const long long ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm.tm_hour << ':'
        << std::setw(2) << tm.tm_min << ':'
        << std::setw(2) << tm.tm_sec << '.'
        << std::setw(3) << ms;
    return oss.str();
}

// Format spec inside a placeholder, e.g. ".2" from {:.2f} or "02" from {:02d}.
struct Spec {
    bool zeroPad = false;
    int width = 0;
    int precision = -1;
    char type = 0; // 'd' or 'f'
};

void ParseSpec(const std::string& s, Spec& out)
{
    size_t i = 0;
    if (i < s.size() && s[i] == '0') {
        out.zeroPad = true;
        ++i;
    }
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        out.width = out.width * 10 + (s[i] - '0');
        ++i;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        out.precision = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            out.precision = out.precision * 10 + (s[i] - '0');
            ++i;
        }
    }
    if (i < s.size())
        out.type = s[i];
}

std::string FormatInt(long long value, const Spec& spec)
{
    std::ostringstream oss;
    if (spec.width > 0)
        oss << (spec.zeroPad ? std::setfill('0') : std::setfill(' ')) << std::setw(spec.width);
    oss << value;
    return oss.str();
}

std::string FormatFloat(double value, const Spec& spec)
{
    std::ostringstream oss;
    if (spec.precision >= 0)
        oss << std::fixed << std::setprecision(spec.precision);
    oss << value;
    return oss.str();
}

std::string FormatArg(const std::any& a, const std::string& specText)
{
    Spec spec;
    ParseSpec(specText, spec);

    if (const auto* p = std::any_cast<bool>(&a))
        return *p ? "true" : "false";

    if (const auto* p = std::any_cast<char>(&a))
        return std::string(1, *p);

    if (const auto* p = std::any_cast<int>(&a))
        return FormatInt(*p, spec);
    if (const auto* p = std::any_cast<unsigned>(&a))
        return FormatInt((long long)*p, spec);
    if (const auto* p = std::any_cast<long>(&a))
        return FormatInt(*p, spec);
    if (const auto* p = std::any_cast<unsigned long>(&a))
        return FormatInt((long long)*p, spec);
    if (const auto* p = std::any_cast<long long>(&a))
        return FormatInt(*p, spec);
    if (const auto* p = std::any_cast<unsigned long long>(&a))
        return FormatInt((long long)*p, spec);

    if (const auto* p = std::any_cast<float>(&a))
        return FormatFloat(*p, spec);
    if (const auto* p = std::any_cast<double>(&a))
        return FormatFloat(*p, spec);

    if (const auto* p = std::any_cast<std::string>(&a))
        return *p;
    if (const auto* p = std::any_cast<const char*>(&a))
        return *p ? std::string(*p) : "(null)";
    if (const auto* p = std::any_cast<char*>(&a))
        return *p ? std::string(*p) : "(null)";

    return "<?>";
}

} // namespace

std::string XConsole::Format(const std::string& fmt, std::vector<std::any>&& args)
{
    std::string out;
    out.reserve(fmt.size() + 32);
    size_t argIndex = 0;
    size_t i = 0;
    while (i < fmt.size()) {
        const char c = fmt[i];
        if (c == '{' && i + 1 < fmt.size() && fmt[i + 1] == '{') {
            out += '{';
            i += 2;
            continue;
        }
        if (c == '}' && i + 1 < fmt.size() && fmt[i + 1] == '}') {
            out += '}';
            i += 2;
            continue;
        }
        if (c == '{') {
            const size_t close = fmt.find('}', i + 1);
            if (close != std::string::npos) {
                const std::string specText = fmt.substr(i + 1, close - i - 1);
                if (argIndex < args.size())
                    out += FormatArg(args[argIndex], specText);
                else
                    out += "{?}";
                ++argIndex;
                i = close + 1;
                continue;
            }
        }
        out += c;
        ++i;
    }
    return out;
}

void XConsole::Log(LogLevel level, std::string&& text)
{
    std::lock_guard<std::mutex> lock(GetMutex());

    if (static_cast<int>(level) < static_cast<int>(GetLevelRef()))
        return;

    const std::string line = "[" + Timestamp() + "] [" + LevelName(level) + "] " + text + "\n";
    if (level == LogLevel::Error) {
        std::fputs(line.c_str(), stderr);
        std::fflush(stderr);
    } else {
        std::fputs(line.c_str(), stdout);
        std::fflush(stdout);
    }

    auto& msgs = GetMessagesRef();
    if (msgs.size() >= kMaxMessages)
        msgs.erase(msgs.begin());
    msgs.push_back({level, std::move(text)});
}

void XConsole::SetLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(GetMutex());
    GetLevelRef() = level;
}

LogLevel XConsole::GetLevel()
{
    std::lock_guard<std::mutex> lock(GetMutex());
    return GetLevelRef();
}

std::vector<LogMessage> XConsole::GetMessages()
{
    std::lock_guard<std::mutex> lock(GetMutex());
    return GetMessagesRef();
}

void XConsole::Clear()
{
    std::lock_guard<std::mutex> lock(GetMutex());
    GetMessagesRef().clear();
}

} // namespace Leir
