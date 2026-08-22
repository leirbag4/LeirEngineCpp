#include "LeirEngine/UI/Dock/DockDropOverlay.h"
#include "LeirEngine/UI/UIPanel.h"

namespace Leir {

DockDropOverlay::DockDropOverlay()
{
    SetName("DockDropOverlay");
    SetOverlayLayer(true);
    SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    GetRect().anchor = AnchorSet::Stretch();
    GetRect().offset = {};

    m_Ghost = new UIPanel();
    m_Ghost->SetName("DropGhost");
    m_Ghost->SetColor({1.0f, 1.0f, 1.0f, 0.18f});
    m_Ghost->SetActive(false);
    AddChild(m_Ghost);

    m_Zone = new UIPanel();
    m_Zone->SetName("DropZone");
    m_Zone->SetColor({0.3f, 0.6f, 1.0f, 0.28f});
    m_Zone->SetActive(false);
    AddChild(m_Zone);

    SetActive(false);
}

DockDropOverlay::~DockDropOverlay()
{
    if (m_Ghost) {
        RemoveChild(m_Ghost);
        delete m_Ghost;
    }
    if (m_Zone) {
        RemoveChild(m_Zone);
        delete m_Zone;
    }
}

void DockDropOverlay::SetVisible(bool visible)
{
    SetActive(visible);
}

void DockDropOverlay::SetGhostRect(const Vector4& rect)
{
    m_Ghost->SetActive(true);
    m_Ghost->GetRect().anchor = AnchorSet::TopLeft();
    m_Ghost->GetRect().offset = {rect.x, rect.y, rect.x + rect.z, rect.y + rect.w};
}

void DockDropOverlay::SetZoneRect(const Vector4& rect, const Vector4& color)
{
    m_Zone->SetActive(true);
    m_Zone->SetColor(color);
    m_Zone->GetRect().anchor = AnchorSet::TopLeft();
    m_Zone->GetRect().offset = {rect.x, rect.y, rect.x + rect.z, rect.y + rect.w};
}

void DockDropOverlay::HideZone()
{
    m_Zone->SetActive(false);
}

void DockDropOverlay::ComputeLayout(const Vector2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);
    // The ghost/zone are positioned in ABSOLUTE screen coordinates by
    // SetGhostRect/SetZoneRect. Lay them out directly from their own rects:
    // the base Free layout would += this panel's origin onto their offsets
    // EVERY frame (permanent mutation), which accumulates when the overlay is
    // not at (0,0) and drifts the drag ghost down each frame.
    for (auto* child : GetChildren()) {
        if (!child->IsActive())
            continue;
        child->ComputeLayout({m_ComputedRect.z, m_ComputedRect.w});
    }
    OnLayoutComputed();
}

} // namespace Leir
