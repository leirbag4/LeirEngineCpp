#include "LeirEngine/UI/Dock/DockPane.h"
#include "LeirEngine/UI/Dock/DockTabBar.h"
#include "LeirEngine/UI/Dock/DockManager.h"
#include <algorithm>

namespace Leir {

DockPane::DockPane(DockManager* manager)
    : DockNode(DockNodeType::Pane)
    , m_Manager(manager)
{
    SetName("DockPane");
    SetColor({0.16f, 0.16f, 0.18f, 1.0f});
    SetLayoutMode(LayoutMode::Column);
    SetPadding(0.0f, 0.0f, 0.0f, 0.0f);
    SetSpacing(0.0f);

    m_TabBar = new DockTabBar();
    m_TabBar->SetName("DockTabBar");
    m_TabBar->SetColor({0.13f, 0.13f, 0.15f, 1.0f});
    m_TabBar->SetSizePolicy(SizePolicy::Fixed);
    AddChild(m_TabBar);

    if (manager) {
        m_TabBar->Init(this, manager);
        m_TabBar->SetFont(manager->GetFont());
    }
}

DockPane::~DockPane()
{
    // The tab bar is owned by the pane. The content host is owned by the
    // editor/caller — the base dtor only nulls its parent pointer.
    if (m_TabBar) {
        RemoveChild(m_TabBar);
        delete m_TabBar;
    }
}

DockPanel* DockPane::GetActivePanel() const
{
    if (m_ActiveIndex >= 0 && m_ActiveIndex < (int)m_Tabs.size())
        return m_Tabs[m_ActiveIndex];
    return nullptr;
}

bool DockPane::Contains(DockPanel* panel) const
{
    for (auto* p : m_Tabs)
        if (p == panel)
            return true;
    return false;
}

void DockPane::AddTab(DockPanel* panel, bool makeActive)
{
    if (!panel)
        return;
    for (auto* p : m_Tabs)
        if (p == panel) {
            if (makeActive)
                SetActivePanel(panel);
            return;
        }

    m_Tabs.push_back(panel);
    if (m_TabBar)
        m_TabBar->AddTab(panel);

    if (makeActive || m_ContentHost == nullptr)
        SetActivePanel(panel);
}

void DockPane::RemoveTab(DockPanel* panel)
{
    auto it = std::find(m_Tabs.begin(), m_Tabs.end(), panel);
    if (it == m_Tabs.end())
        return;
    size_t idx = it - m_Tabs.begin();
    m_Tabs.erase(it);

    if (m_TabBar)
        m_TabBar->RemoveTab(panel);

    if (m_ContentHost == panel->content) {
        m_ContentHost->SetActive(false);
        RemoveChild(m_ContentHost);
        m_ContentHost = nullptr;
        if (!m_Tabs.empty())
            SetActiveTab((int)std::min(idx, m_Tabs.size() - 1));
    } else if (idx < (size_t)m_ActiveIndex) {
        m_ActiveIndex--;
    }
}

void DockPane::SetActivePanel(DockPanel* panel)
{
    int idx = -1;
    for (size_t i = 0; i < m_Tabs.size(); ++i)
        if (m_Tabs[i] == panel) {
            idx = (int)i;
            break;
        }
    if (idx < 0)
        return;

    m_ActiveIndex = idx;

    if (m_ContentHost == panel->content)
        return;

    if (m_ContentHost) {
        m_ContentHost->SetActive(false);
        RemoveChild(m_ContentHost);
    }

    m_ContentHost = panel->content;
    if (m_ContentHost) {
        m_ContentHost->SetActive(true);
        m_ContentHost->SetSizePolicy(SizePolicy::Fill);
        m_ContentHost->GetRect().anchor = AnchorSet::TopLeft();
        m_ContentHost->GetRect().offset = {};
        AddChild(m_ContentHost);
    }
}

void DockPane::SetActiveTab(int index)
{
    if (index < 0 || index >= (int)m_Tabs.size())
        return;
    SetActivePanel(m_Tabs[index]);
}

} // namespace Leir
