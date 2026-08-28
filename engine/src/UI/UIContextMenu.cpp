#include "LeirEngine/UI/UIContextMenu.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Key.h"
#include <algorithm>

namespace Leir {

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
        const auto& cr = GetComputedRect();
        const bool inside = e.position.x >= cr.x && e.position.x <= cr.x + cr.z &&
                            e.position.y >= cr.y && e.position.y <= cr.y + cr.w;
        if (!inside) Close();
    });
    eq.AddKeyHook([this, alive = m_Alive](const KeyEvent& e) {
        if (!*alive || !m_Open) return;
        if (e.key == Key::Escape) Close();
    });
}

UIContextMenu::~UIContextMenu()
{
    *m_Alive = false; // the hooks must never dereference `this` after this point
    RebuildItems();   // frees the rows (RemoveChild + delete)
}

void UIContextMenu::SetFont(Font* font)
{
    m_Font = font;
    for (auto* row : m_Rows) {
        if (auto* item = dynamic_cast<MenuItem*>(row)) item->SetFont(font);
    }
}

void UIContextMenu::AddItem(const std::string& label, std::function<void()> action)
{
    m_Items.push_back({label, std::move(action), false, false});
}

void UIContextMenu::AddSeparator()
{
    m_Items.push_back({"", {}, false, true});
}

void UIContextMenu::AddItemDisabled(const std::string& label)
{
    m_Items.push_back({label, {}, true, false});
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

void UIContextMenu::RebuildItems()
{
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
                if (action) action();
                Close();
            }, it.disabled);
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
}

void UIContextMenu::Close()
{
    if (!m_Open) return;
    m_Open = false;
    SetActive(false);
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
}

bool UIContextMenu::MenuItem::OwnsChild(const UIElement* child) const
{
    return child == m_Label;
}

Vector2 UIContextMenu::MenuItem::GetMinSize() const
{
    // The menu sizes to the widest item: report the label's natural text width
    // (cached by UILabel) plus the 8px L/R padding. Clamped by the menu later.
    const float w = m_Label ? m_Label->GetMinSize().x : 0.0f;
    return { std::max(120.0f, w + 16.0f), 22.0f };
}

void UIContextMenu::MenuItem::ApplyWidthLimit(float itemWidth)
{
    if (!m_Label || !m_Label->GetFont()) return;
    const float labelMax = std::max(0.0f, itemWidth - 16.0f); // 8px L + 8px R padding
    const std::string full = m_Label->GetText();
    if (m_Label->GetMinSize().x <= labelMax)
        return; // fits — no truncation
    // Binary search the longest prefix that fits with the trailing ellipsis.
    const std::string ell = "…";
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
}

bool UIContextMenu::MenuItem::OnPointerDown(const Vector2&)
{
    if (m_Disabled)
        return true; // consume but do nothing (menu stays open)
    if (m_Activate)
        m_Activate(); // executes the action + closes the menu
    return true;
}

void UIContextMenu::MenuItem::OnPointerEnter(const Vector2&)
{
    if (m_Disabled)
        return; // disabled rows never highlight (no background, only grayed text)
    m_Hovered = true;
    UpdateColors();
}

void UIContextMenu::MenuItem::OnPointerExit()
{
    m_Hovered = false;
    UpdateColors();
}

} // namespace Leir