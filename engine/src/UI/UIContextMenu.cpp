#include "LeirEngine/UI/UIContextMenu.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/Input/EventQueue.h"
#include "LeirEngine/Input/Key.h"
#include <algorithm>

namespace Leir {

// ============================== UIContextMenu ==============================

UIContextMenu::UIContextMenu()
{
    SetName("ContextMenu");
    SetColor({0.13f, 0.13f, 0.15f, 0.96f});
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
            sep->SetMinSize({0.0f, 1.0f});
            sep->SetSizePolicy(SizePolicy::Fill);
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
    const Vector2 size = GetContentSize();
    Vector2 pos = canvasPos;
    // Clamp so the menu never overflows the window.
    if (GetParent()) {
        const auto& pc = GetParent()->GetComputedRect();
        pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, pc.z - size.x));
        pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, pc.w - size.y));
    }
    GetRect().anchor = AnchorSet::TopLeft();
    GetRect().offset = {pos.x, pos.y, pos.x + size.x, pos.y + size.y};
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
    SetColor(m_BgNormal);
    SetMinSize({120.0f, 22.0f});
    SetSizePolicy(SizePolicy::Fill);

    m_Label = new UILabel();
    m_Label->SetName("CtxLabel");
    m_Label->SetText(label);
    m_Label->SetSizePolicy(SizePolicy::Fill);
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
    return {120.0f, 22.0f};
}

void UIContextMenu::MenuItem::SetFont(Font* font)
{
    if (m_Label) m_Label->SetFont(font);
}

void UIContextMenu::MenuItem::UpdateColors()
{
    SetColor(m_Hovered ? m_BgHover : m_BgNormal);
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
    m_Hovered = true;
    UpdateColors();
}

void UIContextMenu::MenuItem::OnPointerExit()
{
    m_Hovered = false;
    UpdateColors();
}

} // namespace Leir