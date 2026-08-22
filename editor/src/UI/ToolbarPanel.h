#pragma once

#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/Math/Vector2.h"

#include <functional>

// Non-dockable top toolbar (a sibling of the DockManager in the canvas, full
// width). Radio-group tool buttons W/E/R (translate/rotate/scale) + a
// Global/Local coordinate-space toggle (grayed out in Scale mode, where only
// the local space exists — like Unity).
//
// The active tool button is highlighted (lighter background) so the group
// reads at a glance; the others stay dim and clickable.
class ToolbarPanel : public Leir::UIPanel {
public:
    enum class Tool { None, Translate, Rotate, Scale };
    enum class Space { Global, Local };

    ToolbarPanel();
    ~ToolbarPanel() override;

    void SetFont(Leir::Font* font);

    void SetTool(Tool t);
    Tool GetTool() const { return m_Tool; }
    void SetSpace(Space s);
    Space GetSpace() const { return m_Space; }

    // Scale is always local (single mode): gray out the Global/Local toggle.
    void SetScaleMode(bool scale);

    void SetOnToolChanged(std::function<void(Tool)> cb) { m_OnToolChanged = std::move(cb); }
    void SetOnSpaceChanged(std::function<void(Space)> cb) { m_OnSpaceChanged = std::move(cb); }

    Leir::Vector2 GetMinSize() const override;

protected:
    void OnLayoutComputed() override;

private:
    void ApplyToolState();
    void ApplySpaceState();

    Leir::UIButton* m_ButtonW = nullptr;
    Leir::UIButton* m_ButtonE = nullptr;
    Leir::UIButton* m_ButtonR = nullptr;
    Leir::UIButton* m_SpaceButton = nullptr;
    Leir::UILabel* m_Spacer = nullptr;

    Tool m_Tool = Tool::Translate;
    Space m_Space = Space::Global;
    bool m_ScaleMode = false;
    std::function<void(Tool)> m_OnToolChanged;
    std::function<void(Space)> m_OnSpaceChanged;
};