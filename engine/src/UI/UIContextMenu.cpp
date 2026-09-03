#include "LeirEngine/UI/UIContextMenu.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Key.h"
#include <algorithm>

namespace Leir {

// Submenu arrow icon size in logical pixels (the assets/icons/*.png are 13x13).
static constexpr float kArrowSize = 13.0f;

// ============================== UIContextMenu ==============================

UIContextMenu::UIContextMenu()
{
    SetName("ContextMenu");
    SetColor({0.13f, 0.13f, 0.15f, 1.0f}); // opaque container (items are transparent)
    SetLayoutMode(LayoutMode::Column);
    SetPadding(4.0f, 4.0f, 4.0f, 4.0f);
    SetSpacing(0.0f);
    SetOverlayLayer(true); // renders above the dock/viewports
    SetClip(true);
    SetActive(false);

    // Close on click OUTSIDE the menu and on ESC. Registered once; the hooks
    // capture the m_Alive shared flag so a late event after destruction is a no-op.
    auto& eq = EventQueue::Get();
    eq.AddPointerHook([this, alive = m_Alive](const PointerEvent& e) {
        if (!*alive || !m_Open) return;
        if (e.action != EventAction::Press) return;
        if (m_IgnoreOutsideClick) {
            // The Press that just opened the menu must not close it.
            m_IgnoreOutsideClick = false;
            return;
        }
        if (!HitTestPoint(e.position)) Close();
    });
    eq.AddKeyHook([this, alive = m_Alive](const KeyEvent& e) {
        if (!*alive || !m_Open) return;
        if (e.key == Key::Escape) CloseAllMenus();
    });
}

UIContextMenu::~UIContextMenu()
{
    *m_Alive = false; // the hooks must never dereference `this` after this point
    CloseSubMenu();
    RebuildItems();   // frees the rows (RemoveChild + delete)
    // Owned submenus: remove from canvas and delete.
    for (auto* sub : m_SubMenus) {
        if (sub->GetParent()) sub->GetParent()->RemoveChild(sub);
        delete sub;
    }
    m_SubMenus.clear();
}

void UIContextMenu::SetFont(Font* font)
{
    m_Font = font;
    for (auto* row : m_Rows) {
        if (auto* item = dynamic_cast<MenuItem*>(row)) item->SetFont(font);
    }
    for (auto* sub : m_SubMenus) sub->SetFont(font);
}

void UIContextMenu::SetSubMenuIcon(std::shared_ptr<Texture2D> icon)
{
    m_SubMenuIcon = std::move(icon);
    for (auto* row : m_Rows) {
        if (auto* item = dynamic_cast<MenuItem*>(row)) item->SetSubMenuIcon(m_SubMenuIcon);
    }
    // Propagate to owned submenus (their rows render their own arrow icon).
    for (auto* sub : m_SubMenus) sub->SetSubMenuIcon(m_SubMenuIcon);
}

void UIContextMenu::AddItem(const std::string& label, std::function<void()> action)
{
    m_Items.push_back({label, std::move(action), false, false, nullptr});
}

void UIContextMenu::AddSeparator()
{
    m_Items.push_back({"", {}, false, true, nullptr});
}

void UIContextMenu::AddItemDisabled(const std::string& label)
{
    m_Items.push_back({label, {}, true, false, nullptr});
}

void UIContextMenu::AddSubMenu(const std::string& label, UIContextMenu* subMenu)
{
    if (!subMenu) return;
    subMenu->m_OwnerMenu = this;
    m_SubMenus.push_back(subMenu);
    m_Items.push_back({label, {}, false, false, subMenu});
    // Propagate font + arrow icon so a submenu created after SetFont/SetSubMenuIcon
    // (e.g. the UIMenuBar adds submenus after the menu's font was set) still renders.
    if (m_Font) subMenu->SetFont(m_Font);
    if (m_SubMenuIcon) subMenu->SetSubMenuIcon(m_SubMenuIcon);
}

bool UIContextMenu::OwnsChild(const UIElement* child) const
{
    return std::find(m_Rows.begin(), m_Rows.end(), child) != m_Rows.end();
}
Vector2 UIContextMenu::GetMinSize() const
{
    return {120.0f, 24.0f};
}

Vector2 UIContextMenu::GetContentSize() const
{
    return UIPanel::GetContentSize();
}

void UIContextMenu::ClearCanvasRefs()
{
    UICanvas* canvas = nullptr;
    for (UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { canvas = c; break; }
    }
    if (!canvas) return;
    if (Contains(canvas->GetFocus()) || Contains(canvas->GetHoveredElement()))
        canvas->ClearHoverAndFocus();
}

bool UIContextMenu::Contains(const UIElement* e) const
{
    if (!e) return false;
    if (e == this) return true;
    for (const UIElement* p = e->GetParent(); p; p = p->GetParent()) {
        if (p == this) return true;
    }
    // Owned submenus live on the canvas (not as children of this menu), so a
    // focused/hovered element inside a submenu must be matched recursively.
    for (auto* sub : m_SubMenus) {
        if (sub && sub->Contains(e)) return true;
    }
    return false;
}

void UIContextMenu::RebuildItems()
{
    // The rows below are freed; make sure the canvas doesn't keep pointing at
    // them (focus/hover would dangle → crash on the next SetFocus/HitTest).
    ClearCanvasRefs();
    for (auto* row : m_Rows) {
        RemoveChild(row);
        delete row;
    }
    m_Rows.clear();
    for (const auto& it : m_Items) {
        if (it.separator) {
            auto* sep = new UIPanel();
            sep->SetName("CtxSep");
            sep->SetColor({0.30f, 0.30f, 0.34f, 1.0f});
            sep->SetMinSize({0.0f, 2.0f});
            sep->SetSizePolicy(SizePolicy::Fixed); // full width, fixed 2px (not a Fill share)
            AddChild(sep);
            m_Rows.push_back(sep);
        } else {
            auto* item = new MenuItem(it.label, [this, action = it.action]() {
                // Plain row: run the action and close the whole menu tree.
                // (Rows with a submenu handle the toggle themselves in
                // MenuItem::OnPointerDown, never executing a parent action.)
                if (action) action();
                CloseAllMenus();
            }, it.disabled);
            if (it.subMenu) item->SetSubMenu(this, it.subMenu);
            if (it.subMenu && m_SubMenuIcon) item->SetSubMenuIcon(m_SubMenuIcon);
            item->SetFont(m_Font);
            AddChild(item);
            m_Rows.push_back(item);
        }
    }
}

void UIContextMenu::OpenAt(const Vector2& canvasPos)
{
    RebuildItems();
    const Vector2 content = GetContentSize();
    // Size the menu to the widest item's text, clamped to the user limits.
    const float menuW = std::clamp(content.x, m_MinWidth, m_MaxWidth);
    const float menuH = content.y;
    // Truncate overflowing labels to the clamped width ("…").
    for (auto* row : m_Rows) {
        if (auto* item = dynamic_cast<MenuItem*>(row))
            item->ApplyWidthLimit(menuW - 8.0f); // menu L/R padding = 4 each
    }
    Vector2 pos = canvasPos;
    // Clamp so the menu never overflows the window.
    if (GetParent()) {
        const auto& pc = GetParent()->GetComputedRect();
        pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, pc.z - menuW));
        pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, pc.w - menuH));
    }
    GetRect().anchor = AnchorSet::TopLeft();
    GetRect().offset = {pos.x, pos.y, pos.x + menuW, pos.y + menuH};
    SetActive(true);
    m_Open = true;
    // The Press that triggered this open (e.g. a UIMenuBarItem click) must not
    // be interpreted as an outside click that closes the menu in the same frame.
    m_IgnoreOutsideClick = true;
}

bool UIContextMenu::HitTestPoint(const Vector2& p) const
{
    const auto& cr = GetComputedRect();
    if (p.x >= cr.x && p.x <= cr.x + cr.z && p.y >= cr.y && p.y <= cr.y + cr.w)
        return true;
    // Clicks inside an open submenu belong to this menu tree.
    if (m_OpenSubMenu && m_OpenSubMenu->IsActive() && m_OpenSubMenu->HitTestPoint(p))
        return true;
    return false;
}

void UIContextMenu::Close()
{
    if (!m_Open) return;
    m_Open = false;
    SetActive(false);
    CloseSubMenu();
}

// ============================== Submenu management ==============================

void UIContextMenu::OpenSubMenu(MenuItem* item)
{
    if (!item || !item->GetSubMenu()) return;
    UIContextMenu* sub = item->GetSubMenu();
    if (m_OpenSubMenu == sub) return;

    // Close any other open submenu first (one open at a time per level).
    if (m_OpenSubMenu && m_OpenSubMenu != sub)
        m_OpenSubMenu->Close();
    m_OpenSubMenu = sub;

    // The submenu must live on the canvas (overlay) to render above the dock.
    if (!sub->GetParent()) {
        for (UIElement* e = this; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { c->AddChild(sub); break; }
        }
    }

    // Defensive font propagation: a submenu added after SetFont (menu bar case)
    // must still render its rows.
    if (sub->GetFont() == nullptr && m_Font) sub->SetFont(m_Font);
    if (!sub->GetSubMenuIcon() && m_SubMenuIcon) sub->SetSubMenuIcon(m_SubMenuIcon);

    // Position to the right of the row, vertically aligned to the row TOP (the
    // row already includes the parent menu's top padding). Offset up by the
    // submenu's own top padding so its FIRST row lines up with this row.
    const auto& cr = item->GetComputedRect();
    const Vector2 pos = { cr.x + cr.z, cr.y - sub->GetPaddingTop() };
    sub->OpenAt(pos);
}

void UIContextMenu::CloseSubMenu()
{
    if (!m_OpenSubMenu) return;
    UIContextMenu* sub = m_OpenSubMenu;
    m_OpenSubMenu = nullptr;
    sub->Close();
}

void UIContextMenu::CloseAllMenus()
{
    // Close this menu and every ancestor (used when a plain action runs).
    if (m_OwnerMenu) m_OwnerMenu->CloseAllMenus();
    Close();
}

// ============================== UIContextMenu::MenuItem ==============================

UIContextMenu::MenuItem::MenuItem(const std::string& label, std::function<void()> activate, bool disabled)
    : m_Activate(std::move(activate)), m_Disabled(disabled)
{
    SetName("CtxItem");
    SetLayoutMode(LayoutMode::Row);
    // Horizontal margin so the hover background extends 8px beyond the text.
    SetPadding(8.0f, 3.0f, 8.0f, 3.0f);
    SetColor(m_BgNormal);
    SetSizePolicy(SizePolicy::Fill);

    m_Label = new UILabel();
    m_Label->SetName("CtxLabel");
    m_Label->SetText(label);
    m_Label->SetSizePolicy(SizePolicy::Fill);
    // NOT hit-testable: otherwise the canvas hover lands on the label (the
    // deepest child) and the MenuItem's OnPointerEnter/Exit never fire, so the
    // row would never highlight on hover. The row is the hover/click target.
    m_Label->SetHitTestable(false);
    AddChild(m_Label);
    UpdateColors();
}

UIContextMenu::MenuItem::~MenuItem()
{
    if (m_Label) {
        RemoveChild(m_Label);
        delete m_Label;
        m_Label = nullptr;
    }
    if (m_ArrowImage) {
        RemoveChild(m_ArrowImage);
        delete m_ArrowImage;
        m_ArrowImage = nullptr;
    }
    m_ArrowTexture.reset();
}

void UIContextMenu::MenuItem::SetSubMenu(UIContextMenu* owner, UIContextMenu* sub)
{
    m_Owner = owner;
    m_SubMenu = sub;

    // Right-pointing arrow image (PNG via UITextureCache, crisp at any DPI).
    m_ArrowImage = new UIImage();
    m_ArrowImage->SetName("CtxArrow");
    m_ArrowImage->SetMinSize({kArrowSize, kArrowSize});
    m_ArrowImage->SetSizePolicy(SizePolicy::Fixed);
    // Not hit-testable so the row stays the hover/click target (like the label).
    m_ArrowImage->SetHitTestable(false);
    AddChild(m_ArrowImage);
    UpdateColors();
}

void UIContextMenu::MenuItem::SetSubMenuIcon(std::shared_ptr<Texture2D> icon)
{
    m_ArrowTexture = std::move(icon);
    if (m_ArrowImage) {
        m_ArrowImage->SetTexture(m_ArrowTexture ? m_ArrowTexture.get() : nullptr);
        m_ArrowImage->SetActive(m_ArrowTexture != nullptr);
    }
}

bool UIContextMenu::MenuItem::OwnsChild(const UIElement* child) const
{
    return child == m_Label || child == m_ArrowImage;
}

Vector2 UIContextMenu::MenuItem::GetMinSize() const
{
    // The menu sizes to the widest item: report the label's natural text width
    // (cached by UILabel) plus the 8px L/R padding. Clamped by the menu later.
    float w = m_Label ? m_Label->GetMinSize().x : 0.0f;
    if (m_ArrowImage && m_ArrowImage->IsActive()) w += kArrowSize + 8.0f; // arrow + gap
    return { std::max(120.0f, w + 16.0f), 22.0f };
}

void UIContextMenu::MenuItem::ApplyWidthLimit(float itemWidth)
{
    if (!m_Label || !m_Label->GetFont()) return;
    const float arrow = (m_ArrowImage && m_ArrowImage->IsActive()) ? (kArrowSize + 8.0f) : 0.0f;
    const float labelMax = std::max(0.0f, itemWidth - 16.0f - arrow); // 8px L + 8px R padding
    const std::string full = m_Label->GetText();
    if (m_Label->GetMinSize().x <= labelMax)
        return; // fits — no truncation
    // Binary search the longest prefix that fits with the trailing ellipsis.
    const std::string ell = "\u2026";
    int lo = 1, hi = (int)full.size(), best = 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const std::string cand = full.substr(0, (size_t)mid) + ell;
        if (m_Label->GetFont()->MeasureText(cand, 0.0f).x <= labelMax) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if ((size_t)best < full.size())
        m_Label->SetText(full.substr(0, (size_t)best) + ell);
}

void UIContextMenu::MenuItem::SetFont(Font* font)
{
    if (m_Label) m_Label->SetFont(font);
}

void UIContextMenu::MenuItem::UpdateColors()
{
    // Disabled rows never highlight (m_Hovered is ignored for them).
    SetColor(m_Hovered && !m_Disabled ? m_BgHover : m_BgNormal);
    if (m_Label) m_Label->SetColor(m_Disabled ? m_TextDisabled : m_TextNormal);
    if (m_ArrowImage) m_ArrowImage->SetColor(m_Disabled ? m_TextDisabled : m_TextNormal);
}

void UIContextMenu::MenuItem::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
    // Center the arrow vertically in the row (Row layout is top-aligned).
    if (!m_ArrowImage || !m_ArrowImage->IsActive()) return;
    const auto& cr = GetComputedRect();
    const float arrowTop = (cr.w - kArrowSize) * 0.5f;
    m_ArrowImage->GetRect().anchor = {0, 0, 0, 0};
    m_ArrowImage->GetRect().offset = {cr.z - kArrowSize - 8.0f, arrowTop, cr.z - 8.0f, arrowTop + kArrowSize};
    m_ArrowImage->ComputeLayout({kArrowSize, kArrowSize}, {cr.x, cr.y});
}

bool UIContextMenu::MenuItem::OnPointerDown(const Vector2&)
{
    if (m_Disabled)
        return true; // consume but do nothing (menu stays open)
    if (m_SubMenu && m_Owner) {
        // Clicking a submenu row toggles the submenu (never a parent action).
        if (m_Owner->GetOpenSubMenu() == m_SubMenu) m_Owner->CloseSubMenu();
        else m_Owner->OpenSubMenu(this);
        return true;
    }
    if (m_Activate)
        m_Activate(); // executes the action + closes the whole tree
    return true;
}

void UIContextMenu::MenuItem::OnPointerEnter(const Vector2&)
{
    if (m_Disabled)
        return; // disabled rows never highlight (no background, only grayed text)
    m_Hovered = true;
    UpdateColors();
    // Hover opens the submenu (industry standard for nested menus).
    if (m_SubMenu && m_Owner)
        m_Owner->OpenSubMenu(this);
}

void UIContextMenu::MenuItem::OnPointerExit()
{
    m_Hovered = false;
    UpdateColors();
    // When leaving a submenu row the submenu stays open (the pointer can move
    // into it); it closes via click-outside, ESC or hovering another submenu row.
}

} // namespace Leir