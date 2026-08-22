#pragma once

#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/Math/Vector2.h"

#include <cstdio>
#include <string>

// Dockable "DBG" panel: toggles a verbose gizmo/selection recorder that writes
// every frame's state (camera, mouse/viewport events, selected object transform,
// gizmo tool/space/hovered+dragged handle) to
// <configDir>/LeirEngine/records/record_gizmo_log.txt.
//
// The red button starts recording ("record gizmo log"); while recording it
// shows "stop gizmo log". EditorApp feeds lines via RecordLine() every frame.
class GizmoLogPanel : public Leir::UIPanel {
public:
    GizmoLogPanel();
    ~GizmoLogPanel() override;

    void SetFont(Leir::Font* font);

    bool IsRecording() const { return m_Recording; }
    // Starts recording (creates/truncates the log file) / stops + closes it.
    void SetRecording(bool on);
    void ToggleRecording() { SetRecording(!m_Recording); }

    // Appends a line to the log file when recording.
    void RecordLine(const std::string& line);

    Leir::Vector2 GetMinSize() const override;

protected:
    void OnLayoutComputed() override;

private:
    void ApplyButtonState();

    Leir::UIButton* m_Button = nullptr;
    Leir::UILabel* m_Status = nullptr;
    std::FILE* m_File = nullptr;
    bool m_Recording = false;
};