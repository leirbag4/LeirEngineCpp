#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
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

private:
    UIPanel* m_Ghost = nullptr;
    UIPanel* m_Zone = nullptr;
};

} // namespace Leir
