#include "LeirEngine/UI/UITreeViewItem.h"
#include "LeirEngine/UI/UITreeView.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UIImage.h"
#include "LeirEngine/UI/Font.h"

namespace Leir {

UITreeViewItem::UITreeViewItem()
{
    SetName("TreeViewItem");
    SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    SetLayoutMode(LayoutMode::Free);
    SetSizePolicy(SizePolicy::Fixed);

    m_ArrowLabel = new UILabel();
    m_ArrowLabel->SetName("TreeArrow");
    m_ArrowLabel->SetText("");
    m_ArrowLabel->SetFontSize(10);
    m_ArrowLabel->SetSizePolicy(SizePolicy::Fixed);
    m_ArrowLabel->SetColor({0.70f, 0.70f, 0.70f, 1.0f});
    m_ArrowLabel->SetHitTestable(false);
    AddChild(m_ArrowLabel);

    m_TextLabel = new UILabel();
    m_TextLabel->SetName("TreeText");
    m_TextLabel->SetText("");
    m_TextLabel->SetFontSize(13);
    m_TextLabel->SetSizePolicy(SizePolicy::Fixed);
    m_TextLabel->SetHitTestable(false);
    AddChild(m_TextLabel);

    // Icon quad (12x12) between the arrow and the text. Hidden until SetIcon +
    // SetShowIcon(true) (the UITreeView's SetIconsEnabled drives the latter).
    m_IconImage = new UIImage();
    m_IconImage->SetName("TreeIcon");
    m_IconImage->SetActive(false);
    m_IconImage->SetHitTestable(false);
    AddChild(m_IconImage);

    RebuildLabels();
}

UITreeViewItem::~UITreeViewItem()
{
    if (m_ArrowLabel) { RemoveChild(m_ArrowLabel); delete m_ArrowLabel; }
    if (m_TextLabel) { RemoveChild(m_TextLabel); delete m_TextLabel; }
    if (m_IconImage) { RemoveChild(m_IconImage); delete m_IconImage; }
}

void UITreeViewItem::SetText(const std::string& text)
{
    m_Text = text;
    m_TextWidthDirty = true;
    if (m_TextLabel) m_TextLabel->SetText(text);
}

void UITreeViewItem::SetItemEnabled(bool enabled)
{
    m_ItemEnabled = enabled;
    UpdateColors();
}

void UITreeViewItem::SetSelected(bool selected)
{
    m_Selected = selected;
    UpdateColors();
}

void UITreeViewItem::SetExpanded(bool expanded)
{
    if (m_Expanded == expanded) return;
    m_Expanded = expanded;
    RebuildLabels();
}

int UITreeViewItem::GetDepth() const
{
    int d = 0;
    const UITreeViewItem* p = m_TreeParent;
    while (p) { ++d; p = p->m_TreeParent; }
    return d;
}

void UITreeViewItem::AddTreeChild(UITreeViewItem* child)
{
    if (!child) return;
    if (child->m_TreeParent) child->m_TreeParent->RemoveTreeChild(child);
    child->m_TreeParent = this;
    m_TreeChildren.push_back(child);
    RebuildLabels();
}

void UITreeViewItem::RemoveTreeChild(UITreeViewItem* child)
{
    if (!child) return;
    for (auto it = m_TreeChildren.begin(); it != m_TreeChildren.end(); ++it) {
        if (*it == child) {
            m_TreeChildren.erase(it);
            child->m_TreeParent = nullptr;
            break;
        }
    }
    RebuildLabels();
}

void UITreeViewItem::InsertTreeChildAt(UITreeViewItem* child, size_t index)
{
    if (!child) return;
    if (child->m_TreeParent) child->m_TreeParent->RemoveTreeChild(child);
    child->m_TreeParent = this;
    if (index > m_TreeChildren.size()) index = m_TreeChildren.size();
    m_TreeChildren.insert(m_TreeChildren.begin() + (long)index, child);
    RebuildLabels();
}

void UITreeViewItem::SetFont(Font* font)
{
    m_Font = font;
    if (m_ArrowLabel) m_ArrowLabel->SetFont(font);
    if (m_TextLabel) m_TextLabel->SetFont(font);
    m_TextWidthDirty = true;
}

bool UITreeViewItem::OwnsChild(const UIElement* child) const
{
    return child == m_ArrowLabel || child == m_TextLabel || child == m_IconImage;
}

Vector2 UITreeViewItem::GetMinSize() const
{
    if (auto ov = GetMinSizeOverride()) return *ov;
    return {60.0f, 20.0f};
}

void UITreeViewItem::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
    // Manual Free layout for arrow + text: arrow at depth*indent, text after arrow.
    // Row layout was causing vertical misalignment and zero-width for text when font
    // was not yet propagated, so we position explicitly.
    const auto& cr = GetComputedRect();
    float arrowW = 12.0f;
    float arrowH = 12.0f;
    float indent = (float)GetDepth() * m_Indent;
    // Children are positioned RELATIVE to this item and parentOffset={cr.xy} is
    // passed so their m_ComputedRect lands on absolute coords (the item's own
    // Free layout passes the same parentOffset — no transient double-counting;
    // see TODO_COMPUTE_FREE_LAYOUT_FIX.md).
    // Arrow centered vertically in row
    float arrowTop = (cr.w - arrowH) * 0.5f;
    if (m_ArrowLabel) {
        // TreeView does a manual double-layout: first a stale Free pass (item at
        // 0x0 or double-x) builds glyphs with wrong height, then the correct
        // manual pass in TreeView::OnLayoutComputed fixes the rect. Only force a
        // rebuild when the rect actually changed — unconditional Invalidate() every
        // frame caused ~40 * 60 rebuilds/s and dropped FPS to ~10.
        float prevW = m_ArrowLabel->GetComputedRect().z;
        float prevH = m_ArrowLabel->GetComputedRect().w;
        if (prevW != arrowW || prevH != arrowH) m_ArrowLabel->Invalidate();
        m_ArrowLabel->GetRect().anchor = {0, 0, 0, 0};
        m_ArrowLabel->GetRect().offset = {indent, arrowTop, indent + arrowW, arrowTop + arrowH};
        m_ArrowLabel->ComputeLayout({arrowW, arrowH}, {cr.x, cr.y});
    }
    // Cursor starts AFTER the reserved arrow slot (12px) + gap, so leaves align
    // with nodes that have children (a leaf at depth N starts where depth N's
    // arrow slot would be). Previously leaves used only +4 -> barely indented.
    float cursor = indent + arrowW + 4.0f;
    // Icon (12x12) between the arrow and the text, vertically centered.
    if (m_ShowIcon && m_IconImage && m_IconImage->IsActive()) {
        float iconTop = (cr.w - m_IconSize) * 0.5f;
        m_IconImage->GetRect().anchor = {0, 0, 0, 0};
        m_IconImage->GetRect().offset = {cursor, iconTop, cursor + m_IconSize, iconTop + m_IconSize};
        m_IconImage->ComputeLayout({m_IconSize, m_IconSize}, {cr.x, cr.y});
        cursor += m_IconSize + 4.0f;
    }
    float textW = std::max(0.0f, cr.z - cursor - 2.0f);
    float textH = cr.w;
    if (m_TextLabel) {
        float prevW = m_TextLabel->GetComputedRect().z;
        float prevH = m_TextLabel->GetComputedRect().w;
        if (prevW != textW || prevH != textH) m_TextLabel->Invalidate();
        m_TextLabel->GetRect().anchor = {0, 0, 0, 0};
        m_TextLabel->GetRect().offset = {cursor, 0.0f, cursor + textW, textH};
        m_TextLabel->ComputeLayout({textW, textH}, {cr.x, cr.y});
    }
}

void UITreeViewItem::SetTreeHovered(bool hovered)
{
    if (m_TreeHovered == hovered) return;
    m_TreeHovered = hovered;
    UpdateColors();
}

void UITreeViewItem::OnPointerEnter(const Vector2&)
{
    // Hover is driven by parent UITreeView (SetTreeHovered), not by canvas IsHovered().
    // The hit is often TreeText/TreeArrow (deepest child), so IsHovered() on the
    // item is unreliable and would leave stale hover when leaving toward another row.
}

void UITreeViewItem::OnPointerExit()
{
    // See OnPointerEnter.
}

void UITreeViewItem::OnPointerMove(const Vector2& pos)
{
    // Forward full-width row hover to the owning TreeView (B). The item is now a
    // child of the internal clipped TreeViewport, so walk up to find the tree.
    for (UIElement* p = GetParent(); p; p = p->GetParent()) {
        if (auto* tv = dynamic_cast<UITreeView*>(p)) {
            tv->NotifyItemHovered(this);
            break;
        }
    }
    (void)pos;
}

void UITreeViewItem::SetIcon(std::shared_ptr<Texture2D> icon)
{
    m_Icon = std::move(icon);
    UpdateIconState();
}

void UITreeViewItem::SetShowIcon(bool show)
{
    if (m_ShowIcon == show) return;
    m_ShowIcon = show;
    UpdateIconState();
}

void UITreeViewItem::UpdateIconState()
{
    if (!m_IconImage) return;
    m_IconImage->SetTexture(m_Icon ? m_Icon.get() : nullptr);
    m_IconImage->SetActive(m_ShowIcon && m_Icon != nullptr);
}

void UITreeViewItem::RebuildLabels()
{
    if (!m_ArrowLabel) return;
    bool hasChildren = !m_TreeChildren.empty();
    // Use ASCII fallback — font atlas only covers 32..126, unicode triangles render as "?"
    m_ArrowLabel->SetText(hasChildren ? (m_Expanded ? "v" : ">") : "");
    m_ArrowLabel->SetActive(hasChildren);
    UpdateIconState();
    UpdateColors();
}

void UITreeViewItem::UpdateColors()
{
    if (!m_ItemEnabled) {
        if (m_TextLabel) m_TextLabel->SetColor({0.50f, 0.50f, 0.50f, 1.0f});
        SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        return;
    }
    if (m_Selected) {
        SetColor(m_SelectionColor);
        if (m_TextLabel) m_TextLabel->SetColor(m_TextSelectionColor);
        if (m_ArrowLabel) m_ArrowLabel->SetColor(m_ArrowColor);
    } else if (m_TreeHovered) {
        SetColor(m_HoverColor);
        if (m_TextLabel) m_TextLabel->SetColor(m_TextHoverColor);
        if (m_ArrowLabel) m_ArrowLabel->SetColor(m_ArrowColor);
    } else {
        SetColor({0.0f, 0.0f, 0.0f, 0.0f});
        if (m_TextLabel) m_TextLabel->SetColor(m_TextColor);
        if (m_ArrowLabel) m_ArrowLabel->SetColor(m_ArrowColor);
    }
}

} // namespace Leir
