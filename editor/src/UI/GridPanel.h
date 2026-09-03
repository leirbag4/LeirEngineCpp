#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/Math/Vector2.h>
#include <LeirEngine/Math/Vector4.h>
#include <functional>
#include "UIDragFloatInput.h"

// Dockable "Grid" panel: all live knobs for the editor ground grid LOD.
//
// A "Manual: ON/OFF" toggle controls whether the inputs drive the grid:
//   - ON  (manual): the inputs are editable and influence the grid directly.
//   - OFF (auto):   the inputs are greyed out / read-only and do NOT influence
//                   the grid; the grid uses the defaults for cell-fade/width/
//                   density and an AUTO horizon fade computed from the camera
//                   height (EditorGrid) that is written back into the greyed
//                   inputs every frame so you can see what auto chose.
// Switching back to manual restores the last manual values.
class GridPanel : public Leir::UIPanel {
public:
    GridPanel();
    ~GridPanel() override;

    void SetFont(Leir::Font* font);

    bool IsManual() const { return m_Manual; }
    void SetManual(bool manual);

    // Diagnostic controls.
    bool IsChunkOnly() const { return m_ChunkOnly; }
    bool IsDisableClip() const { return m_DisableClip; }
    bool IsThinChunks() const { return m_ThinChunks; }
    uint32_t GetLevelMask() const { return m_LevelMask; }
    void SetDiagnostics(uint32_t nanSkipped, uint32_t quadCount, uint32_t quadMax,
                        uint32_t segCount);

    // Stored manual values (read by main.cpp when manual).
    float GetGridDensityOverride() const { return m_DensityVal; }
    float GetGridFadeStartPx() const { return m_FadeStartVal; }
    float GetGridFadeEndPx() const { return m_FadeEndVal; }
    float GetGridChunkWidth() const { return m_ChunkWidthVal; }
    float GetGridHorizonFadeStart() const { return m_HorizonStartVal; }
    float GetGridHorizonFadeEnd() const { return m_HorizonEndVal; }

    // Auto mode: called every frame with the values the grid is actually using.
    // Writes them into the (greyed) inputs for display; programmatic SetValue
    // does NOT fire the onChanged callbacks, so the stored manual values are
    // preserved for when the user toggles back to manual.
    void SetAutoValues(float fadeStartPx, float fadeEndPx, float chunkWidth,
                       float density, float horizonStart, float horizonEnd);

    // Horizon fade band (view depth, world units) for a given camera height.
    // Piecewise-linear through the user-tuned breakpoints (camH -> start : end):
    //   <=30 -> 100:400, 100 -> 200:800, 200 -> 400:1400, 300 -> 600:1800,
    //   500 -> 800:2000, 1000 -> 900:2000, >=2000 -> 1000:2000
    // (monotonic; clamped at the extremes; end caps at the camera far plane 2000).
    static void ComputeAutoHorizon(float camH, float& outStart, float& outEnd);

    Leir::Vector2 GetMinSize() const override;

private:
    void AddField(Leir::UIPanel* parent, const std::string& labelText,
                  UIDragFloatInput*& outInput, float initial,
                  std::function<void(float)> onChanged);
    void UpdateEnabledVisuals();

    Leir::UIButton* m_ManualButton = nullptr;
    Leir::UIButton* m_ChunkOnlyBtn = nullptr;
    Leir::UIButton* m_DisableClipBtn = nullptr;
    Leir::UIButton* m_ThinChunksBtn = nullptr;
    Leir::UIButton* m_LevelBtns[4] = {};
    Leir::UILabel* m_DiagLabel = nullptr;

    UIDragFloatInput* m_Density = nullptr;
    UIDragFloatInput* m_FadeStart = nullptr;
    UIDragFloatInput* m_FadeEnd = nullptr;
    UIDragFloatInput* m_ThickWidth = nullptr;
    UIDragFloatInput* m_HorizonStart = nullptr;
    UIDragFloatInput* m_HorizonEnd = nullptr;

    // Stored manual values (updated by user edits; preserved across auto).
    float m_DensityVal = -1.0f;
    float m_FadeStartVal = 15.0f;
    float m_FadeEndVal = 30.0f;
    float m_ChunkWidthVal = 0.9f;
    float m_HorizonStartVal = 1000.0f;
    float m_HorizonEndVal = 1800.0f;

    bool m_Manual = false; // auto by default: inputs greyed out, horizon auto

    // Diagnostic state.
    bool m_ChunkOnly = false;
    bool m_DisableClip = false;
    bool m_ThinChunks = false;
    uint32_t m_LevelMask = 0xFu; // all 4 levels on by default
};