#include "LeirEngine/UI/Dock/DockTabBar.h"
#include "LeirEngine/UI/Dock/DockPane.h"
#include "LeirEngine/UI/Dock/DockManager.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Pointer.h"
#include <algorithm>

namespace Leir {

// ---- DockTabBar ----

DockTabBar::DockTabBar()
{
    SetLayoutMode(LayoutMode::Row);
}

DockTabBar::~DockTabBar()
{
    auto children = GetChildren();
    for (auto* c : children) {
        RemoveChild(c);
        delete c;
    }
}

void DockTabBar::Init(DockPane* pane, DockManager* manager)
{
    m_Pane = pane;
    m_Manager = manager;
}

DockTab* DockTabBar::AddTab(DockPanel* panel)
{
    auto* tab = new DockTab();
    tab->SetName("Tab:" + panel->title);
    tab->Setup(panel, m_Pane, m_Manager, m_Font);
    AddChild(tab);
    return tab;
}

DockTab* DockTabBar::InsertTab(DockPanel* panel, size_t index)
{
    auto* tab = new DockTab();
    tab->SetName("Tab:" + panel->title);
    tab->Setup(panel, m_Pane, m_Manager, m_Font);
    InsertChildAt(tab, std::min(index, GetChildren().size()));
    return tab;
}

void DockTabBar::RemoveTab(DockPanel* panel)
{
    for (auto* c : GetChildren()) {
        if (auto* tab = dynamic_cast<DockTab*>(c))
            if (tab->GetPanel() == panel) {
                RemoveChild(tab);
                delete tab;
                return;
            }
    }
}

DockTab* DockTabBar::FindTab(DockPanel* panel) const
{
    for (auto* c : GetChildren())
        if (auto* tab = dynamic_cast<DockTab*>(c))
            if (tab->GetPanel() == panel)
                return tab;
    return nullptr;
}

Vector2 DockTabBar::GetMinSize() const
{
    float w = 0.0f;
    float h = 24.0f;
    for (auto* c : GetChildren()) {
        if (auto* tab = dynamic_cast<DockTab*>(c)) {
            Vector2 m = tab->GetMinSize();
            w += m.x;
            h = std::max(h, m.y);
        }
    }
    return {w, h};
}

// ---- DockTab ----

DockTab::DockTab() = default;
DockTab::~DockTab() = default;

void DockTab::Setup(DockPanel* panel, DockPane* pane, DockManager* manager, Font* font)
{
    m_Panel = panel;
    m_Pane = pane;
    m_Manager = manager;
    m_Font = font;
}

bool DockTab::IsActive() const
{
    return m_Pane && m_Pane->GetActivePanel() == m_Panel;
}

Vector2 DockTab::GetMinSize() const
{
    float w = 24.0f;
    if (m_Font && m_Panel)
        w += m_Font->MeasureText(m_Panel->title).x;
    if (m_Panel && m_Panel->closeable)
        w += 16.0f;
    return {w, 24.0f};
}

bool DockTab::OnPointerDown(const Vector2& pos)
{
    if (!m_Panel)
        return true;

    // Right-click on a tab opens the panel context menu (Detach to Window,
    // Close Panel). The polling state is already updated when the UI hook
    // dispatches (EventQueue processes Pointer events before the canvas hooks).
    if (Pointer::IsDown(PointerButton::Secondary)) {
        if (m_Manager)
            m_Manager->OpenTabContextMenu(m_Panel, pos);
        return true;
    }

    const auto& cr = GetComputedRect();
    const float localX = pos.x - cr.x;

    // Close button (rightmost 16px of a closeable tab). The close is deferred
    // so this click finishes dispatching before the tab element is deleted.
    if (m_Panel->closeable && localX >= cr.z - 16.0f) {
        if (m_Manager)
            m_Manager->RequestClosePanel(m_Panel);
        return true;
    }

    if (m_Pane)
        m_Pane->SetActivePanel(m_Panel);
    if (m_Manager)
        m_Manager->BeginTabDrag(this, pos);
    return true;
}

bool DockTab::OnPointerUp(const Vector2& pos)
{
    (void)pos;
    return true;
}

void DockTab::OnPointerEnter(const Vector2& pos)
{
    (void)pos;
}

void DockTab::OnPointerExit()
{
}

} // namespace Leir
