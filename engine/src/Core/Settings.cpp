#include "LeirEngine/Core/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace Leir {

namespace {
constexpr const char* kSettingsFileName = "settings.json";
}

LeirSettings& LeirSettings::Get()
{
    static LeirSettings instance;
    return instance;
}

std::string LeirSettings::GetDefaultPath() const
{
    namespace fs = std::filesystem;

    fs::path configDir;
#ifdef _WIN32
    if (const char* ap = std::getenv("APPDATA"))
        configDir = fs::path(ap) / "LeirEngine";
    else
        configDir = fs::current_path() / "LeirEngine";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    configDir = fs::path(home ? home : ".") / "Library" / "Application Support" / "LeirEngine";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        configDir = fs::path(xdg) / "LeirEngine";
    else {
        const char* home = std::getenv("HOME");
        configDir = fs::path(home ? home : ".") / ".config" / "LeirEngine";
    }
#endif
    return (configDir / kSettingsFileName).string();
}

bool LeirSettings::Load(const std::string& path)
{
    m_Path = path.empty() ? GetDefaultPath() : path;

    std::ifstream f(m_Path);
    if (!f.is_open()) {
        spdlog::warn("Settings file '{}' not found, creating with defaults", m_Path);
        SetDefaults();
        Save();
        return true;
    }

    try {
        nlohmann::json j;
        f >> j;

        window.width = j.value("window", nlohmann::json::object()).value("width", 1280);
        window.height = j.value("window", nlohmann::json::object()).value("height", 720);
        window.pos_x = j.value("window", nlohmann::json::object()).value("pos_x", INT_MIN);
        window.pos_y = j.value("window", nlohmann::json::object()).value("pos_y", INT_MIN);
        window.fullscreen = j.value("window", nlohmann::json::object()).value("fullscreen", false);
        window.maximized = j.value("window", nlohmann::json::object()).value("maximized", false);
        window.vsync = j.value("window", nlohmann::json::object()).value("vsync", true);

        debug.ui_outlines = j.value("debug", nlohmann::json::object()).value("ui_outlines", false);
        debug.show_overlay = j.value("debug", nlohmann::json::object()).value("show_overlay", true);
        debug.show_glyph_quads = j.value("debug", nlohmann::json::object()).value("show_glyph_quads", false);
        debug.ui_event_log = j.value("debug", nlohmann::json::object()).value("ui_event_log", false);

        layout.hierarchy_width = j.value("layout", nlohmann::json::object()).value("hierarchy_width", 264.0f);
        layout.inspector_width = j.value("layout", nlohmann::json::object()).value("inspector_width", 290.0f);

        spdlog::info("Settings loaded from '{}'", m_Path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse settings file '{}': {}", m_Path, e.what());
        SetDefaults();
        return false;
    }
}

bool LeirSettings::Save()
{
    if (m_Path.empty()) return false;

    nlohmann::json j;
    j["window"]["width"] = window.width;
    j["window"]["height"] = window.height;
    j["window"]["pos_x"] = window.pos_x;
    j["window"]["pos_y"] = window.pos_y;
    j["window"]["fullscreen"] = window.fullscreen;
    j["window"]["maximized"] = window.maximized;
    j["window"]["vsync"] = window.vsync;
    j["debug"]["ui_outlines"] = debug.ui_outlines;
    j["debug"]["show_overlay"] = debug.show_overlay;
    j["debug"]["show_glyph_quads"] = debug.show_glyph_quads;
    j["debug"]["ui_event_log"] = debug.ui_event_log;
    j["layout"]["hierarchy_width"] = layout.hierarchy_width;
    j["layout"]["inspector_width"] = layout.inspector_width;

    try {
        namespace fs = std::filesystem;
        fs::path dir = fs::path(m_Path).parent_path();
        if (!dir.empty() && !fs::exists(dir))
            fs::create_directories(dir);

        std::ofstream f(m_Path);
        if (!f.is_open()) {
            spdlog::error("Failed to write settings file '{}'", m_Path);
            return false;
        }
        f << j.dump(4);
        spdlog::info("Settings saved to '{}'", m_Path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings: {}", e.what());
        return false;
    }
}

void LeirSettings::SetLayoutWidths(float hierarchyWidth, float inspectorWidth)
{
    layout.hierarchy_width = hierarchyWidth;
    layout.inspector_width = inspectorWidth;
}

void LeirSettings::SetDefaults()
{
    window.width = 1280;
    window.height = 720;
    window.pos_x = INT_MIN;
    window.pos_y = INT_MIN;
    window.fullscreen = false;
    window.maximized = false;
    window.vsync = true;
    debug.ui_outlines = false;
    debug.show_overlay = true;
    debug.show_glyph_quads = false;
    debug.ui_event_log = false;
    layout.hierarchy_width = 300.0f;
    layout.inspector_width = 300.0f;
}

} // namespace Leir
