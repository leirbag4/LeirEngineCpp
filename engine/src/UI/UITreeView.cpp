#include "LeirEngine/UI/UITreeView.h"
#include "LeirEngine/UI/UITreeViewItem.h"
#include "LeirEngine/UI/UIScrollbar.h"
#include "LeirEngine/UI/UITextInput.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Keyboard.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace Leir {

// Inline edit input that commits/cancels back to the owning UITreeView.
class TreeEditInput : public UITextInput {
public:
    explicit TreeEditInput(UITreeView* owner) : m_Owner(owner) { SetName("TreeEditInput"); }
    bool OnKeyDown(int key) override {
        if (key == 257 || key == 335) { // Enter / Numpad Enter
            if (m_Owner) m_Owner->CommitEdit(false);
            return true;
        }
        if (key == 256) { // Escape
            if (m_Owner) m_Owner->CommitEdit(true);
            return true;
        }
        return UITextInput::OnKeyDown(key);
    }
    void OnBlur() override {
        UITextInput::OnBlur();
        if (m_Owner) m_Owner->CommitEdit(false);
    }
private:
    UITreeView* m_Owner = nullptr;
};

// Clipped content viewport (ScrollView pattern). The canvas only forwards
// OnPointerMove to the single deepest hovered element (no bubbling), so moves
// landing on the viewport's empty area must be forwarded to the tree so it can
// recompute/clear its row hover.
class TreeViewport : public UIPanel {
public:
    explicit TreeViewport(UITreeView* owner) : m_Owner(owner) { SetName("TreeViewport"); }
    void OnPointerMove(const Vector2& pos) override {
        if (m_Owner) m_Owner->OnPointerMove(pos);
    }
private:
    UITreeView* m_Owner = nullptr;
};

UITreeView::UITreeView()
{
    SetName("TreeView");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetClip(true);
    SetSizePolicy(SizePolicy::Fill);

    // Internal clipped viewport: items live here (added via AddItem), scissored
    // to the content area minus the scrollbar strips. Added BEFORE the scrollbars
    // so the scrollbars render on top.
    m_Viewport = new TreeViewport(this);
    m_Viewport->SetClip(true);
    // UIPanel default color is opaque white; the viewport is only a clip region,
    // so it must not paint a background over the tree content.
    m_Viewport->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    AddChild(m_Viewport);

    m_VScrollbar = new UIScrollbar(true);
    m_VScrollbar->SetName("TreeViewVScrollbar");
    AddChild(m_VScrollbar);
    m_VScrollbar->SetOnScroll([this](float v) {
        float maxY = std::max(0.0f, GetContentSize().y - GetViewportSize().y);
        m_ScrollOffset.y = v * maxY;
    });

    m_HScrollbar = new UIScrollbar(false);
    m_HScrollbar->SetName("TreeViewHScrollbar");
    AddChild(m_HScrollbar);
    m_HScrollbar->SetOnScroll([this](float v) {
        float maxX = std::max(0.0f, GetContentSize().x - GetViewportSize().x);
        m_ScrollOffset.x = v * maxX;
    });
}

UITreeView::~UITreeView()
{
    if (m_Viewport) { RemoveChild(m_Viewport); delete m_Viewport; }
    if (m_VScrollbar) { RemoveChild(m_VScrollbar); delete m_VScrollbar; }
    if (m_HScrollbar) { RemoveChild(m_HScrollbar); delete m_HScrollbar; }
    if (m_EditInput) { RemoveChild(m_EditInput); delete m_EditInput; }
    if (m_PendingDeleteInput) { RemoveChild(m_PendingDeleteInput); delete m_PendingDeleteInput; }
    if (m_GhostLabel) { RemoveChild(m_GhostLabel); delete m_GhostLabel; }
    if (m_DropIndicator) { RemoveChild(m_DropIndicator); delete m_DropIndicator; }
    // Items are owned by the caller (like DockPanel content), not deleted here.
    // Flat cache is just views into the tree.
}

void UITreeView::ProcessPendingEditDeletion()
{
    if (!m_PendingDeleteInput) return;
    UITextInput* doomed = m_PendingDeleteInput;
    m_PendingDeleteInput = nullptr;
    // FIX (2026-08-24): the canvas may still hover this input (mouse over it when
    // Enter/blur commits). Deleting it leaves m_HoveredElement dangling ->
    // UIDebugOverlay::Update reads freed memory (GetName) -> corrupt std::string
    // -> bad_alloc crash. Clear hover/focus if they reference the doomed element
    // (same pattern DockManager uses after deleting dock content).
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) {
            if (c->GetHoveredElement() == doomed || c->GetFocus() == doomed)
                c->ClearHoverAndFocus();
            break;
        }
        e = e->GetParent();
    }
    RemoveChild(doomed);
    delete doomed;
}

bool UITreeView::OwnsChild(const UIElement* child) const
{
    return child == m_Viewport || child == m_VScrollbar || child == m_HScrollbar || child == m_EditInput || child == m_PendingDeleteInput || child == m_GhostLabel || child == m_DropIndicator;
}

void UITreeView::AddItem(UITreeViewItem* item, UITreeViewItem* parent)
{
    if (!item) return;
    // Remove from previous parent/root if exists
    if (item->GetTreeParent()) {
        item->GetTreeParent()->RemoveTreeChild(item);
    } else {
        auto it = std::find(m_Roots.begin(), m_Roots.end(), item);
        if (it != m_Roots.end()) m_Roots.erase(it);
    }
    // Also ensure not already a UI child (stale)
    if (m_Viewport) m_Viewport->RemoveChild(item);
    if (parent) {
        parent->AddTreeChild(item);
    } else {
        m_Roots.push_back(item);
    }
    // Items are children of the internal clipped viewport (flat virtualized
    // rendering, ScrollView pattern), not of the tree directly.
    if (m_Viewport) m_Viewport->AddChild(item);
    if (m_Font) item->SetFont(m_Font);
    item->SetIndent(m_Indent);
    item->SetShowIcon(m_IconsEnabled);
    item->SetIconSize(m_IconSize);
    InvalidateFlatCache();
}

void UITreeView::RemoveItem(UITreeViewItem* item)
{
    if (!item) return;
    // Remove from selected
    m_SelectedItems.erase(std::remove(m_SelectedItems.begin(), m_SelectedItems.end(), item), m_SelectedItems.end());
    if (m_HoveredItem == item) m_HoveredItem = nullptr;
    if (m_DropTarget.item == item) ClearDropHighlight();
    if (m_DragItem == item) m_DragItem = nullptr;

    if (item->GetTreeParent()) {
        item->GetTreeParent()->RemoveTreeChild(item);
    } else {
        auto it = std::find(m_Roots.begin(), m_Roots.end(), item);
        if (it != m_Roots.end()) m_Roots.erase(it);
    }
    if (m_Viewport) m_Viewport->RemoveChild(item);
    // Also remove all descendants from UI children and roots tracking
    std::function<void(UITreeViewItem*)> removeDescendants = [&](UITreeViewItem* n) {
        for (auto* c : n->GetTreeChildren()) {
            if (m_Viewport) m_Viewport->RemoveChild(c);
            removeDescendants(c);
        }
    };
    removeDescendants(item);
    InvalidateFlatCache();
}

void UITreeView::ClearItems()
{
    RebuildFlatCache();
    for (auto* it : m_FlatVisible)
        if (m_Viewport) m_Viewport->RemoveChild(it);
    m_Roots.clear();
    m_SelectedItems.clear();
    m_HoveredItem = nullptr;
    m_DropTarget = {};
    m_FlatVisible.clear();
    m_FlatDirty = true;
    m_CachedMaxWidthDirty = true;
}

int UITreeView::GetItemCount() const
{
    RebuildFlatCache();
    return (int)m_FlatVisible.size();
}

UITreeViewItem* UITreeView::GetItemAt(int visibleIndex) const
{
    RebuildFlatCache();
    if (visibleIndex < 0 || visibleIndex >= (int)m_FlatVisible.size()) return nullptr;
    return m_FlatVisible[visibleIndex];
}

int UITreeView::GetSelectedIndex() const
{
    if (m_SelectedItems.empty()) return -1;
    RebuildFlatCache();
    for (int i = 0; i < (int)m_FlatVisible.size(); ++i) {
        if (m_FlatVisible[i] == m_SelectedItems[0]) return i;
    }
    return -1;
}

void UITreeView::SetSelectedIndex(int index)
{
    RebuildFlatCache();
    if (index < 0 || index >= (int)m_FlatVisible.size()) {
        SetSelectedItems({});
        return;
    }
    SetSelectedItem(m_FlatVisible[index]);
}

UITreeViewItem* UITreeView::GetSelectedItem() const
{
    return m_SelectedItems.empty() ? nullptr : m_SelectedItems[0];
}

void UITreeView::SetSelectedItem(UITreeViewItem* item)
{
    if (!item) { SetSelectedItems({}); return; }
    SetSelectedItems({item});
}

void UITreeView::SetSelectedItems(const std::vector<UITreeViewItem*>& items)
{
    // Filter disabled items
    std::vector<UITreeViewItem*> filtered;
    for (auto* it : items) {
        if (it && it->IsItemEnabled()) filtered.push_back(it);
    }
    if (!m_MultipleSelection && filtered.size() > 1) {
        filtered.resize(1);
    }
    // Update visuals
    for (auto* it : m_SelectedItems) if (it) it->SetSelected(false);
    m_SelectedItems = filtered;
    for (auto* it : m_SelectedItems) if (it) it->SetSelected(true);
    if (!m_SelectedItems.empty()) m_LastSelectedIndex = GetSelectedIndex();
    NotifySelectionChanged();
}

void UITreeView::SetIndent(float indent)
{
    m_Indent = indent;
    for (auto* r : m_Roots) r->SetIndent(indent);
    InvalidateFlatCache();
}

void UITreeView::SetIconsEnabled(bool enabled)
{
    m_IconsEnabled = enabled;
    std::function<void(UITreeViewItem*)> apply = [&](UITreeViewItem* it) {
        it->SetShowIcon(enabled);
        it->SetIconSize(m_IconSize);
        for (auto* c : it->GetTreeChildren()) apply(c);
    };
    for (auto* r : m_Roots) apply(r);
    InvalidateFlatCache();
}

void UITreeView::SetIconSize(float size)
{
    m_IconSize = size;
    std::function<void(UITreeViewItem*)> apply = [&](UITreeViewItem* it) {
        it->SetIconSize(size);
        for (auto* c : it->GetTreeChildren()) apply(c);
    };
    for (auto* r : m_Roots) apply(r);
}

void UITreeView::SetFont(Font* font)
{
    m_Font = font;
    std::function<void(UITreeViewItem*)> apply = [&](UITreeViewItem* it) {
        it->SetFont(font);
        for (auto* c : it->GetTreeChildren()) apply(c);
    };
    for (auto* r : m_Roots) apply(r);
    if (m_EditInput) m_EditInput->SetFont(font);
    if (m_GhostLabel) m_GhostLabel->SetFont(font);
    InvalidateFlatCache();
}

void UITreeView::SetVerticalScrollbarEnabled(bool e)
{
    m_VScrollbarEnabled = e;
    if (m_VScrollbar) m_VScrollbar->SetActive(e);
}

void UITreeView::SetHorizontalScrollbarEnabled(bool e)
{
    m_HScrollbarEnabled = e;
    if (m_HScrollbar) m_HScrollbar->SetActive(e);
}

Vector2 UITreeView::GetContentSize() const
{
    RebuildFlatCache();
    if (!m_CachedMaxWidthDirty && !m_FlatDirty) {
        float h = (float)m_FlatVisible.size() * m_RowHeight;
        return {m_CachedMaxWidth, h};
    }
    float maxW = 0.0f;
    for (auto* it : m_FlatVisible) {
        float w = (float)it->GetDepth() * m_Indent + 12.0f + 8.0f;
        if (m_Font) {
            w += m_Font->MeasureText(it->GetText()).x;
        } else {
            w += (float)it->GetText().size() * 7.0f;
        }
        maxW = std::max(maxW, w);
    }
    // Cache
    const_cast<UITreeView*>(this)->m_CachedMaxWidth = maxW;
    const_cast<UITreeView*>(this)->m_CachedMaxWidthDirty = false;
    float h = (float)m_FlatVisible.size() * m_RowHeight;
    return {maxW, h};
}

Vector2 UITreeView::GetViewportSize() const
{
    const auto& cr = GetComputedRect();
    float w = cr.z;
    float h = cr.w;
    if (m_VScrollbarEnabled && m_VScrollbar && m_VScrollbar->IsActive()) w -= 10.0f;
    if (m_HScrollbarEnabled && m_HScrollbar && m_HScrollbar->IsActive()) h -= 10.0f;
    return {std::max(0.0f, w), std::max(0.0f, h)};
}

void UITreeView::SetScrollOffset(Vector2 offset)
{
    Vector2 max = {std::max(0.0f, GetContentSize().x - GetViewportSize().x),
                   std::max(0.0f, GetContentSize().y - GetViewportSize().y)};
    m_ScrollOffset.x = std::clamp(offset.x, 0.0f, max.x);
    m_ScrollOffset.y = std::clamp(offset.y, 0.0f, max.y);
}

void UITreeView::RebuildFlatCache() const
{
    if (!m_FlatDirty) return;
    auto* self = const_cast<UITreeView*>(this);
    self->m_FlatVisible.clear();
    std::function<void(UITreeViewItem*)> dfs = [&](UITreeViewItem* item) {
        // Filtered nodes (and their subtrees) are excluded from the flat view —
        // they are NOT destroyed, just hidden (SetFilter computes the flags).
        if (item->IsTreeFiltered()) return;
        self->m_FlatVisible.push_back(item);
        if (item->IsExpanded()) {
            for (auto* c : item->GetTreeChildren()) dfs(c);
        }
    };
    for (auto* r : m_Roots) dfs(r);
    self->m_FlatDirty = false;
}

bool UITreeView::FilterMatches(const std::string& text) const
{
    if (m_Filter.empty()) return true;
    if (m_Filter.size() > text.size()) return false;
    for (size_t i = 0; i + m_Filter.size() <= text.size(); ++i) {
        size_t j = 0;
        for (; j < m_Filter.size(); ++j) {
            if (std::tolower((unsigned char)text[i + j]) != (unsigned char)m_Filter[j])
                break;
        }
        if (j == m_Filter.size()) return true;
    }
    return false;
}

// Godot-style bottom-up visibility: an item is visible if it matches (and is not
// filter-excluded) OR any descendant is visible. Sets the transient filtered flag
// on the item. Post-order so children are decided before their parent.
// An EMPTY filter matches everything (even filter-excluded items), so clearing
// the filter always brings every item back — exclusion only applies while a
// non-empty filter is active.
bool UITreeView::ComputeFilterVisibility(UITreeViewItem* item)
{
    const bool matches = m_Filter.empty() || (!item->IsFilterExcluded() && FilterMatches(item->GetText()));
    bool visible = matches;
    for (auto* c : item->GetTreeChildren())
        if (ComputeFilterVisibility(c)) visible = true;
    item->SetTreeFiltered(!visible);
    return visible;
}

void UITreeView::SetFilter(const std::string& filter)
{
    m_Filter.clear();
    for (char ch : filter) m_Filter += (char)std::tolower((unsigned char)ch);
    for (auto* r : m_Roots) ComputeFilterVisibility(r);
    InvalidateFlatCache();
}

void UITreeView::SyncScrollbars()
{
    const auto& cr = GetComputedRect();
    float rightEdge = std::round(cr.x + cr.z);
    float bottomEdge = std::round(cr.y + cr.w);
    const float kW = 10.0f;

    Vector2 content = GetContentSize();
    Vector2 viewport = GetViewportSize();

    bool vOverflow = m_VScrollbarEnabled && content.y > viewport.y && viewport.y > 1.0f;
    bool hOverflow = m_HScrollbarEnabled && content.x > viewport.x && viewport.x > 1.0f;

    if (m_VScrollbar) {
        m_VScrollbar->SetActive(vOverflow);
        if (vOverflow) {
            // Relative to the tree + parentOffset={cr.xy} (no transient double).
            m_VScrollbar->GetRect().anchor = {0, 0, 0, 0};
            m_VScrollbar->GetRect().offset = {rightEdge - kW - cr.x, std::round(cr.y) - cr.y, rightEdge - cr.x, (hOverflow ? bottomEdge - kW : bottomEdge) - cr.y};
            m_VScrollbar->ComputeLayout({cr.z, cr.w}, {cr.x, cr.y});
            m_VScrollbar->SetRange(viewport.y, content.y);
            float maxY = std::max(0.0f, content.y - viewport.y);
            m_VScrollbar->SetValue(maxY > 0 ? m_ScrollOffset.y / maxY : 0.0f);
        }
    }
    if (m_HScrollbar) {
        m_HScrollbar->SetActive(hOverflow);
        if (hOverflow) {
            m_HScrollbar->GetRect().anchor = {0, 0, 0, 0};
            m_HScrollbar->GetRect().offset = {std::round(cr.x) - cr.x, bottomEdge - kW - cr.y, (vOverflow ? rightEdge - kW : rightEdge) - cr.x, bottomEdge - cr.y};
            m_HScrollbar->ComputeLayout({cr.z, cr.w}, {cr.x, cr.y});
            m_HScrollbar->SetRange(viewport.x, content.x);
            float maxX = std::max(0.0f, content.x - viewport.x);
            m_HScrollbar->SetValue(maxX > 0 ? m_ScrollOffset.x / maxX : 0.0f);
        }
    }
}

void UITreeView::UpdateSelection(UITreeViewItem* clickedItem, bool ctrl, bool shift)
{
    if (!clickedItem || !clickedItem->IsItemEnabled()) return;
    RebuildFlatCache();
    if (m_MultipleSelection && ctrl) {
        auto it = std::find(m_SelectedItems.begin(), m_SelectedItems.end(), clickedItem);
        if (it != m_SelectedItems.end()) {
            (*it)->SetSelected(false);
            m_SelectedItems.erase(it);
        } else {
            clickedItem->SetSelected(true);
            m_SelectedItems.push_back(clickedItem);
        }
    } else if (m_MultipleSelection && shift && m_LastSelectedIndex >= 0) {
        int clickedIdx = -1;
        for (int i = 0; i < (int)m_FlatVisible.size(); ++i) if (m_FlatVisible[i]==clickedItem) clickedIdx=i;
        int lo = std::min(m_LastSelectedIndex, clickedIdx);
        int hi = std::max(m_LastSelectedIndex, clickedIdx);
        for (auto* it : m_SelectedItems) it->SetSelected(false);
        m_SelectedItems.clear();
        for (int i = lo; i <= hi; ++i) {
            if (m_FlatVisible[i]->IsItemEnabled()) {
                m_FlatVisible[i]->SetSelected(true);
                m_SelectedItems.push_back(m_FlatVisible[i]);
            }
        }
    } else {
        for (auto* it : m_SelectedItems) it->SetSelected(false);
        m_SelectedItems.clear();
        clickedItem->SetSelected(true);
        m_SelectedItems.push_back(clickedItem);
        m_LastSelectedIndex = -1;
        for (int i = 0; i < (int)m_FlatVisible.size(); ++i) if (m_FlatVisible[i]==clickedItem) m_LastSelectedIndex=i;
    }
    NotifySelectionChanged();
}

void UITreeView::NotifySelectionChanged()
{
    int idx = GetSelectedIndex();
    if (m_OnSelectedIndexChanged) m_OnSelectedIndexChanged(idx);
    if (m_OnSelectedItemChanged) m_OnSelectedItemChanged(GetSelectedItem());
    if (m_OnSelectedItemsChanged) m_OnSelectedItemsChanged(m_SelectedItems);
}

void UITreeView::BeginEdit(UITreeViewItem* item)
{
    if (!m_Editable || !item || !item->IsItemEnabled()) return;
    if (m_PendingDeleteInput) ProcessPendingEditDeletion();
    if (m_EditInput) { RemoveChild(m_EditInput); delete m_EditInput; m_EditInput = nullptr; }
    if (m_EditingItem) CommitEdit(true); // cancel any stale edit without deleting inside its callback
    m_EditingItem = item;
    m_EditOldText = item->GetText();
    m_EditInput = new TreeEditInput(this);
    m_EditInput->SetFont(m_Font);
    m_EditInput->SetText(m_EditOldText);
    m_EditInput->SetOverlayLayer(true);
    AddChild(m_EditInput);
    UpdateEditInputRect();
    // Focus
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { c->SetFocus(m_EditInput); break; }
        e = e->GetParent();
    }
}

void UITreeView::CommitEdit(bool cancel)
{
    // Re-entrancy: ClearFocus() below goes through SetFocus(nullptr) → OnBlur
    // on the input → TreeEditInput::OnBlur → CommitEdit again. Also deleting
    // the input inside its own OnKeyDown/OnBlur is use-after-free. Defer the
    // actual delete to the next layout.
    if (m_EditCommitting) return;
    if (!m_EditInput || !m_EditingItem) return;
    m_EditCommitting = true;

    UITextInput* input = m_EditInput;
    UITreeViewItem* item = m_EditingItem;
    std::string newText = input->GetText();
    std::string oldText = m_EditOldText;

    // Detach state first so a re-entrant call via OnBlur sees nothing to do.
    m_EditInput = nullptr;
    m_EditingItem = nullptr;
    m_EditOldText.clear();

    // Hide immediately (stops hit-test/render while pending).
    input->SetActive(false);

    // Flush any older pending input from a previous edit that never got collected
    // (e.g. BeginEdit deleted m_EditInput synchronously but not pending).
    if (m_PendingDeleteInput) ProcessPendingEditDeletion();

    // Clear canvas focus WITHOUT re-entering CommitEdit. SetFocus(nullptr) will
    // call OnBlur on the input (still reachable via `input` param). The guard
    // above ensures the nested CommitEdit from that OnBlur is a no-op because
    // m_EditInput is already null.
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { if (c->GetFocus() == input) c->ClearFocus(); break; }
        e = e->GetParent();
    }

    // Apply rename
    if (!cancel && newText != oldText) {
        item->SetText(newText);
        InvalidateFlatCache();
        if (m_OnItemRenamed) m_OnItemRenamed(item, oldText, newText);
    }

    // Defer actual destruction — deleting `input` while OnKeyDown/OnBlur is still
    // on the stack would be use-after-free (the caller is `input` itself).
    m_PendingDeleteInput = input;

    m_EditCommitting = false;
}

void UITreeView::UpdateEditInputRect()
{
    if (!m_EditInput || !m_EditingItem) return;
    RebuildFlatCache();
    const auto& cr = GetComputedRect();
    int idx = -1;
    for (int i = 0; i < (int)m_FlatVisible.size(); ++i) if (m_FlatVisible[i]==m_EditingItem) idx=i;
    if (idx < 0) return;
    float y = std::round(cr.y + (float)idx * m_RowHeight - m_ScrollOffset.y) - cr.y;
    // Input must sit exactly over the row's TEXT: always reserve the arrow slot
    // (12px, even for leaves — matches the layout fix) plus the icon slot when
    // icons are enabled and the item has one. Relative to the tree + parentOffset.
    float x = std::round(cr.x + (float)m_EditingItem->GetDepth() * m_Indent + 12.0f + 4.0f - m_ScrollOffset.x) - cr.x;
    if (m_IconsEnabled && m_EditingItem->HasIcon())
        x += m_IconSize + 4.0f;
    float w = std::max(60.0f, GetViewportSize().x - x);
    m_EditInput->GetRect().anchor = {0, 0, 0, 0};
    m_EditInput->GetRect().offset = {x, y, x + w, y + m_RowHeight};
    m_EditInput->ComputeLayout({w, m_RowHeight}, {cr.x, cr.y});
}

bool UITreeView::IsItemVisible(UITreeViewItem* item) const
{
    RebuildFlatCache();
    for (auto* it : m_FlatVisible) if (it == item) return true;
    return false;
}

UITreeView::DropTarget UITreeView::HitTestDropTarget(const Vector2& pos) const
{
    RebuildFlatCache();
    const auto& cr = GetComputedRect();
    int idx = (int)std::floor((pos.y - cr.y + m_ScrollOffset.y) / m_RowHeight);
    if (idx < 0 || idx >= (int)m_FlatVisible.size()) return {};
    auto* item = m_FlatVisible[idx];
    float rowY = cr.y + (float)idx * m_RowHeight - m_ScrollOffset.y;
    float relY = pos.y - rowY;
    DropMode mode = DropMode::Onto;
    if (relY > m_RowHeight - 4.0f) mode = DropMode::Below;
    return {item, mode};
}

void UITreeView::ClearDropHighlight()
{
    m_DropTarget = {};
    if (m_DropIndicator) m_DropIndicator->SetActive(false);
}

void UITreeView::OnLayoutComputed()
{
    UIElement::OnLayoutComputed();

    if (m_PendingDeleteInput) ProcessPendingEditDeletion();

    RebuildFlatCache();
    Vector2 content = GetContentSize();
    Vector2 viewport = GetViewportSize();

    // Clamp scroll offset
    float maxY = std::max(0.0f, content.y - viewport.y);
    float maxX = std::max(0.0f, content.x - viewport.x);
    m_ScrollOffset.y = std::clamp(m_ScrollOffset.y, 0.0f, maxY);
    m_ScrollOffset.x = std::clamp(m_ScrollOffset.x, 0.0f, maxX);

    const auto& cr = GetComputedRect();
    int n = (int)m_FlatVisible.size();
    int first = 0, last = -1;
    if (n > 0 && viewport.y > 0) {
        first = (int)std::floor(m_ScrollOffset.y / m_RowHeight);
        last = (int)std::ceil((m_ScrollOffset.y + viewport.y) / m_RowHeight) - 1;
        first = std::clamp(first, 0, n - 1);
        last = std::clamp(last, 0, n - 1);
    }

    // Position the internal clipped viewport = content area minus the scrollbar
    // strips. Items are its children and are scissored to it, so rows never
    // render over/under the scrollbars (they look "translucent" because content
    // drew on top) and horizontal scroll clips correctly.
    if (m_Viewport) {
        // Positioned RELATIVE to the tree + parentOffset={cr.xy} (the tree's own
        // Free pass passes the same) so no transient double-counting occurs.
        m_Viewport->GetRect().anchor = {0, 0, 0, 0};
        m_Viewport->GetRect().offset = {
            std::round(cr.x) - cr.x, std::round(cr.y) - cr.y,
            std::round(cr.x + viewport.x) - cr.x, std::round(cr.y + viewport.y) - cr.y
        };
        m_Viewport->ComputeLayout({viewport.x, viewport.y}, {cr.x, cr.y});
    }
    const auto& vp = m_Viewport ? m_Viewport->GetComputedRect() : cr;
    // Row background spans max(viewport, content) so scrolled rows still cover the
    // visible area; horizontal scroll shifts rows by m_ScrollOffset.x (FIX: the
    // hscrollbar previously moved the offset but rows ignored it).
    const float bgW = std::max(viewport.x, content.x);

    for (int i = 0; i < n; ++i) {
        auto* item = m_FlatVisible[i];
        bool visible = (i >= first && i <= last);
        item->SetActive(visible);
        if (visible) {
            // Rows are children of the VIEWPORT: position RELATIVE to it and pass
            // parentOffset={vp.xy} so m_ComputedRect lands on absolute coords (the
            // viewport's Free pass passes the same offset — no transient double).
            float y = std::round(vp.y + (float)i * m_RowHeight - m_ScrollOffset.y) - vp.y;
            float x0 = std::round(vp.x - m_ScrollOffset.x) - vp.x;
            // Full-width row for selection background
            item->GetRect().anchor = {0, 0, 0, 0};
            item->GetRect().offset = {x0, y, x0 + bgW, y + m_RowHeight};
            item->ComputeLayout({bgW, m_RowHeight}, {vp.x, vp.y});
        }
    }

    // FIX (2026-08-23): items that left m_FlatVisible (collapsed parents' children)
    // were left SetActive(true) forever, so the stale ComputeFreeLayout pass kept
    // positioning them at {treeView.x, treeView.y} + accumulated offset -> the
    // "flying / conglomerate of text" at startup and on collapse. Deactivate any
    // UITreeViewItem child of the viewport not in the visible flat range.
    const auto& itemChildren = m_Viewport ? m_Viewport->GetChildren() : GetChildren();
    for (auto* child : itemChildren) {
        auto* it = dynamic_cast<UITreeViewItem*>(child);
        if (!it) continue;
        bool inVisible = false;
        for (int i = first; i <= last && i < n; ++i) {
            if (m_FlatVisible[i] == it) { inVisible = true; break; }
        }
        if (!inVisible && it->IsActive())
            it->SetActive(false);
    }

    SyncScrollbars();

    // Keep edit input rect in sync if editing
    if (m_EditInput && m_EditInput->IsActive()) {
        UpdateEditInputRect();
    }

    // Ghost is a Free child of the tree: positioned RELATIVE to it (m_GhostPos is
    // absolute canvas coords) + parentOffset={cr.xy} so it renders at the cursor
    // without any transient double from the tree's own Free pass.
    if (m_GhostLabel && m_GhostLabel->IsActive()) {
        m_GhostLabel->GetRect().anchor = {0, 0, 0, 0};
        m_GhostLabel->GetRect().offset = {
            m_GhostPos.x + 12.0f - cr.x, m_GhostPos.y + 8.0f - cr.y,
            m_GhostPos.x + 200.0f - cr.x, m_GhostPos.y + 28.0f - cr.y
        };
        m_GhostLabel->ComputeLayout({200, 20}, {cr.x, cr.y});
    }

    // FIX (2026-08-23): drop feedback — a 2px line at the target row's bottom
    // edge (Below = reorder) or a translucent fill over the target row (Onto =
    // nest). Positioned here (absolute) so the ComputeFreeLayout accumulation
    // never drifts it, same pattern as the ghost.
    if (m_Dragging && m_DropTarget.item && n > 0) {
        if (!m_DropIndicator) {
            m_DropIndicator = new UIPanel();
            m_DropIndicator->SetName("TreeDropIndicator");
            // FIX (2026-08-24): overlay layer routes this to the debug batch,
            // which Flush draws LAST (regular -> viewport -> debug). Without it,
            // items re-parented via drag are re-added at the END of m_Children
            // and draw OVER the indicator, hiding its bottom half (the line
            // looked 1px instead of 2px over recently-nested items).
            m_DropIndicator->SetOverlayLayer(true);
            AddChild(m_DropIndicator);
        }
        int tIdx = -1;
        for (int i = 0; i < n; ++i) {
            if (m_FlatVisible[i] == m_DropTarget.item) { tIdx = i; break; }
        }
        if (tIdx >= first && tIdx <= last) {
            m_DropIndicator->SetActive(true);
            // Relative to the tree + parentOffset={cr.xy} (no transient double).
            float rowY = std::round(cr.y + (float)tIdx * m_RowHeight - m_ScrollOffset.y) - cr.y;
            float x0 = std::round(cr.x - m_ScrollOffset.x) - cr.x;
            float indW = std::max(viewport.x, content.x);
            m_DropIndicator->GetRect().anchor = {0, 0, 0, 0};
            if (m_DropTarget.mode == DropMode::Below) {
                m_DropIndicator->SetColor({0.65f, 0.45f, 1.0f, 0.95f});
                m_DropIndicator->GetRect().offset = {x0, rowY + m_RowHeight - 1.0f, x0 + indW, rowY + m_RowHeight + 1.0f};
                m_DropIndicator->ComputeLayout({indW, 2.0f}, {cr.x, cr.y});
            } else { // Onto
                m_DropIndicator->SetColor({0.55f, 0.40f, 1.0f, 0.35f});
                m_DropIndicator->GetRect().offset = {x0, rowY, x0 + indW, rowY + m_RowHeight};
                m_DropIndicator->ComputeLayout({indW, m_RowHeight}, {cr.x, cr.y});
            }
        } else {
            m_DropIndicator->SetActive(false);
        }
    } else if (m_DropIndicator) {
        m_DropIndicator->SetActive(false);
    }
}

bool UITreeView::OnPointerDown(const Vector2& pos)
{
    RebuildFlatCache();
    const auto& cr = GetComputedRect();
    Vector2 viewport = GetViewportSize();
    // Check if click is on scrollbar area — let scrollbar handle (hit-test will route to it first,
    // but this is fallback)
    if (m_VScrollbar && m_VScrollbar->IsActive()) {
        const auto& sr = m_VScrollbar->GetComputedRect();
        if (pos.x >= sr.x && pos.x <= sr.x + sr.z && pos.y >= sr.y && pos.y <= sr.y + sr.w) return false;
    }
    if (m_HScrollbar && m_HScrollbar->IsActive()) {
        const auto& sr = m_HScrollbar->GetComputedRect();
        if (pos.x >= sr.x && pos.x <= sr.x + sr.z && pos.y >= sr.y && pos.y <= sr.y + sr.w) return false;
    }

    // Find row under pos
    int idx = (int)std::floor((pos.y - cr.y + m_ScrollOffset.y) / m_RowHeight);
    if (idx < 0 || idx >= (int)m_FlatVisible.size()) return false;
    auto* item = m_FlatVisible[idx];
    if (!item || !item->IsItemEnabled()) return true; // consume but not selectable

    // Check arrow hit (12x12 at indent, shifted by horizontal scroll)
    float arrowX = cr.x - m_ScrollOffset.x + (float)item->GetDepth() * m_Indent;
    float arrowY = cr.y + (float)idx * m_RowHeight - m_ScrollOffset.y;
    bool hasChildren = !item->GetTreeChildren().empty();
    bool onArrow = hasChildren && pos.x >= arrowX && pos.x <= arrowX + 12.0f && pos.y >= arrowY && pos.y <= arrowY + m_RowHeight;

    if (onArrow) {
        bool exp = !item->IsExpanded();
        item->SetExpanded(exp);
        InvalidateFlatCache();
        if (exp && m_OnItemExpanded) m_OnItemExpanded(item);
        else if (!exp && m_OnItemCollapsed) m_OnItemCollapsed(item);
        // No actualizar m_LastClick* aquí: el toggle de flecha no debe contar como click para doble-click
        return true;
    }

    // Double-click detection (solo para filas, no flecha — la flecha ya retornó arriba)
    static int s_Frame = 0; ++s_Frame;
    bool isDoubleClick = (item == m_LastClickItem && (s_Frame - m_LastClickFrame) <= 15 && std::fabs(pos.x - m_LastClickPos.x) <= 3.0f && std::fabs(pos.y - m_LastClickPos.y) <= 3.0f);
    m_LastClickFrame = s_Frame;
    m_LastClickPos = pos;
    m_LastClickItem = item;

    if (isDoubleClick) {
        if (m_OnItemDoubleClicked) m_OnItemDoubleClicked(item);
        return true;
    }

    // Selection — full-width row, so any x within viewport selects
    if (pos.x < cr.x || pos.x > cr.x + viewport.x) return false;
    if (pos.y < cr.y || pos.y > cr.y + viewport.y) return false;

    bool ctrl = Keyboard::IsDown(Key::LeftControl) || Keyboard::IsDown(Key::RightControl);
    bool shift = Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift);
    // FIX (2026-08-23): a plain click on an item that is ALREADY selected must
    // NOT clear the multi-selection — that would break dragging several items at
    // once. Defer the collapse-to-single until OnPointerUp (only when no drag
    // actually happens). The drag code in OnPointerMove then drags the whole
    // m_SelectedItems like any standard treeview.
    m_DeferredSelect = nullptr;
    bool alreadySelected = m_MultipleSelection &&
        std::find(m_SelectedItems.begin(), m_SelectedItems.end(), item) != m_SelectedItems.end();
    if (!ctrl && !shift && alreadySelected) {
        m_DeferredSelect = item;
    } else {
        UpdateSelection(item, ctrl, shift);
    }

    // Drag start
    m_DragItem = item;
    m_DragStartPos = pos;
    m_Dragging = false;
    // Capture pointer for drag
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { c->CapturePointer(this); break; }
        e = e->GetParent();
    }

    return true;
}

void UITreeView::OnPointerMove(const Vector2& pos)
{
    // Hover tracking — full-width rows, driven by parent (B). Works even when
    // the hit is TreeText/TreeArrow (deepest child) and full-width selection.
    // vieja lógica con IsHovered() del canvas solo funcionaba a la izquierda.
    RebuildFlatCache();
    const auto& cr = GetComputedRect();
    Vector2 viewport = GetViewportSize();
    bool inside = (pos.x >= cr.x && pos.x <= cr.x + viewport.x && pos.y >= cr.y && pos.y <= cr.y + viewport.y);
    UITreeViewItem* hovered = nullptr;
    int dbgIdx = -1;
    if (inside) {
        dbgIdx = (int)std::floor((pos.y - cr.y + m_ScrollOffset.y) / m_RowHeight);
        if (dbgIdx >= 0 && dbgIdx < (int)m_FlatVisible.size()) {
            auto* it = m_FlatVisible[dbgIdx];
            if (it->IsItemEnabled()) hovered = it;
        }
    }
    NotifyItemHovered(hovered);

    if (m_DragItem && !m_Dragging) {
        float dx = pos.x - m_DragStartPos.x;
        float dy = pos.y - m_DragStartPos.y;
        if (dx*dx + dy*dy > 16.0f) {
            m_Dragging = true;
            m_DeferredSelect = nullptr; // dragging -> keep the multi-selection
            // Collect drag items: if dragging a selected item, drag all selected; else single
            auto it = std::find(m_SelectedItems.begin(), m_SelectedItems.end(), m_DragItem);
            if (it != m_SelectedItems.end()) {
                m_DragItems = m_SelectedItems;
            } else {
                m_DragItems = {m_DragItem};
            }
            // Create ghost
            if (!m_GhostLabel) {
                m_GhostLabel = new UILabel();
                m_GhostLabel->SetName("TreeDragGhost");
                m_GhostLabel->SetFont(m_Font);
                m_GhostLabel->SetFontSize(13);
                // UILabel::m_Color is the GLYPH color (not background) — a
                // transparent color here made the ghost text invisible.
                m_GhostLabel->SetColor({0.90f, 0.90f, 0.90f, 0.95f});
                m_GhostLabel->SetOverlayLayer(true);
                AddChild(m_GhostLabel);
            }
            std::string ghostText = m_DragItems.size() == 1 ? m_DragItems[0]->GetText() : std::to_string(m_DragItems.size()) + " items";
            m_GhostLabel->SetText(ghostText);
            m_GhostLabel->SetActive(true);
            m_GhostPos = pos;
            // FIX (2026-08-23): on the FIRST drag the ghost is created with a
            // default {0,0,0,0} rect. The stale Free pass then laid it out with
            // height 0, so UILabel::Rebuild built the glyphs with cr.w=0
            // (offsetY=(0-blockH)/2+ascender) — the text rendered ~10px ABOVE
            // the box on the first drag only. Give the ghost a proper height
            // immediately so the first rebuild uses correct centering. Relative
            // to the tree + parentOffset={cr.xy} (no transient double).
            m_GhostLabel->GetRect().anchor = {0, 0, 0, 0};
            m_GhostLabel->GetRect().offset = {
                m_GhostPos.x + 12.0f - cr.x, m_GhostPos.y + 8.0f - cr.y,
                m_GhostPos.x + 200.0f - cr.x, m_GhostPos.y + 28.0f - cr.y
            };
            m_GhostLabel->ComputeLayout({200, 20}, {cr.x, cr.y});
        }
    }
    if (m_Dragging && m_GhostLabel) {
        // Ghost position is authoritative in m_GhostPos (set on every move);
        // OnLayoutComputed re-applies it each frame so it tracks the cursor.
        m_GhostPos = pos;
        // Drop target highlight
        auto target = HitTestDropTarget(pos);
        if (target.item != m_DropTarget.item || target.mode != m_DropTarget.mode) {
            ClearDropHighlight();
            m_DropTarget = target;
        }
    }
}

void UITreeView::OnPointerExit()
{
    if (m_HoveredItem) {
        m_HoveredItem->SetTreeHovered(false);
        m_HoveredItem = nullptr;
    }
}

void UITreeView::NotifyItemHovered(UITreeViewItem* item)
{
    if (item && !item->IsItemEnabled()) item = nullptr;
    if (item == m_HoveredItem) return;
    if (m_HoveredItem) m_HoveredItem->SetTreeHovered(false);
    m_HoveredItem = item;
    if (m_HoveredItem) m_HoveredItem->SetTreeHovered(true);
}

bool UITreeView::OnPointerUp(const Vector2& pos)
{
    if (m_Dragging) {
        auto target = HitTestDropTarget(pos);
        if (target.item && !m_DragItems.empty()) {
            // Prevent dropping onto self or descendant
            bool valid = true;
            for (auto* di : m_DragItems) {
                if (di == target.item) valid = false;
                // Check descendant
                std::function<bool(UITreeViewItem*, UITreeViewItem*)> isDesc = [&](UITreeViewItem* p, UITreeViewItem* q) -> bool {
                    for (auto* c : p->GetTreeChildren()) {
                        if (c == q) return true;
                        if (isDesc(c, q)) return true;
                    }
                    return false;
                };
                for (auto* di2 : m_DragItems) if (isDesc(di2, target.item)) valid = false;
            }
            if (valid) {
                // Perform reparent/reorder
                for (auto* di : m_DragItems) {
                    // Remove from old location
                    if (di->GetTreeParent()) di->GetTreeParent()->RemoveTreeChild(di);
                    else {
                        auto it = std::find(m_Roots.begin(), m_Roots.end(), di);
                        if (it != m_Roots.end()) m_Roots.erase(it);
                    }
                    if (m_Viewport) m_Viewport->RemoveChild(di);
                }
                // Insert at target
                for (auto* di : m_DragItems) {
                    if (target.mode == DropMode::Onto) {
                        target.item->AddTreeChild(di);
                        target.item->SetExpanded(true);
                        if (m_Viewport) m_Viewport->AddChild(di);
                        if (m_OnItemDragged) m_OnItemDragged(di, target.item, (int)target.item->GetTreeChildren().size() - 1);
                    } else if (target.mode == DropMode::Below) {
                        UITreeViewItem* parent = target.item->GetTreeParent();
                        int idx = -1;
                        if (parent) {
                            for (int i = 0; i < (int)parent->GetTreeChildren().size(); ++i) if (parent->GetTreeChildren()[i]==target.item) idx=i;
                            parent->InsertTreeChildAt(di, idx + 1);
                        } else {
                            for (int i = 0; i < (int)m_Roots.size(); ++i) if (m_Roots[i]==target.item) idx=i;
                            m_Roots.insert(m_Roots.begin() + idx + 1, di);
                            di->SetIndent(m_Indent);
                        }
                        if (m_Viewport) m_Viewport->AddChild(di);
                        if (m_OnItemDragged) m_OnItemDragged(di, parent, idx + 1);
                    }
                }
                InvalidateFlatCache();
            }
        }
        // Cleanup
        m_Dragging = false;
        m_DragItems.clear();
        m_DragItem = nullptr;
        if (m_GhostLabel) m_GhostLabel->SetActive(false);
        ClearDropHighlight();
        // Release capture
        UIElement* e = this;
        while (e) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { c->ReleasePointer(); break; }
            e = e->GetParent();
        }
        return true;
    }
    m_DragItem = nullptr;
    // Plain click on an already-selected item (no drag): collapse to single now.
    if (m_DeferredSelect) {
        UpdateSelection(m_DeferredSelect, false, false);
        m_DeferredSelect = nullptr;
    }
    return false;
}

bool UITreeView::OnScroll(float delta)
{
    float maxY = std::max(0.0f, GetContentSize().y - GetViewportSize().y);
    if (maxY <= 0) return false;
    // Shift+wheel = horizontal
    if (Keyboard::IsDown(Key::LeftShift) || Keyboard::IsDown(Key::RightShift)) {
        float maxX = std::max(0.0f, GetContentSize().x - GetViewportSize().x);
        if (maxX <= 0) return false;
        m_ScrollOffset.x = std::clamp(m_ScrollOffset.x - delta * 16.0f, 0.0f, maxX);
    } else {
        m_ScrollOffset.y = std::clamp(m_ScrollOffset.y - delta * 16.0f, 0.0f, maxY);
    }
    return true;
}

bool UITreeView::OnKeyDown(int key)
{
    if (m_Editable && key == 291) { // F2
        auto* item = GetSelectedItem();
        if (item && item->IsItemEnabled()) {
            BeginEdit(item);
            return true;
        }
    }
    if (key == 256) { // Escape
        if (m_EditInput && m_EditInput->IsActive()) {
            CommitEdit(true);
            return true;
        }
    }
    return false;
}

} // namespace Leir
