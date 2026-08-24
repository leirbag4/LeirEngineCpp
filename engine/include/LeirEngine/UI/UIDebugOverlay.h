#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIRenderer.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Leir {

class Font;
class UICanvas;
class UIPanel;
class UILabel;
class UIButton;

// Floating "Stats" overlay (FPS / frame time / draw calls / memory / input /
// hover / last event). Collapsible: a draggable title bar ("Stats" + a -/+ toggle)
// minimizes it to a small bar pinned to the viewport's bottom-right corner.
// Position and maximized/minimized state persist via LeirSettings (debug.stats).
class LEIR_API UIDebugOverlay {
public:
    UIDebugOverlay(Font* font, UICanvas* canvas);
    ~UIDebugOverlay();

    void SetFont(Font* font);
    void Update(float deltaTime);
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    // Provider for last-frame render stats (set by the app so the overlay
    // stays decoupled from the UIRenderer's lifetime).
    using RenderStatsProvider = std::function<UIRenderStats()>;
    void SetRenderStatsProvider(RenderStatsProvider provider) { m_StatsProvider = std::move(provider); }

    // Provider for the 3D viewport rect (canvas coords). The minimized panel is
    // pinned to the viewport's bottom-right corner, so the app supplies it.
    using ViewportRectProvider = std::function<Vector4()>;
    void SetViewportRectProvider(ViewportRectProvider provider) { m_ViewportRectProvider = std::move(provider); }

    // Title-bar interaction (the internal OverlayTitleBar forwards these).
    void BeginTitleDrag(UIPanel* titleBar, const Vector2& pos);
    void TitleDragTo(const Vector2& pos);
    void EndTitleDrag(const Vector2& pos);
    void ToggleMinimized();
    bool IsMinimized() const { return m_Minimized; }

private:
    void CreatePanel(Font* font);
    void ApplyMaximizedLayout();
    void ApplyMinimizedLayout();
    void RestoreState();
    void SaveState();

    UICanvas* m_Canvas = nullptr;

    UIPanel* m_Panel = nullptr;
    UIPanel* m_HeaderRow = nullptr;   // draggable title bar
    UILabel* m_TitleLabel = nullptr;
    UIButton* m_MinMaxButton = nullptr;
    UIPanel* m_ContentPanel = nullptr; // holds the stat labels (hidden when minimized)
    class UILabel* m_FpsLabel = nullptr;
    class UILabel* m_FrameTimeLabel = nullptr;
    class UILabel* m_DrawCallsLabel = nullptr;
    class UILabel* m_MemoryLabel = nullptr;
    class UILabel* m_MouseLabel = nullptr;
    class UILabel* m_ButtonsLabel = nullptr;
    class UILabel* m_KeysLabel = nullptr;
    class UILabel* m_HoverLabel = nullptr;
    class UILabel* m_LastEventLabel = nullptr;

    RenderStatsProvider m_StatsProvider;
    ViewportRectProvider m_ViewportRectProvider;

    bool m_Active = true;

    // Maximized layout (x, y, w, h in canvas coords); position persists.
    Vector4 m_MaximizedRect = {274.0f, 10.0f, 316.0f, 330.0f};
    bool m_Minimized = false;

    // Title-bar drag
    bool m_Dragging = false;
    Vector2 m_DragStart = {0.0f, 0.0f};
    Vector2 m_StartOffset = {0.0f, 0.0f};

    // FPS smoothing
    float m_FpsAccum = 0.0f;
    int m_FrameCount = 0;
    float m_CurrentFps = 0.0f;

    // Per-frame render stat averages (share the FPS smoothing window): the
    // panel shows the current value plus the average over the last window,
    // Unity-stats-style. Nothing here is cumulative.
    std::uint64_t m_DrawCallsAccum = 0;
    std::uint64_t m_QuadsAccum = 0;
    uint32_t m_AvgDrawCalls = 0;
    uint32_t m_AvgQuads = 0;

    // Event tracking (stores the hovered element's NAME, not a raw pointer:
    // dock operations can delete the previously-hovered element between
    // frames, so retaining the pointer would be a use-after-free).
    std::string m_LastHoveredName;
    std::string m_LastEvent;
    int m_LastEventFrames = 0;
};

} // namespace Leir
