#pragma once

/**
 * @file UIDebugOverlay.h
 * @brief Floating stats overlay (FPS, frame time, draw calls, input, hover, last event).
 * @ingroup UI
 */

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

/**
 * @brief Floating stats overlay: FPS, frame time, draw calls, input, hover and last event.
 * @ingroup UI
 * @details Collapsible draggable title bar ("Stats" + -/+ toggle) minimizes to a
 *  bar pinned to the viewport's bottom-right corner. Position and state persist
 *  via LeirSettings.
 */
class LEIR_API UIDebugOverlay {
public:
    /**
     * @brief Constructs the overlay.
     * @param[in] font Font for labels.
     * @param[in] canvas Owning canvas (for hover queries).
     */
    UIDebugOverlay(Font* font, UICanvas* canvas);

    /**
     * @brief Destroys the overlay.
     */
    ~UIDebugOverlay();

    /**
     * @brief Sets the font for labels.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font);

    /**
     * @brief Updates stats and labels.
     * @param[in] deltaTime Frame delta time.
     */
    void Update(float deltaTime);

    /**
     * @brief Sets active state (visible or hidden).
     * @param[in] active True to show.
     */
    void SetActive(bool active);

    /**
     * @brief Returns active state.
     * @return True if active.
     */
    bool IsActive() const { return m_Active; }

    /**
     * @brief Provider for last-frame render stats.
     */
    using RenderStatsProvider = std::function<UIRenderStats()>;

    /**
     * @brief Sets the render stats provider.
     * @param[in] provider Function returning UIRenderStats for the last frame.
     */
    void SetRenderStatsProvider(RenderStatsProvider provider) { m_StatsProvider = std::move(provider); }

    /**
     * @brief Provider for the 3D viewport rect (canvas coords).
     */
    using ViewportRectProvider = std::function<Vector4()>;

    /**
     * @brief Sets the viewport rect provider (for minimized pinning).
     * @param[in] provider Function returning viewport rect (x,y,w,h).
     */
    void SetViewportRectProvider(ViewportRectProvider provider) { m_ViewportRectProvider = std::move(provider); }

    /**
     * @brief Begins title bar drag.
     * @param[in] titleBar Title bar panel.
     * @param[in] pos Pointer position.
     */
    void BeginTitleDrag(UIPanel* titleBar, const Vector2& pos);

    /**
     * @brief Drags title bar to a position.
     * @param[in] pos Pointer position.
     */
    void TitleDragTo(const Vector2& pos);

    /**
     * @brief Ends title bar drag.
     * @param[in] pos Pointer position.
     */
    void EndTitleDrag(const Vector2& pos);

    /**
     * @brief Toggles minimized state.
     */
    void ToggleMinimized();

    /**
     * @brief Returns whether minimized.
     * @return True if minimized.
     */
    bool IsMinimized() const { return m_Minimized; }

private:
    void CreatePanel(Font* font);
    void ApplyMaximizedLayout();
    void ApplyMinimizedLayout();
    void RestoreState();
    void SaveState();

    UICanvas* m_Canvas = nullptr;                       ///< Owning canvas.

    UIPanel* m_Panel = nullptr;                         ///< Root panel (owned by canvas).
    UIPanel* m_HeaderRow = nullptr;                     ///< Draggable title bar.
    UILabel* m_TitleLabel = nullptr;                    ///< Title label.
    UIButton* m_MinMaxButton = nullptr;                 ///< Min/max toggle button.
    UIPanel* m_ContentPanel = nullptr;                  ///< Content panel (hidden when minimized).
    class UILabel* m_FpsLabel = nullptr;                ///< FPS label.
    class UILabel* m_FrameTimeLabel = nullptr;          ///< Frame time label.
    class UILabel* m_DrawCallsLabel = nullptr;          ///< Draw calls label.
    class UILabel* m_MemoryLabel = nullptr;             ///< Memory label.
    class UILabel* m_MouseLabel = nullptr;              ///< Mouse label.
    class UILabel* m_ButtonsLabel = nullptr;            ///< Buttons label.
    class UILabel* m_KeysLabel = nullptr;               ///< Keys label.
    class UILabel* m_HoverLabel = nullptr;              ///< Hover label.
    class UILabel* m_LastEventLabel = nullptr;          ///< Last event label.

    RenderStatsProvider m_StatsProvider;                ///< Render stats provider.
    ViewportRectProvider m_ViewportRectProvider;        ///< Viewport rect provider.

    bool m_Active = true;                               ///< Active flag.

    Vector4 m_MaximizedRect = {274.0f, 10.0f, 316.0f, 330.0f}; ///< Maximized rect.
    bool m_Minimized = false;                           ///< Minimized flag.

    bool m_Dragging = false;                            ///< Dragging flag.
    Vector2 m_DragStart = {0.0f, 0.0f};                  ///< Drag start position.
    Vector2 m_StartOffset = {0.0f, 0.0f};                ///< Start offset.

    float m_FpsAccum = 0.0f;                            ///< FPS accum for smoothing.
    int m_FrameCount = 0;                               ///< Frame count for smoothing.
    float m_CurrentFps = 0.0f;                          ///< Current FPS.

    std::uint64_t m_DrawCallsAccum = 0;                 ///< Draw calls accum.
    std::uint64_t m_QuadsAccum = 0;                     ///< Quads accum.
    uint32_t m_AvgDrawCalls = 0;                        ///< Average draw calls.
    uint32_t m_AvgQuads = 0;                            ///< Average quads.

    std::string m_LastHoveredName;                      ///< Last hovered element name.
    std::string m_LastEvent;                            ///< Last event description.
    int m_LastEventFrames = 0;                          ///< Frames since last event.
};

} // namespace Leir
