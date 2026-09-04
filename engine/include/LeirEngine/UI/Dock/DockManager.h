#pragma once

/**
 * @file DockManager.h
 * @brief Dock tree root: panel registry, layout, drag&drop and serialization.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/Dock/DockPanel.h"
#include "LeirEngine/Math/Vector2.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class DockNode;
class DockPane;
class DockTab;
class DockDropOverlay;
class DockSplitNode;
class Font;
class UIContextMenu;

/**
 * @brief Drop zone for docking (split or tab-merge).
 * @ingroup UI
 */
enum class LEIR_API DockDropZone {
    None,   ///< No drop.
    Left,   ///< Split left.
    Right,  ///< Split right.
    Top,    ///< Split top.
    Bottom, ///< Split bottom.
    Center, ///< Tab-merge (center).
};

/**
 * @brief Root of the dock tree: registry, layout, drag&drop and serialization.
 * @ingroup UI
 * @details Add as a child of a UICanvas; keep the bottom of the screen free for a status bar.
 */
class LEIR_API DockManager : public UIPanel {
public:
    /**
     * @brief Constructs an empty dock manager.
     */
    DockManager();

    /**
     * @brief Destroys the dock manager and its tree.
     */
    ~DockManager() override;

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
     * @brief Registers a dockable panel (content subtree owned by the caller).
     * @param[in] id Panel id (unique).
     * @param[in] title Display title.
     * @param[in] content Content element.
     * @param[in] closeable True if closeable.
     * @return Registered DockPanel pointer.
     */
    DockPanel* RegisterPanel(const std::string& id, const std::string& title,
                             UIElement* content, bool closeable);

    /**
     * @brief Finds a panel by id.
     * @param[in] id Panel id.
     * @return Panel or nullptr.
     */
    DockPanel* FindPanelById(const std::string& id) const;

    /**
     * @brief Builds the default layout (Hierarchy | Viewport | Inspector).
     */
    void BuildDefaultLayout();

    /**
     * @brief Loads layout from JSON.
     * @param[in] json JSON string (from SerializeLayout).
     * @return True on success.
     */
    bool LoadLayout(const std::string& json);

    /**
     * @brief Serializes layout to JSON.
     * @return JSON string.
     */
    std::string SerializeLayout() const;

    /**
     * @brief Returns the root dock node.
     * @return Root node or nullptr.
     */
    DockNode* GetRoot() const { return m_Root; }

    /**
     * @brief Finds the pane hosting a panel.
     * @param[in] panel Panel to find.
     * @return Pane or nullptr.
     */
    DockPane* FindPaneByPanel(DockPanel* panel) const;

    /**
     * @brief Finds the pane at a screen position.
     * @param[in] node Node to search under (nullptr = root).
     * @param[in] pos Screen position.
     * @return Pane or nullptr.
     */
    DockPane* FindPaneAt(DockNode* node, const Vector2& pos) const;

    /**
     * @brief Closes a panel (removes from its pane, cleans up empty panes).
     * @param[in] panel Panel to close.
     */
    void ClosePanel(DockPanel* panel);

    /**
     * @brief Merges a panel into a target pane as a new tab.
     * @param[in] panel Panel to move.
     * @param[in] target Target pane.
     */
    void MergeIntoPane(DockPanel* panel, DockPane* target);

    /**
     * @brief Splits a target pane and moves a panel into the new zone.
     * @param[in] panel Panel to move.
     * @param[in] target Target pane to split.
     * @param[in] zone Drop zone (Left/Right/Top/Bottom).
     */
    void SplitPane(DockPanel* panel, DockPane* target, DockDropZone zone);

    /**
     * @brief Schedules a panel close (deferred to next Process).
     * @param[in] panel Panel to close.
     */
    void RequestClosePanel(DockPanel* panel);

    /**
     * @brief Detaches a panel into an external window.
     * @details Removes the panel from the dock tree (the content is reparented
     *  out of its pane) and fires SetOnPanelDetached so the host can create the
     *  external window hosting the SAME content reference (no copy). Re-dock
     *  with ReattachPanel when the window closes.
     * @param[in] panel Panel to detach.
     */
    void DetachPanel(DockPanel* panel);

    /**
     * @brief Re-docks a detached panel (called when its external window closes).
     * @details Adds the panel back into the dock tree. Prefers the pane it was
     *  detached from; falls back to the first available pane, or creates a new
     *  root pane if the tree is empty.
     * @param[in] panel Panel to re-attach.
     */
    void ReattachPanel(DockPanel* panel);

    /**
     * @brief Sets the callback fired after a panel is detached.
     * @details The host (editor) uses it to create the external window hosting
     *  the panel's content. Receives the detached DockPanel*.
     * @param[in] cb Callback.
     */
    void SetOnPanelDetached(std::function<void(DockPanel*)> cb) { m_OnPanelDetached = std::move(cb); }

    /**
     * @brief Whether a panel is currently detached (floating in a window).
     * @param[in] panel Panel to query.
     * @return True if detached.
     */
    bool IsPanelDetached(DockPanel* panel) const { return panel && panel->detached; }

    /**
     * @brief Opens the tab context menu (right-click on a tab) for a panel.
     * @details Rebuilds a reused UIContextMenu with panel actions: "Detach to
     *  Window" and (for closeable panels) "Close Panel".
     * @param[in] panel Panel the menu is for.
     * @param[in] pos Canvas position to open at.
     */
    void OpenTabContextMenu(DockPanel* panel, const Vector2& pos);

    /**
     * @brief Processes deferred closes (call every frame from the host).
     */
    void Process();

    /**
     * @brief Starts a tab drag (called by DockTab).
     * @param[in] tab Tab being dragged.
     * @param[in] pos Pointer position.
     */
    void BeginTabDrag(DockTab* tab, const Vector2& pos);

    /**
     * @brief Handles pointer move (drag).
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Handles pointer release (drop).
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Sets the callback invoked after any layout change.
     * @param[in] cb Callback.
     */
    void SetOnLayoutChanged(std::function<void()> cb) { m_OnLayoutChanged = std::move(cb); }

    /**
     * @brief Notifies that layout changed (invokes the callback).
     */
    void NotifyLayoutChanged();

    /**
     * @brief Lays out the dock tree with absolute rects.
     * @param[in] availableSize Available size.
     * @param[in] parentOffset Parent absolute offset.
     */
    void ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset = Vector2(0.0f, 0.0f)) override;

    DockManager(const DockManager&) = delete;
    DockManager& operator=(const DockManager&) = delete;

private:
    void DestroyTree();
    void ReplaceNode(DockNode* oldNode, DockNode* newNode, DockSplitNode* parentSplit);
    void RemovePanelFromPane(DockPanel* panel);
    void CleanupEmptyPanes();
    bool CleanupNode(DockNode* node);
    void PlaceMissingPanels();
    void PushOverlayToTop();
    void ClearDanglingPointers();
    DockDropZone ComputeZone(DockPane* pane, const Vector2& pos) const;

    DockNode* m_Root = nullptr;                           ///< Root dock node.
    DockDropOverlay* m_Overlay = nullptr;                 ///< Drop overlay (owned).
    Font* m_Font = nullptr;                               ///< Font for tabs.
    std::vector<std::unique_ptr<DockPanel>> m_Panels;     ///< Panel registry.
    std::function<void()> m_OnLayoutChanged;              ///< Layout changed callback.
    std::function<void(DockPanel*)> m_OnPanelDetached;    ///< Panel detached callback.
    UIContextMenu* m_TabMenu = nullptr;                   ///< Tab context menu (lazy, owned).

    DockTab* m_DragTab = nullptr;                         ///< Dragged tab.
    DockPanel* m_DragPanel = nullptr;                     ///< Dragged panel.
    Vector2 m_DragStartPos = {0.0f, 0.0f};                 ///< Drag start position.
    Vector2 m_GrabOffset = {0.0f, 0.0f};                   ///< Grab offset.
    bool m_Dragging = false;                              ///< Dragging flag.
    DockPane* m_HoverPane = nullptr;                      ///< Hovered pane during drag.
    DockDropZone m_HoverZone = DockDropZone::None;        ///< Hovered zone.

    DockPanel* m_PendingClosePanel = nullptr;             ///< Pending deferred close.
};

} // namespace Leir
