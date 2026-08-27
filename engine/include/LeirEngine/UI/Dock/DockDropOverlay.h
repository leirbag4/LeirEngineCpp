#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

// Visual feedback for dock drag & drop: a ghost of the dragged tab plus a
// highlighted drop zone. Rendered in the overlay layer (above viewports) via
// SetOverlayLayer. Inactive unless a drag is in progress.
class LEIR_API DockDropOverlay : public UIPanel {
public:
    DockDropOverlay();
    ~DockDropOverlay() override;

    void SetVisible(bool visible);
    void SetGhostRect(const Vector4& rect);
    void SetZoneRect(const Vector4& rect, const Vector4& color);
    void HideZone();

    // Computes this panel's rect WITHOUT adding its position to the ghost/zone
    // children: those are positioned in ABSOLUTE screen coordinates by
    // SetGhostRect/SetZoneRect. The base Free layout would += this panel's
    // origin onto them every frame (accumulating when the overlay is not at
    // (0,0)), which would drift the drag ghost.
    void ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset = Vector2(0.0f, 0.0f)) override;

private:
    UIPanel* m_Ghost = nullptr;
    UIPanel* m_Zone = nullptr;
};

} // namespace Leir
