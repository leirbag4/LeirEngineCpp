#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIRenderer.h"
#include <string>
#include <vector>
#include <functional>

namespace Leir {

class Font;
class UICanvas;

class LEIR_API UIDebugOverlay {
public:
    UIDebugOverlay(Font* font, UICanvas* canvas);
    ~UIDebugOverlay();

    void Update(float deltaTime);
    void SetActive(bool active);
    bool IsActive() const { return m_Active; }

    // Provider for last-frame render stats (set by the app so the overlay
    // stays decoupled from the UIRenderer's lifetime).
    using RenderStatsProvider = std::function<UIRenderStats()>;
    void SetRenderStatsProvider(RenderStatsProvider provider) { m_StatsProvider = std::move(provider); }

private:
    void CreatePanel(Font* font);

    UICanvas* m_Canvas = nullptr;

    class UIPanel* m_Panel = nullptr;
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

    bool m_Active = true;

    // FPS smoothing
    float m_FpsAccum = 0.0f;
    int m_FrameCount = 0;
    float m_CurrentFps = 0.0f;

    // Event tracking (stores the hovered element's NAME, not a raw pointer:
    // dock operations can delete the previously-hovered element between
    // frames, so retaining the pointer would be a use-after-free).
    std::string m_LastHoveredName;
    std::string m_LastEvent;
    int m_LastEventFrames = 0;
};

} // namespace Leir
