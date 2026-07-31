#pragma once
#include "LeirEngine/Core/Export.h"
#include <string>

namespace Leir {

class LEIR_API LeirSettings {
public:
    static LeirSettings& Get();

    bool Load(const std::string& path = "");
    bool Save();
    void SetLayoutWidths(float hierarchyWidth, float inspectorWidth);

    struct {
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool vsync = true;
    } window;

    struct {
        bool ui_outlines = false;
        bool show_overlay = true;
        bool show_glyph_quads = false;
        bool ui_event_log = false;
    } debug;

    struct {
        float hierarchy_width = 300.0f;
        float inspector_width = 300.0f;
    } layout;

    // Resolved platform config path (<config>/LeirEngine/settings.json)
    const std::string& GetPath() const { return m_Path; }

private:
    LeirSettings() = default;

    void SetDefaults();
    std::string GetDefaultPath() const;
    std::string m_Path;
};

} // namespace Leir
