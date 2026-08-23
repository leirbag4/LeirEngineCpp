#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>

namespace Leir {

class Font;
class UILabel;

// Single row in a UITreeView. Holds text, enabled/selected/expanded state,
// tree parent/children, indent and per-item colors. The UITreeView owns the
// tree structure (see UITreeView::AddItem) and drives layout/render.
class LEIR_API UITreeViewItem : public UIPanel {
public:
    UITreeViewItem();
    ~UITreeViewItem() override;

    // Text
    void SetText(const std::string& text);
    const std::string& GetText() const { return m_Text; }

    // Enabled — false = grayed, not selectable, OnPointerDown returns false
    void SetItemEnabled(bool enabled);
    bool IsItemEnabled() const { return m_ItemEnabled; }

    // Selection
    void SetSelected(bool selected);
    bool IsSelected() const { return m_Selected; }

    // Expanded — whether children are visible in the flat traversal
    void SetExpanded(bool expanded);
    bool IsExpanded() const { return m_Expanded; }

    // Indent per level (px). Default 16. Overridden by UITreeView::SetIndent when the
    // item belongs to a tree.
    void SetIndent(float indent) { m_Indent = indent; }
    float GetIndent() const { return m_Indent; }

    // Depth in the tree (0 = root). Computed from m_TreeParent chain.
    int GetDepth() const;

    // Tree structure — managed by UITreeView (AddItem/RemoveItem). Direct use is
    // allowed but prefer going through UITreeView to keep flat cache coherent.
    UITreeViewItem* GetTreeParent() const { return m_TreeParent; }
    const std::vector<UITreeViewItem*>& GetTreeChildren() const { return m_TreeChildren; }
    void AddTreeChild(UITreeViewItem* child);
    void RemoveTreeChild(UITreeViewItem* child);
    void InsertTreeChildAt(UITreeViewItem* child, size_t index);

    // Appearance — defaults match the engine dark theme
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

    bool OwnsChild(const UIElement* child) const override;

    Vector2 GetMinSize() const override;

    // Cached text width for virtualization (invalidated on SetText/SetFont)
    float GetCachedTextWidth() const { return m_CachedTextWidth; }
    void InvalidateTextWidth() { m_TextWidthDirty = true; }

    // Hover is driven by parent UITreeView (full-width row), not by canvas IsHovered().
    // UITreeView calls this when the row under the cursor changes; it works even
    // when the hit is TreeText/TreeArrow (deepest child) and clears when leaving.
    void SetTreeHovered(bool hovered);

protected:
    void OnLayoutComputed() override;
    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;
    void OnPointerMove(const Vector2& pos) override;

private:
    void RebuildLabels();
    void UpdateColors();

    std::string m_Text;
    Font* m_Font = nullptr;
    UILabel* m_ArrowLabel = nullptr;
    UILabel* m_TextLabel = nullptr;

    bool m_ItemEnabled = true;
    bool m_Selected = false;
    bool m_TreeHovered = false;
    bool m_Expanded = true;
    float m_Indent = 16.0f;

    UITreeViewItem* m_TreeParent = nullptr;
    std::vector<UITreeViewItem*> m_TreeChildren;

    Vector4 m_SelectionColor = {0.30f, 0.50f, 1.0f, 0.40f};
    Vector4 m_HoverColor = {0.15f, 0.15f, 0.18f, 1.0f};
    Vector4 m_TextColor = {0.85f, 0.85f, 0.85f, 1.0f};
    Vector4 m_TextHoverColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 m_TextSelectionColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 m_ArrowColor = {0.70f, 0.70f, 0.70f, 1.0f};

    mutable float m_CachedTextWidth = 0.0f;
    mutable bool m_TextWidthDirty = true;
};

} // namespace Leir
