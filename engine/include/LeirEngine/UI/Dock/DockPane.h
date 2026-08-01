#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Dock/DockNode.h"
#include "LeirEngine/UI/Dock/DockPanel.h"
#include <vector>

namespace Leir {

class DockManager;
class DockTabBar;
class UIElement;

// A tab group: a DockTabBar on top plus the active panel's content below.
class LEIR_API DockPane : public DockNode {
public:
    explicit DockPane(DockManager* manager);
    ~DockPane() override;

    size_t GetTabCount() const { return m_Tabs.size(); }
    DockPanel* GetTab(size_t i) const { return m_Tabs[i]; }
    int GetActiveIndex() const { return m_ActiveIndex; }
    DockPanel* GetActivePanel() const;
    bool Contains(DockPanel* panel) const;

    void AddTab(DockPanel* panel, bool makeActive = true);
    void RemoveTab(DockPanel* panel);
    void SetActivePanel(DockPanel* panel);
    void SetActiveTab(int index);

    DockManager* GetDockManager() const { return m_Manager; }

private:
    DockManager* m_Manager = nullptr;
    DockTabBar* m_TabBar = nullptr;
    std::vector<DockPanel*> m_Tabs;
    int m_ActiveIndex = 0;
    UIElement* m_ContentHost = nullptr;
};

} // namespace Leir
