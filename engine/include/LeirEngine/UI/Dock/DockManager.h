#pragma once
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

enum class LEIR_API DockDropZone {
    None,
    Left,
    Right,
    Top,
    Bottom,
    Center,   // tab-merge (drop onto the pane's center)
};

// Root of the dock tree: owns the DockPanel registry, drives tree operations
// (split / tab-merge / close / collapse), drag & drop, and layout
// serialization. Add as a child of a UICanvas; keep the bottom of the screen
// free (e.g. offset {0,0,0,-30}) for a status bar.
class LEIR_API DockManager : public UIPanel {
public:
    DockManager();
    ~DockManager() override;

    void SetFont(Font* font) { m_Font = font; }
    Font* GetFont() const { return m_Font; }

    // Register a dockable panel. The content subtree is owned by the caller.
    DockPanel* RegisterPanel(const std::string& id, const std::string& title,
                             UIElement* content, bool closeable);
    DockPanel* FindPanelById(const std::string& id) const;

    // Layout building / persistence
    void BuildDefaultLayout();
    bool LoadLayout(const std::string& json);
    std::string SerializeLayout() const;

    // Tree queries
    DockNode* GetRoot() const { return m_Root; }
    DockPane* FindPaneByPanel(DockPanel* panel) const;
    DockPane* FindPaneAt(DockNode* node, const Vector2& pos) const;

    // Tree operations
    void ClosePanel(DockPanel* panel);
    void MergeIntoPane(DockPanel* panel, DockPane* target);
    void SplitPane(DockPanel* panel, DockPane* target, DockDropZone zone);

    // Deferred close: schedules a close so the click that triggers it can
    // finish dispatching before the tab element is deleted. Call Process()
    // every frame from the host.
    void RequestClosePanel(DockPanel* panel);
    void Process();

    // Drag & drop (started by DockTab; the manager captures the pointer)
    void BeginTabDrag(DockTab* tab, const Vector2& pos);
    void OnPointerMove(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;

    // Fired after any layout change (used by the host to persist the layout).
    void SetOnLayoutChanged(std::function<void()> cb) { m_OnLayoutChanged = std::move(cb); }
    void NotifyLayoutChanged();

    DockManager(const DockManager&) = delete;
    DockManager& operator=(const DockManager&) = delete;

private:
    void DestroyTree();
    // Replaces `oldNode` with `newNode` inside `parentSplit`. If `parentSplit`
    // is null, `oldNode` is the root (the manager adopts `newNode` directly).
    // The parent must be passed explicitly: SplitPane re-parents the target
    // pane into the new split before replacing it, so computing the parent
    // from `oldNode->GetParent()` would point at the new split itself.
    void ReplaceNode(DockNode* oldNode, DockNode* newNode, DockSplitNode* parentSplit);
    void RemovePanelFromPane(DockPanel* panel);
    void CleanupEmptyPanes();
    bool CleanupNode(DockNode* node);
    void PlaceMissingPanels();
    void PushOverlayToTop();
    void ClearDanglingPointers();
    DockDropZone ComputeZone(DockPane* pane, const Vector2& pos) const;

    DockNode* m_Root = nullptr;
    DockDropOverlay* m_Overlay = nullptr;
    Font* m_Font = nullptr;
    std::vector<std::unique_ptr<DockPanel>> m_Panels;
    std::function<void()> m_OnLayoutChanged;

    // Drag state
    DockTab* m_DragTab = nullptr;
    DockPanel* m_DragPanel = nullptr;
    Vector2 m_DragStartPos = {0.0f, 0.0f};
    Vector2 m_GrabOffset = {0.0f, 0.0f};
    bool m_Dragging = false;
    DockPane* m_HoverPane = nullptr;
    DockDropZone m_HoverZone = DockDropZone::None;

    // Deferred close state
    DockPanel* m_PendingClosePanel = nullptr;
};

} // namespace Leir
