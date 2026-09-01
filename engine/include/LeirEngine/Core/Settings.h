#pragma once
#include "LeirEngine/Core/Export.h"
#include <climits>
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
        int pos_x = INT_MIN;   // INT_MIN = unset (window centered on first run)
        int pos_y = INT_MIN;
        bool fullscreen = false;
        bool maximized = false;
        bool vsync = true;
        bool hidpi = true;  // use system DPI scale (false = fixed/1x UI)
        bool window_shadow = true;  // drop shadow behind internal/external windows
    } window;

    struct {
        bool ui_outlines = false;
        bool show_overlay = true;
        bool show_glyph_quads = false;
        bool ui_event_log = false;
        // Stats overlay (UIDebugOverlay): persisted position + collapsed state.
        struct {
            int pos_x = INT_MIN;   // INT_MIN = unset (default top-left placement)
            int pos_y = INT_MIN;
            bool minimized = false;
        } stats;
    } debug;

    struct {
        // Graphics API backend: "vulkan" or "d3d12" (empty = compile-time default).
        std::string backend = "vulkan";
    } graphics;

    struct {
        float hierarchy_width = 300.0f;
        float inspector_width = 300.0f;
    } layout;

    struct {
        std::string layout;   // serialized dock tree (JSON); empty = default
    } dock;

    // Resolved platform config path (<config>/LeirEngine/settings.json)
    const std::string& GetPath() const { return m_Path; }

private:
    LeirSettings() = default;

    void SetDefaults();
    std::string GetDefaultPath() const;
    std::string m_Path;
};

} // namespace Leir
