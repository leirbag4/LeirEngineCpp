#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <functional>
#include <string>
#include <vector>

namespace Leir {

class Font;
class UIScrollbar;
class UITextInput;
class UILabel;
class UIPanel;
class UITreeViewItem;

// Virtualized tree view with full-width rows, indent, expand/collapse arrow,
// single/multiple selection, drag&drop (nest + reorder) and inline F2 editing.
// Owns its scrollbars and the optional edit input (OwnsChild). Tree items are
// owned by the caller (like DockPanel content) but the view tracks roots and a
// flat cache of the currently visible items for virtualization.
class LEIR_API UITreeView : public UIElement {
public:
    UITreeView();
    ~UITreeView() override;

    bool OwnsChild(const UIElement* child) const override;

    // Roots of the tree (top-level items). Children are stored in each item's
    // tree children. Flat visible list is derived from expanded roots.
    void AddItem(UITreeViewItem* item, UITreeViewItem* parent = nullptr);
    void RemoveItem(UITreeViewItem* item);
    void ClearItems();
    int GetItemCount() const; // flat visible count
    UITreeViewItem* GetItemAt(int visibleIndex) const;
    const std::vector<UITreeViewItem*>& GetRoots() const { return m_Roots; }

    // Selection
    void SetMultipleSelectionEnabled(bool enabled) { m_MultipleSelection = enabled; }
    bool IsMultipleSelectionEnabled() const { return m_MultipleSelection; }

    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);

    UITreeViewItem* GetSelectedItem() const;
    void SetSelectedItem(UITreeViewItem* item);

    std::vector<UITreeViewItem*> GetSelectedItems() const { return m_SelectedItems; }
    void SetSelectedItems(const std::vector<UITreeViewItem*>& items);

    // Indent per tree level (px). Propagated to items on next layout. Default 16.
    void SetIndent(float indent);
    float GetIndent() const { return m_Indent; }

    // Row height (px). Default 20.
    void SetRowHeight(float h) { m_RowHeight = h; }
    float GetRowHeight() const { return m_RowHeight; }

    // Appearance — defaults match engine dark theme
    void SetSelectionColor(const Vector4& c) { m_SelectionColor = c; }
    Vector4 GetSelectionColor() const { return m_SelectionColor; }
    void SetHoverColor(const Vector4& c) { m_HoverColor = c; }
    Vector4 GetHoverColor() const { return m_HoverColor; }
    void SetTextColor(const Vector4& c) { m_TextColor = c; }
    Vector4 GetTextColor() const { return m_TextColor; }
    void SetTextHoverColor(const Vector4& c) { m_TextHoverColor = c; }
    Vector4 GetTextHoverColor() const { return m_TextHoverColor; }
    void SetTextSelectionColor(const Vector4& c) { m_TextSelectionColor = c; }
    Vector4 GetTextSelectionColor() const { return m_TextSelectionColor; }
    void SetArrowColor(const Vector4& c) { m_ArrowColor = c; }
    Vector4 GetArrowColor() const { return m_ArrowColor; }

    void SetFont(Font* font);
    Font* GetFont() const { return m_Font; }
    void CommitEdit(bool cancel); // called by inline edit input (Enter/Escape/Blur)

    // Editable — when true, F2 on a selected enabled item opens an inline
    // UITextInput overlay to rename. Default false.
    void SetEditable(bool editable) { m_Editable = editable; }
    bool IsEditable() const { return m_Editable; }

    // Scrollbar visibility toggles (like ScrollView/UITextArea)
    void SetVerticalScrollbarEnabled(bool e);
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }
    void SetHorizontalScrollbarEnabled(bool e);
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    Vector2 GetContentSize() const override;
    Vector2 GetViewportSize() const;
    void SetScrollOffset(Vector2 offset);
    Vector2 GetScrollOffset() const { return m_ScrollOffset; }

    // Events — SetOnX pattern (UIButton, UIScrollbar, DockManager)
    void SetOnSelectedIndexChanged(std::function<void(int)> cb) { m_OnSelectedIndexChanged = std::move(cb); }
    void SetOnSelectedItemChanged(std::function<void(UITreeViewItem*)> cb) { m_OnSelectedItemChanged = std::move(cb); }
    void SetOnSelectedItemsChanged(std::function<void(const std::vector<UITreeViewItem*>&)> cb) { m_OnSelectedItemsChanged = std::move(cb); }
    void SetOnItemExpanded(std::function<void(UITreeViewItem*)> cb) { m_OnItemExpanded = std::move(cb); }
    void SetOnItemCollapsed(std::function<void(UITreeViewItem*)> cb) { m_OnItemCollapsed = std::move(cb); }
    void SetOnItemDragged(std::function<void(UITreeViewItem* draggedItem, UITreeViewItem* newParent, int newIndex)> cb) { m_OnItemDragged = std::move(cb); }
    void SetOnItemDoubleClicked(std::function<void(UITreeViewItem*)> cb) { m_OnItemDoubleClicked = std::move(cb); }
    void SetOnItemRenamed(std::function<void(UITreeViewItem*, const std::string& oldText, const std::string& newText)> cb) { m_OnItemRenamed = std::move(cb); }

    // Input
    bool OnPointerDown(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    bool OnScroll(float delta) override;
    bool OnKeyDown(int key) override;
    void OnPointerExit() override;

    UITreeViewItem* GetHoveredItem() const { return m_HoveredItem; }
    void NotifyItemHovered(UITreeViewItem* item);

protected:
    void OnLayoutComputed() override;

private:
    void RebuildFlatCache() const;
    void InvalidateFlatCache() const { m_FlatDirty = true; m_CachedMaxWidthDirty = true; }
    void SyncScrollbars();
    void UpdateSelection(UITreeViewItem* clickedItem, bool ctrl, bool shift);
    void NotifySelectionChanged();
    void BeginEdit(UITreeViewItem* item);
    void UpdateEditInputRect();
    bool IsItemVisible(UITreeViewItem* item) const;

    // Drop handling
    enum class DropMode { None, Onto, Below };
    struct DropTarget { UITreeViewItem* item = nullptr; DropMode mode = DropMode::None; };
    DropTarget HitTestDropTarget(const Vector2& pos) const;
    void ClearDropHighlight();

    std::vector<UITreeViewItem*> m_Roots;
    mutable std::vector<UITreeViewItem*> m_FlatVisible;
    mutable bool m_FlatDirty = true;
    mutable float m_CachedMaxWidth = 0.0f;
    mutable bool m_CachedMaxWidthDirty = true;

    std::vector<UITreeViewItem*> m_SelectedItems;
    int m_LastSelectedIndex = -1; // for Shift range
    // When a plain click lands on an item that is ALREADY part of a multi
    // selection, the collapse-to-single is deferred to OnPointerUp so that
    // starting a drag preserves the multi-selection (standard treeview behavior).
    UITreeViewItem* m_DeferredSelect = nullptr;

    float m_Indent = 16.0f;
    float m_RowHeight = 20.0f;

    Vector4 m_SelectionColor = {0.50f, 0.35f, 1.0f, 0.40f};
    Vector4 m_HoverColor = {0.15f, 0.15f, 0.18f, 1.0f};
    Vector4 m_TextColor = {0.85f, 0.85f, 0.85f, 1.0f};
    Vector4 m_TextHoverColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 m_TextSelectionColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 m_ArrowColor = {0.70f, 0.70f, 0.70f, 1.0f};

    Font* m_Font = nullptr;
    bool m_Editable = false;
    bool m_MultipleSelection = false;
    bool m_VScrollbarEnabled = true;
    bool m_HScrollbarEnabled = true;

    UIScrollbar* m_VScrollbar = nullptr;
    UIScrollbar* m_HScrollbar = nullptr;
    UITextInput* m_EditInput = nullptr;
    UITreeViewItem* m_EditingItem = nullptr;
    std::string m_EditOldText;
    // Deferred deletion for the edit input — deleting inside TreeEditInput::OnKeyDown/OnBlur
    // (i.e. inside its own callback) is use-after-free; defer until next layout.
    UITextInput* m_PendingDeleteInput = nullptr;
    bool m_EditCommitting = false;
    void ProcessPendingEditDeletion();

    Vector2 m_ScrollOffset = {0.0f, 0.0f};

    // Drag state
    bool m_Dragging = false;
    Vector2 m_DragStartPos = {0.0f, 0.0f};
    std::vector<UITreeViewItem*> m_DragItems; // all selected when dragging, else single
    UITreeViewItem* m_DragItem = nullptr; // primary dragged
    DropTarget m_DropTarget;
    UILabel* m_GhostLabel = nullptr; // translucent text overlay (IsOverlayLayer)
    Vector2 m_GhostPos = {0.0f, 0.0f}; // authoritative ghost position (re-applied in OnLayoutComputed)
    // Drop feedback: a 2px line at the row edge (Below = reorder) or a translucent
    // fill over the target row (Onto = nest). Positioned in OnLayoutComputed so the
    // ComputeFreeLayout accumulation never drifts it.
    UIPanel* m_DropIndicator = nullptr;

    // Double-click detection
    int m_LastClickFrame = -1000;
    Vector2 m_LastClickPos = {0.0f, 0.0f};
    UITreeViewItem* m_LastClickItem = nullptr;

    // Hover
    UITreeViewItem* m_HoveredItem = nullptr;

    // Events
    std::function<void(int)> m_OnSelectedIndexChanged;
    std::function<void(UITreeViewItem*)> m_OnSelectedItemChanged;
    std::function<void(const std::vector<UITreeViewItem*>&)> m_OnSelectedItemsChanged;
    std::function<void(UITreeViewItem*)> m_OnItemExpanded;
    std::function<void(UITreeViewItem*)> m_OnItemCollapsed;
    std::function<void(UITreeViewItem*, UITreeViewItem*, int)> m_OnItemDragged;
    std::function<void(UITreeViewItem*)> m_OnItemDoubleClicked;
    std::function<void(UITreeViewItem*, const std::string&, const std::string&)> m_OnItemRenamed;
};

} // namespace Leir
