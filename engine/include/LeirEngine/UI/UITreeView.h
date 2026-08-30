#pragma once

/**
 * @file UITreeView.h
 * @brief Virtualized tree view with rows, selection, drag&drop and inline editing.
 * @ingroup UI
 */

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

/**
 * @brief Virtualized tree view with full-width rows, indent, expand/collapse, selection, drag&drop and inline editing.
 * @ingroup UI
 * @details Owns its scrollbars and edit input (OwnsChild). Tree items are owned by the caller but the view tracks roots and a flat cache of visible items for virtualization.
 */
class LEIR_API UITreeView : public UIElement {
public:
    /**
     * @brief Constructs an empty tree view.
     */
    UITreeView();

    /**
     * @brief Destroys the tree view and its scrollbars.
     */
    ~UITreeView() override;

    /**
     * @brief Drop mode for 3-zone row hit-testing.
     * @ingroup UI
     */
    enum class DropMode { None,  ///< No drop.
                          Onto,  ///< Center = nest into target.
                          Above, ///< Top edge = insert before target.
                          Below  ///< Bottom edge = insert after target.
    };

    /**
     * @brief Drop target: item under cursor and its zone.
     * @ingroup UI
     */
    struct DropTarget { UITreeViewItem* item = nullptr; ///< Target item.
                        DropMode mode = DropMode::None; ///< Drop zone.
    };

    /**
     * @brief Returns whether the view owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if owned (scrollbars, edit input, viewport).
     */
    bool OwnsChild(const UIElement* child) const override;

    /**
     * @brief Adds an item (optionally as child of a parent).
     * @param[in] item Item to add.
     * @param[in] parent Parent item or nullptr for root.
     */
    void AddItem(UITreeViewItem* item, UITreeViewItem* parent = nullptr);

    /**
     * @brief Removes an item from the tree (does not delete it).
     * @param[in] item Item to remove.
     */
    void RemoveItem(UITreeViewItem* item);

    /**
     * @brief Clears all items (detaches but does not delete items).
     */
    void ClearItems();

    /**
     * @brief Returns flat visible item count.
     * @return Count.
     */
    int GetItemCount() const;

    /**
     * @brief Returns visible item at an index.
     * @param[in] visibleIndex Flat visible index.
     * @return Item pointer or nullptr.
     */
    UITreeViewItem* GetItemAt(int visibleIndex) const;

    /**
     * @brief Returns root items.
     * @return Roots vector.
     */
    const std::vector<UITreeViewItem*>& GetRoots() const { return m_Roots; }

    /**
     * @brief Enables multiple selection.
     * @param[in] enabled True to enable multi-select.
     */
    void SetMultipleSelectionEnabled(bool enabled) { m_MultipleSelection = enabled; }

    /**
     * @brief Returns whether multiple selection is enabled.
     * @return True if enabled.
     */
    bool IsMultipleSelectionEnabled() const { return m_MultipleSelection; }

    /**
     * @brief Returns selected flat index (single selection).
     * @return Index or -1 if none.
     */
    int GetSelectedIndex() const;

    /**
     * @brief Sets selected index (single selection).
     * @param[in] index Flat visible index.
     */
    void SetSelectedIndex(int index);

    /**
     * @brief Returns selected item (single selection).
     * @return Item or nullptr.
     */
    UITreeViewItem* GetSelectedItem() const;

    /**
     * @brief Sets selected item (single selection).
     * @param[in] item Item to select or nullptr to clear.
     */
    void SetSelectedItem(UITreeViewItem* item);

    /**
     * @brief Returns all selected items (multi-selection).
     * @return Vector of selected items.
     */
    std::vector<UITreeViewItem*> GetSelectedItems() const { return m_SelectedItems; }

    /**
     * @brief Sets selected items (multi-selection).
     * @param[in] items Items to select.
     */
    void SetSelectedItems(const std::vector<UITreeViewItem*>& items);

    /**
     * @brief Sets indent per tree level.
     * @param[in] indent Indent in logical pixels (default 16).
     */
    void SetIndent(float indent);

    /**
     * @brief Returns indent per level.
     * @return Indent.
     */
    float GetIndent() const { return m_Indent; }

    /**
     * @brief Sets row height.
     * @param[in] h Row height in logical pixels (default 20).
     */
    void SetRowHeight(float h) { m_RowHeight = h; }

    /**
     * @brief Returns row height.
     * @return Row height.
     */
    float GetRowHeight() const { return m_RowHeight; }

    /**
     * @brief Enables icon slot (12×12) to the left of each row's text.
     * @param[in] enabled True to enable icons.
     */
    void SetIconsEnabled(bool enabled);

    /**
     * @brief Returns whether icons are enabled.
     * @return True if enabled.
     */
    bool AreIconsEnabled() const { return m_IconsEnabled; }

    /**
     * @brief Sets icon size.
     * @param[in] size Size in logical pixels.
     */
    void SetIconSize(float size);

    /**
     * @brief Returns icon size.
     * @return Size.
     */
    float GetIconSize() const { return m_IconSize; }

    /**
     * @brief Sets selection background color.
     * @param[in] c Color.
     */
    void SetSelectionColor(const Vector4& c) { m_SelectionColor = c; }

    /**
     * @brief Returns selection color.
     * @return Color.
     */
    Vector4 GetSelectionColor() const { return m_SelectionColor; }

    /**
     * @brief Sets hover background color.
     * @param[in] c Color.
     */
    void SetHoverColor(const Vector4& c) { m_HoverColor = c; }

    /**
     * @brief Returns hover color.
     * @return Color.
     */
    Vector4 GetHoverColor() const { return m_HoverColor; }

    /**
     * @brief Sets text color.
     * @param[in] c Color.
     */
    void SetTextColor(const Vector4& c) { m_TextColor = c; }

    /**
     * @brief Returns text color.
     * @return Color.
     */
    Vector4 GetTextColor() const { return m_TextColor; }

    /**
     * @brief Sets text hover color.
     * @param[in] c Color.
     */
    void SetTextHoverColor(const Vector4& c) { m_TextHoverColor = c; }

    /**
     * @brief Returns text hover color.
     * @return Color.
     */
    Vector4 GetTextHoverColor() const { return m_TextHoverColor; }

    /**
     * @brief Sets text selection color.
     * @param[in] c Color.
     */
    void SetTextSelectionColor(const Vector4& c) { m_TextSelectionColor = c; }

    /**
     * @brief Returns text selection color.
     * @return Color.
     */
    Vector4 GetTextSelectionColor() const { return m_TextSelectionColor; }

    /**
     * @brief Sets arrow color.
     * @param[in] c Color.
     */
    void SetArrowColor(const Vector4& c) { m_ArrowColor = c; }

    /**
     * @brief Returns arrow color.
     * @return Color.
     */
    Vector4 GetArrowColor() const { return m_ArrowColor; }

    /**
     * @brief Sets font for rows.
     * @param[in] font Font pointer (not owned).
     */
    void SetFont(Font* font);

    /**
     * @brief Returns font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Commits or cancels inline edit (called by edit input).
     * @param[in] cancel True to cancel (Escape), false to commit (Enter/Blur).
     */
    void CommitEdit(bool cancel);

    /**
     * @brief Sets editable flag (F2 opens inline UITextInput).
     * @param[in] editable True to make editable.
     */
    void SetEditable(bool editable) { m_Editable = editable; }

    /**
     * @brief Returns editable flag.
     * @return True if editable.
     */
    bool IsEditable() const { return m_Editable; }

    /**
     * @brief Sets filter (Godot-style, case-insensitive substring, no rebuild).
     * @param[in] filter Filter text (empty = clear).
     */
    void SetFilter(const std::string& filter);

    /**
     * @brief Enables vertical scrollbar.
     * @param[in] e True to enable.
     */
    void SetVerticalScrollbarEnabled(bool e);

    /**
     * @brief Returns whether vertical scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsVerticalScrollbarEnabled() const { return m_VScrollbarEnabled; }

    /**
     * @brief Enables horizontal scrollbar.
     * @param[in] e True to enable.
     */
    void SetHorizontalScrollbarEnabled(bool e);

    /**
     * @brief Returns whether horizontal scrollbar is enabled.
     * @return True if enabled.
     */
    bool IsHorizontalScrollbarEnabled() const { return m_HScrollbarEnabled; }

    /**
     * @brief Returns content size for scrolling.
     * @return Content size.
     */
    Vector2 GetContentSize() const override;

    /**
     * @brief Returns viewport size (content area minus scrollbar strips).
     * @return Viewport size.
     */
    Vector2 GetViewportSize() const;

    /**
     * @brief Sets scroll offset.
     * @param[in] offset Offset (x,y).
     */
    void SetScrollOffset(Vector2 offset);

    /**
     * @brief Returns scroll offset.
     * @return Offset.
     */
    Vector2 GetScrollOffset() const { return m_ScrollOffset; }

    /**
     * @brief Sets callback for selected index changed.
     * @param[in] cb Callback with index.
     */
    void SetOnSelectedIndexChanged(std::function<void(int)> cb) { m_OnSelectedIndexChanged = std::move(cb); }

    /**
     * @brief Sets callback for selected item changed.
     * @param[in] cb Callback with item.
     */
    void SetOnSelectedItemChanged(std::function<void(UITreeViewItem*)> cb) { m_OnSelectedItemChanged = std::move(cb); }

    /**
     * @brief Sets callback for selected items changed (multi).
     * @param[in] cb Callback with items.
     */
    void SetOnSelectedItemsChanged(std::function<void(const std::vector<UITreeViewItem*>&)> cb) { m_OnSelectedItemsChanged = std::move(cb); }

    /**
     * @brief Sets callback for item expanded.
     * @param[in] cb Callback with item.
     */
    void SetOnItemExpanded(std::function<void(UITreeViewItem*)> cb) { m_OnItemExpanded = std::move(cb); }

    /**
     * @brief Sets callback for item collapsed.
     * @param[in] cb Callback with item.
     */
    void SetOnItemCollapsed(std::function<void(UITreeViewItem*)> cb) { m_OnItemCollapsed = std::move(cb); }

    /**
     * @brief Sets drag&drop callback (called before tree mutates).
     * @details Returns whether the scene accepted the change; tree only mutates when true.
     * @param[in] cb Callback with draggedItems, targetItem and mode (Onto/Above/Below).
     */
    void SetOnItemDragged(std::function<bool(const std::vector<UITreeViewItem*>& draggedItems,
        UITreeViewItem* targetItem, DropMode mode)> cb) { m_OnItemDragged = std::move(cb); }

    /**
     * @brief Sets double-click callback.
     * @param[in] cb Callback with item.
     */
    void SetOnItemDoubleClicked(std::function<void(UITreeViewItem*)> cb) { m_OnItemDoubleClicked = std::move(cb); }

    /**
     * @brief Sets rename callback.
     * @param[in] cb Callback with item, oldText and newText.
     */
    void SetOnItemRenamed(std::function<void(UITreeViewItem*, const std::string& oldText, const std::string& newText)> cb) { m_OnItemRenamed = std::move(cb); }

    /**
     * @brief Handles pointer press.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Handles pointer move.
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

    /**
     * @brief Handles pointer release.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerUp(const Vector2& pos) override;

    /**
     * @brief Handles wheel scroll.
     * @param[in] delta Scroll delta.
     * @return True if consumed.
     */
    bool OnScroll(float delta) override;

    /**
     * @brief Handles key down (navigation, F2 edit, etc.).
     * @param[in] key Key code.
     * @return True if consumed.
     */
    bool OnKeyDown(int key) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

    /**
     * @brief Returns hovered item.
     * @return Hovered item or nullptr.
     */
    UITreeViewItem* GetHoveredItem() const { return m_HoveredItem; }

    /**
     * @brief Notifies that an item is hovered (called by item).
     * @param[in] item Hovered item.
     */
    void NotifyItemHovered(UITreeViewItem* item);

protected:
    /**
     * @brief Called after layout to sync scrollbars and edit rect.
     */
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
    DropTarget HitTestDropTarget(const Vector2& pos) const;
    void ClearDropHighlight();
    bool FilterMatches(const std::string& text) const;
    bool ComputeFilterVisibility(UITreeViewItem* item);

    std::vector<UITreeViewItem*> m_Roots;                   ///< Root items.
    mutable std::vector<UITreeViewItem*> m_FlatVisible;     ///< Flat visible cache.
    mutable bool m_FlatDirty = true;                        ///< Flat cache dirty.
    mutable float m_CachedMaxWidth = 0.0f;                  ///< Cached max width.
    mutable bool m_CachedMaxWidthDirty = true;              ///< Max width dirty.

    std::vector<UITreeViewItem*> m_SelectedItems;           ///< Selected items.
    int m_LastSelectedIndex = -1;                           ///< Last selected flat index for Shift range.
    UITreeViewItem* m_DeferredSelect = nullptr;             ///< Deferred single-select on pointer up.

    float m_Indent = 16.0f;                                 ///< Indent per level.
    float m_RowHeight = 20.0f;                              ///< Row height.
    bool m_IconsEnabled = false;                            ///< Icons enabled flag.
    float m_IconSize = 12.0f;                               ///< Icon size.

    Vector4 m_SelectionColor = {0.50f, 0.35f, 1.0f, 0.40f};   ///< Selection background.
    Vector4 m_HoverColor = {0.15f, 0.15f, 0.18f, 1.0f};       ///< Hover background.
    Vector4 m_TextColor = {0.85f, 0.85f, 0.85f, 1.0f};        ///< Text color.
    Vector4 m_TextHoverColor = {1.0f, 1.0f, 1.0f, 1.0f};      ///< Text hover color.
    Vector4 m_TextSelectionColor = {1.0f, 1.0f, 1.0f, 1.0f};  ///< Text selection color.
    Vector4 m_ArrowColor = {0.70f, 0.70f, 0.70f, 1.0f};       ///< Arrow color.

    Font* m_Font = nullptr;                                 ///< Font (not owned).
    bool m_Editable = false;                                ///< Editable flag.
    bool m_MultipleSelection = false;                       ///< Multi-selection flag.
    std::string m_Filter;                                   ///< Lowercased filter text.
    bool m_VScrollbarEnabled = true;                        ///< Vertical scrollbar flag.
    bool m_HScrollbarEnabled = true;                        ///< Horizontal scrollbar flag.

    UIScrollbar* m_VScrollbar = nullptr;                    ///< Vertical scrollbar (owned).
    UIScrollbar* m_HScrollbar = nullptr;                    ///< Horizontal scrollbar (owned).
    UIPanel* m_Viewport = nullptr;                          ///< Viewport container (owned).
    UITextInput* m_EditInput = nullptr;                     ///< Inline edit input (owned).
    UITreeViewItem* m_EditingItem = nullptr;                ///< Item being edited.
    std::string m_EditOldText;                              ///< Text before edit.
    UITextInput* m_PendingDeleteInput = nullptr;            ///< Deferred delete of edit input.
    bool m_EditCommitting = false;                          ///< Committing flag.
    void ProcessPendingEditDeletion();

    Vector2 m_ScrollOffset = {0.0f, 0.0f};                   ///< Scroll offset.

    bool m_Dragging = false;                                ///< Dragging flag.
    Vector2 m_DragStartPos = {0.0f, 0.0f};                   ///< Drag start position.
    std::vector<UITreeViewItem*> m_DragItems;               ///< Dragged items.
    UITreeViewItem* m_DragItem = nullptr;                   ///< Primary dragged item.
    DropTarget m_DropTarget;                                ///< Current drop target.
    UILabel* m_GhostLabel = nullptr;                        ///< Ghost label (overlay).
    Vector2 m_GhostPos = {0.0f, 0.0f};                      ///< Ghost position.
    UIPanel* m_DropIndicator = nullptr;                     ///< Drop indicator (owned).

    int m_LastClickFrame = -1000;                           ///< Last click frame for double-click.
    Vector2 m_LastClickPos = {0.0f, 0.0f};                  ///< Last click position.
    UITreeViewItem* m_LastClickItem = nullptr;              ///< Last clicked item.

    UITreeViewItem* m_HoveredItem = nullptr;                ///< Hovered item.

    std::function<void(int)> m_OnSelectedIndexChanged;      ///< Selected index changed callback.
    std::function<void(UITreeViewItem*)> m_OnSelectedItemChanged; ///< Selected item changed callback.
    std::function<void(const std::vector<UITreeViewItem*>&)> m_OnSelectedItemsChanged; ///< Selected items changed callback.
    std::function<void(UITreeViewItem*)> m_OnItemExpanded;  ///< Item expanded callback.
    std::function<void(UITreeViewItem*)> m_OnItemCollapsed; ///< Item collapsed callback.
    std::function<bool(const std::vector<UITreeViewItem*>&, UITreeViewItem*, DropMode)> m_OnItemDragged; ///< Drag callback.
    std::function<void(UITreeViewItem*)> m_OnItemDoubleClicked; ///< Double-click callback.
    std::function<void(UITreeViewItem*, const std::string&, const std::string&)> m_OnItemRenamed; ///< Rename callback.
};

} // namespace Leir
