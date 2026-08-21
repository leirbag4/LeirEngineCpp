#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Math/Vector3.h>
#include <LeirEngine/Math/Vector4.h>
#include "UIDragFloatInput.h"
#include <functional>

// Dockable "Test2" panel with live knobs for one gizmo test line: color
// (R/G/B), transparency (Alpha) and width (pixels). The EditorApp reads the
// state every frame and draws the line in DrawGizmoShowcase, so dragging an
// input updates the gizmo in real time.
class GizmoLineTestPanel : public Leir::UIPanel {
public:
    GizmoLineTestPanel();
    ~GizmoLineTestPanel() override;

    void SetFont(Leir::Font* font);

    Leir::Vector3 GetStart() const { return m_Start; }
    Leir::Vector3 GetEnd() const { return m_End; }
    Leir::Vector4 GetColor() const { return {m_RVal, m_GVal, m_BVal, m_AVal}; }
    float GetWidth() const { return m_WidthVal; }
    // Manual LOD driver for the editor grid: uniform pixels-per-world-unit
    // override (>= 0) that drives the recursive chunk transition, decoupled
    // from the camera. -1 (default) = camera-driven (real per-line density).
    float GetGridDensityOverride() const { return m_DensityVal; }
    // Live cell-fade thresholds (px of cell size) for the grid LOD: below
    // fadeStart a line's role is invisible, above fadeEnd fully visible.
    float GetGridFadeStartPx() const { return m_FadeStartVal; }
    float GetGridFadeEndPx() const { return m_FadeEndVal; }
    // Chunk (thick) line width in pixels for the grid LOD.
    float GetGridChunkWidth() const { return m_ChunkWidthVal; }

    Leir::Vector2 GetMinSize() const override;

protected:
    void OnLayoutComputed() override;

private:
    void AddField(Leir::UIPanel* parent, const std::string& labelText,
                  UIDragFloatInput*& outInput, float initial,
                  std::function<void(float)> onChanged);

    UIDragFloatInput* m_R = nullptr;
    UIDragFloatInput* m_G = nullptr;
    UIDragFloatInput* m_B = nullptr;
    UIDragFloatInput* m_A = nullptr;
    UIDragFloatInput* m_Width = nullptr;
    UIDragFloatInput* m_Density = nullptr;
    UIDragFloatInput* m_FadeStart = nullptr;
    UIDragFloatInput* m_FadeEnd = nullptr;
    UIDragFloatInput* m_ThickWidth = nullptr;

    Leir::Vector3 m_Start = {0.0f, 0.6f, 0.0f};
    Leir::Vector3 m_End = {5.0f, 0.6f, 0.0f};
    float m_RVal = 1.0f;
    float m_GVal = 1.0f;
    float m_BVal = 1.0f;
    float m_AVal = 0.5f;
    float m_WidthVal = 3.0f;
    // Manual LOD driver for the editor grid: uniform pxPerUnit (pixels per
    // world unit). Lower value = camera farther away = coarser chunks.
    // -1 (default) = camera-driven: each line reads the real screen density
    // at its closest visible point, so zooming the camera drives the LOD.
    float m_DensityVal = -1.0f;
    // Cell-fade thresholds in pixels (Unity-style defaults): the fine level is
    // fully hidden below 15px of cell size and fully visible above 30px, so
    // only 2 levels are ever visible and micro-squares never turn solid.
    float m_FadeStartVal = 15.0f;
    float m_FadeEndVal = 30.0f;
    // Chunk (thick) grid line width in pixels (drag to make chunk lines finer).
    float m_ChunkWidthVal = 0.9f;
};