#pragma once

/**
 * @file DockPane.h
 * @brief Dock pane: tab bar on top plus active panel content.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Dock/DockNode.h"
#include "LeirEngine/UI/Dock/DockPanel.h"
#include <vector>

namespace Leir {

class DockManager;
class DockTabBar;
class UIElement;

/**
 * @brief Dock pane: a tab group with a tab bar and active content.
 * @ingroup UI
 */
class LEIR_API DockPane : public DockNode {
public:
    /**
     * @brief Constructs a pane for a manager.
     * @param[in] manager Owning DockManager.
     */
    explicit DockPane(DockManager* manager);

    /**
     * @brief Destroys the pane and its tab bar.
     */
    ~DockPane() override;

    /**
     * @brief Returns tab count.
     * @return Number of tabs.
     */
    size_t GetTabCount() const { return m_Tabs.size(); }

    /**
     * @brief Returns tab at an index.
     * @param[in] i Index.
     * @return Panel pointer.
     */
    DockPanel* GetTab(size_t i) const { return m_Tabs[i]; }

    /**
     * @brief Returns active tab index.
     * @return Index or -1 if empty.
     */
    int GetActiveIndex() const { return m_ActiveIndex; }

    /**
     * @brief Returns active panel.
     * @return Active panel or nullptr.
     */
    DockPanel* GetActivePanel() const;

    /**
     * @brief Returns whether the pane contains a panel.
     * @param[in] panel Panel to query.
     * @return True if contains.
     */
    bool Contains(DockPanel* panel) const;

    /**
     * @brief Adds a tab (optionally makes it active).
     * @param[in] panel Panel to add.
     * @param[in] makeActive True to activate.
     */
    void AddTab(DockPanel* panel, bool makeActive = true);

    /**
     * @brief Removes a tab.
     * @param[in] panel Panel to remove.
     */
    void RemoveTab(DockPanel* panel);

    /**
     * @brief Sets the active panel.
     * @param[in] panel Panel to activate.
     */
    void SetActivePanel(DockPanel* panel);

    /**
     * @brief Sets active tab by index.
     * @param[in] index Tab index.
     */
    void SetActiveTab(int index);

    /**
     * @brief Inserts a tab at a specific index.
     * @param[in] panel Panel to insert.
     * @param[in] index Position.
     */
    void InsertTab(DockPanel* panel, size_t index);

    /**
     * @brief Reorders a tab to the insertion point implied by pos.x.
     * @param[in] panel Panel to reorder.
     * @param[in] pos Pointer position.
     * @return True if order changed.
     */
    bool ReorderTabTo(DockPanel* panel, const Vector2& pos);

    /**
     * @brief Returns the tab bar.
     * @return Tab bar pointer.
     */
    DockTabBar* GetTabBar() const { return m_TabBar; }

    /**
     * @brief Returns the owning dock manager.
     * @return Manager pointer.
     */
    DockManager* GetDockManager() const { return m_Manager; }

private:
    DockManager* m_Manager = nullptr;               ///< Owning manager.
    DockTabBar* m_TabBar = nullptr;                 ///< Tab bar (owned).
    std::vector<DockPanel*> m_Tabs;                 ///< Tabs.
    int m_ActiveIndex = 0;                          ///< Active index.
    UIElement* m_ContentHost = nullptr;             ///< Content host.
};

} // namespace Leir
