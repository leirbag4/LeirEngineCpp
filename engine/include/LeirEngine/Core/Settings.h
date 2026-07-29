#pragma once
#include "LeirEngine/Core/Export.h"
#include <string>

namespace Leir {

class LEIR_API LeirSettings {
public:
    static LeirSettings& Get();

    bool Load(const std::string& path = "leir_settings.json");
    bool Save();

    struct {
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool vsync = true;
    } window;

    struct {
        bool ui_outlines = false;
        bool show_overlay = true;
    } debug;

private:
    LeirSettings() = default;

    void SetDefaults();
    std::string m_Path;
};

} // namespace Leir
