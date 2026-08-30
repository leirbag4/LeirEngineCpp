#pragma once

/**
 * @file UITreeViewItem.h
 * @brief Single row in a UITreeView: text, icon, selection, expansion and tree links.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector4.h"
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class Font;
class UILabel;
class UIImage;
class Texture2D;

/**
 * @brief Single row in a UITreeView: text, icon, selection and tree structure.
 * @ingroup UI
 * @details Holds text, enabled/selected/expanded state and tree parent/children.
 *  The UITreeView drives layout and rendering.
 */
class LEIR_API UITreeViewItem : public UIPanel {
public:
    /**
     * @brief Constructs an empty tree item.
     */
    UITreeViewItem();

    /**
     * @brief Destroys the tree item.
     */
    ~UITreeViewItem() override;

    /**
     * @brief Sets the row text.
     * @param[in] text Text to display.
     */
    void SetText(const std::string& text);

    /**
     * @brief Returns the text.
     * @return Text string.
     */
    const std::string& GetText() const { return m_Text; }

    /**
     * @brief Sets the icon texture.
     * @param[in] icon Icon texture (shared, from UITextureCache).
     */
    void SetIcon(std::shared_ptr<Texture2D> icon);

    /**
     * @brief Sets whether icon is shown.
     * @param[in] show True to show icon.
     */
    void SetShowIcon(bool show);

    /**
     * @brief Sets icon size.
     * @param[in] size Size in logical pixels.
     */
    void SetIconSize(float size) { m_IconSize = size; }

    /**
     * @brief Returns whether an icon is set.
     * @return True if icon exists.
     */
    bool HasIcon() const { return m_Icon != nullptr; }

    /**
     * @brief Sets enabled state (grayed, not selectable).
     * @param[in] enabled True to enable.
     */
    void SetItemEnabled(bool enabled);

    /**
     * @brief Returns enabled state.
     * @return True if enabled.
     */
    bool IsItemEnabled() const { return m_ItemEnabled; }

    /**
     * @brief Sets selected state.
     * @param[in] selected True to select.
     */
    void SetSelected(bool selected);

    /**
     * @brief Returns selected state.
     * @return True if selected.
     */
    bool IsSelected() const { return m_Selected; }

    /**
     * @brief Sets expanded state (children visible in flat traversal).
     * @param[in] expanded True to expand.
     */
    void SetExpanded(bool expanded);

    /**
     * @brief Returns expanded state.
     * @return True if expanded.
     */
    bool IsExpanded() const { return m_Expanded; }

    /**
     * @brief Sets indent per level.
     * @param[in] indent Indent in logical pixels.
     */
    void SetIndent(float indent) { m_Indent = indent; }

    /**
     * @brief Returns indent per level.
     * @return Indent.
     */
    float GetIndent() const { return m_Indent; }

    /**
     * @brief Returns depth in the tree (0 = root).
     * @return Depth.
     */
    int GetDepth() const;

    /**
     * @brief Returns tree parent.
     * @return Parent pointer or nullptr if root.
     */
    UITreeViewItem* GetTreeParent() const { return m_TreeParent; }

    /**
     * @brief Returns tree children.
     * @return Children vector.
     */
    const std::vector<UITreeViewItem*>& GetTreeChildren() const { return m_TreeChildren; }

    /**
     * @brief Adds a tree child.
     * @param[in] child Child to add.
     */
    void AddTreeChild(UITreeViewItem* child);

    /**
     * @brief Removes a tree child.
     * @param[in] child Child to remove.
     */
    void RemoveTreeChild(UITreeViewItem* child);

    /**
     * @brief Inserts a tree child at a specific index.
     * @param[in] child Child to insert.
     * @param[in] index Position.
     */
    void InsertTreeChildAt(UITreeViewItem* child, size_t index);

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
     * @brief Sets font.
     * @param[in] font Font pointer (not owned).
     */
    void SetFont(Font* font);

    /**
     * @brief Returns font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Returns whether the view owns the child.
     * @param[in] child Child to query.
     * @return True if owned.
     */
    bool OwnsChild(const UIElement* child) const override;

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Returns cached text width for virtualization.
     * @return Text width.
     */
    float GetCachedTextWidth() const { return m_CachedTextWidth; }

    /**
     * @brief Invalidates cached text width.
     */
    void InvalidateTextWidth() { m_TextWidthDirty = true; }

    /**
     * @brief Sets hovered state (driven by parent tree).
     * @param[in] hovered True if hovered.
     */
    void SetTreeHovered(bool hovered);

    /**
     * @brief Sets filtered flag (transient view-state).
     * @param[in] filtered True if filtered (hidden).
     */
    void SetTreeFiltered(bool filtered) { m_TreeFiltered = filtered; }

    /**
     * @brief Returns filtered flag.
     * @return True if filtered.
     */
    bool IsTreeFiltered() const { return m_TreeFiltered; }

    /**
     * @brief Sets filter-excluded flag (never matches by own text).
     * @param[in] excluded True to exclude from text matching.
     */
    void SetFilterExcluded(bool excluded) { m_FilterExcluded = excluded; }

    /**
     * @brief Returns filter-excluded flag.
     * @return True if excluded.
     */
    bool IsFilterExcluded() const { return m_FilterExcluded; }

protected:
    /**
     * @brief Called after layout.
     */
    void OnLayoutComputed() override;

    /**
     * @brief Called when pointer enters.
     * @param[in] pos Pointer position.
     */
    void OnPointerEnter(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

    /**
     * @brief Called on pointer move.
     * @param[in] pos Pointer position.
     */
    void OnPointerMove(const Vector2& pos) override;

private:
    void RebuildLabels();
    void UpdateColors();
    void UpdateIconState();

    std::string m_Text;                                     ///< Row text.
    Font* m_Font = nullptr;                                 ///< Font (not owned).
    UILabel* m_ArrowLabel = nullptr;                        ///< Arrow label (owned).
    UILabel* m_TextLabel = nullptr;                         ///< Text label (owned).
    UIImage* m_IconImage = nullptr;                         ///< Icon image (owned).
    std::shared_ptr<Texture2D> m_Icon;                      ///< Icon texture.
    bool m_ShowIcon = false;                                ///< Show icon flag.
    float m_IconSize = 12.0f;                               ///< Icon size.

    bool m_ItemEnabled = true;                              ///< Enabled flag.
    bool m_Selected = false;                                ///< Selected flag.
    bool m_TreeHovered = false;                             ///< Hovered flag (tree-driven).
    bool m_TreeFiltered = false;                            ///< Filtered flag.
    bool m_FilterExcluded = false;                          ///< Filter-excluded flag.
    bool m_Expanded = true;                                 ///< Expanded flag.
    float m_Indent = 16.0f;                                 ///< Indent per level.

    UITreeViewItem* m_TreeParent = nullptr;                 ///< Tree parent.
    std::vector<UITreeViewItem*> m_TreeChildren;            ///< Tree children.

    Vector4 m_SelectionColor = {0.50f, 0.35f, 1.0f, 0.40f};   ///< Selection background.
    Vector4 m_HoverColor = {0.15f, 0.15f, 0.18f, 1.0f};       ///< Hover background.
    Vector4 m_TextColor = {0.85f, 0.85f, 0.85f, 1.0f};        ///< Text color.
    Vector4 m_TextHoverColor = {1.0f, 1.0f, 1.0f, 1.0f};      ///< Text hover color.
    Vector4 m_TextSelectionColor = {1.0f, 1.0f, 1.0f, 1.0f};  ///< Text selection color.
    Vector4 m_ArrowColor = {0.70f, 0.70f, 0.70f, 1.0f};       ///< Arrow color.

    mutable float m_CachedTextWidth = 0.0f;                 ///< Cached text width.
    mutable bool m_TextWidthDirty = true;                   ///< Text width dirty flag.
};

} // namespace Leir
