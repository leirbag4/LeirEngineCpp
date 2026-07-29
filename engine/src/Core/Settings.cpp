#include "LeirEngine/Core/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace Leir {

LeirSettings& LeirSettings::Get()
{
    static LeirSettings instance;
    return instance;
}

bool LeirSettings::Load(const std::string& path)
{
    m_Path = path;
    std::ifstream f(path);
    if (!f.is_open()) {
        spdlog::warn("Settings file '{}' not found, creating with defaults", path);
        SetDefaults();
        Save();
        return true;
    }

    try {
        nlohmann::json j;
        f >> j;

        window.width = j.value("window", nlohmann::json::object()).value("width", 1280);
        window.height = j.value("window", nlohmann::json::object()).value("height", 720);
        window.fullscreen = j.value("window", nlohmann::json::object()).value("fullscreen", false);
        window.vsync = j.value("window", nlohmann::json::object()).value("vsync", true);

        debug.ui_outlines = j.value("debug", nlohmann::json::object()).value("ui_outlines", false);

        spdlog::info("Settings loaded from '{}'", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse settings file '{}': {}", path, e.what());
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
    j["window"]["fullscreen"] = window.fullscreen;
    j["window"]["vsync"] = window.vsync;
    j["debug"]["ui_outlines"] = debug.ui_outlines;

    try {
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

void LeirSettings::SetDefaults()
{
    window.width = 1280;
    window.height = 720;
    window.fullscreen = false;
    window.vsync = true;
    debug.ui_outlines = false;
}

} // namespace Leir
