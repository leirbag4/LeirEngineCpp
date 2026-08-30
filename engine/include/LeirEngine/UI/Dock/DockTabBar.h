#pragma once

/**
 * @file DockTabBar.h
 * @brief Dock tab bar (row of tabs) and single tab widget.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/Dock/DockPanel.h"

namespace Leir {

class DockPane;
class DockManager;
class DockTab;
class Font;

/**
 * @brief Row of tabs at the top of a DockPane.
 * @ingroup UI
 */
class LEIR_API DockTabBar : public UIPanel {
public:
    /**
     * @brief Constructs an empty tab bar.
     */
    DockTabBar();

    /**
     * @brief Destroys the tab bar.
     */
    ~DockTabBar() override;

    /**
     * @brief Initializes the tab bar for a pane and manager.
     * @param[in] pane Owning pane.
     * @param[in] manager Dock manager.
     */
    void Init(DockPane* pane, DockManager* manager);

    /**
     * @brief Sets the font for tabs.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font) { m_Font = font; }

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Adds a tab for a panel.
     * @param[in] panel Panel for the tab.
     * @return Created DockTab pointer.
     */
    DockTab* AddTab(DockPanel* panel);

    /**
     * @brief Inserts a tab at a specific index (used for tab reordering).
     * @param[in] panel Panel for the tab.
     * @param[in] index Position in the row.
     * @return Created DockTab pointer.
     */
    DockTab* InsertTab(DockPanel* panel, size_t index);

    /**
     * @brief Removes a tab for a panel.
     * @param[in] panel Panel whose tab to remove.
     */
    void RemoveTab(DockPanel* panel);

    /**
     * @brief Finds a tab for a panel.
     * @param[in] panel Panel to find.
     * @return Tab pointer or nullptr.
     */
    DockTab* FindTab(DockPanel* panel) const;

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

private:
    DockPane* m_Pane = nullptr;                     ///< Owning pane.
    DockManager* m_Manager = nullptr;               ///< Dock manager.
    Font* m_Font = nullptr;                         ///< Font for tabs.
};

/**
 * @brief Single tab: label + optional close button.
 * @ingroup UI
 * @details Pointer-down on the label activates the tab and starts a dock drag;
 *  pointer-down on the close area closes the panel.
 */
class LEIR_API DockTab : public UIElement {
public:
    /**
     * @brief Constructs an empty tab.
     */
    DockTab();

    /**
     * @brief Destroys the tab.
     */
    ~DockTab() override;

    /**
     * @brief Sets up the tab for a panel, pane, manager and font.
     * @param[in] panel Panel for this tab.
     * @param[in] pane Owning pane.
     * @param[in] manager Dock manager.
     * @param[in] font Font pointer.
     */
    void Setup(DockPanel* panel, DockPane* pane, DockManager* manager, Font* font);

    /**
     * @brief Returns the panel for this tab.
     * @return Panel pointer.
     */
    DockPanel* GetPanel() const { return m_Panel; }

    /**
     * @brief Returns the dock manager.
     * @return Manager pointer.
     */
    DockManager* GetManager() const { return m_Manager; }

    /**
     * @brief Returns the font.
     * @return Font pointer.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Returns whether the tab is active (its panel is the pane's active).
     * @return True if active.
     */
    bool IsActive() const;

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Handles pointer press (activate + start drag or close).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer release.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Called when pointer enters.
     * @param[in] pos Pointer position.
     */
    void OnPointerEnter(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

private:
    DockPanel* m_Panel = nullptr;                   ///< Panel for this tab.
    DockPane* m_Pane = nullptr;                     ///< Owning pane.
    DockManager* m_Manager = nullptr;               ///< Dock manager.
    Font* m_Font = nullptr;                         ///< Font.
};

} // namespace Leir
