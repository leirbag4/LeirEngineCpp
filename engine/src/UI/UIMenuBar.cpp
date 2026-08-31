#include "LeirEngine/UI/UIMenuBar.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include <algorithm>

namespace Leir {

// ============================== UIMenuBarItem ==============================

UIMenuBarItem::UIMenuBarItem(const std::string& label)
    : m_Text(label)
{
    SetName("MenuBarItem");
    SetLayoutMode(LayoutMode::Row);
    SetPadding(10.0f, 4.0f, 10.0f, 4.0f);
    SetColor(m_BgNormal);
    SetSizePolicy(SizePolicy::Fixed);

    m_Label = new UILabel();
    m_Label->SetName("MenuBarLabel");
    m_Label->SetText(label);
    m_Label->SetSizePolicy(SizePolicy::Content);
    // Not hit-testable: the item is the hover/click target (like menu rows).
    m_Label->SetHitTestable(false);
    AddChild(m_Label);

    m_Menu = new UIContextMenu();
    m_Menu->SetName("MenuBarDropdown");
    m_Menu->SetActive(false);
}

UIMenuBarItem::~UIMenuBarItem()
{
    if (m_Label) {
        RemoveChild(m_Label);
        delete m_Label;
        m_Label = nullptr;
    }
    if (m_Menu) {
        // The dropdown is a canvas child (overlay); detach before deleting so
        // the canvas never holds a dangling pointer.
        if (m_Menu->GetParent()) m_Menu->GetParent()->RemoveChild(m_Menu);
        delete m_Menu;
        m_Menu = nullptr;
    }
}

void UIMenuBarItem::SetText(const std::string& text)
{
    m_Text = text;
    if (m_Label) m_Label->SetText(text);
}

const std::string& UIMenuBarItem::GetText() const
{
    return m_Text;
}

void UIMenuBarItem::SetFont(Font* font)
{
    m_Font = font;
    if (m_Label) m_Label->SetFont(font);
    if (m_Menu) m_Menu->SetFont(font);
}

void UIMenuBarItem::SetColors(const Vector4& normal, const Vector4& hover)
{
    m_BgNormal = normal;
    m_BgHover = hover;
    SetColor(m_BgNormal);
}

void UIMenuBarItem::OpenMenu()
{
    if (!m_Menu) return;
    // Add the dropdown to the canvas (overlay) on first open.
    if (!m_Menu->GetParent()) {
        for (UIElement* e = this; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { c->AddChild(m_Menu); break; }
        }
    }
    const auto& cr = GetComputedRect();
    m_Menu->OpenAt({cr.x, cr.y + cr.w}); // below the item
    if (m_OnToggle) m_OnToggle(true);
}

void UIMenuBarItem::CloseMenu()
{
    if (m_Menu) m_Menu->Close();
    if (m_OnToggle) m_OnToggle(false);
}

bool UIMenuBarItem::OnPointerDown(const Vector2&)
{
    if (m_Menu && m_Menu->IsActive()) CloseMenu();
    else OpenMenu();
    return true;
}

void UIMenuBarItem::OnPointerEnter(const Vector2&)
{
    m_Hovered = true;
    if (m_Menu && m_Menu->IsActive()) return; // keep open highlight
    SetColor(m_BgHover);
}

void UIMenuBarItem::OnPointerExit()
{
    m_Hovered = false;
    if (m_Menu && m_Menu->IsActive()) return; // keep open highlight
    SetColor(m_BgNormal);
}

Vector2 UIMenuBarItem::GetMinSize() const
{
    const float w = m_Label ? m_Label->GetMinSize().x : 0.0f;
    return { w + 20.0f, 28.0f }; // 10px L/R padding
}

bool UIMenuBarItem::OwnsChild(const UIElement* child) const
{
    return child == m_Label;
}

void UIMenuBarItem::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
    // Refresh the highlight: open state beats hover.
    if (m_Menu && m_Menu->IsActive()) {
        if (m_Label) m_Label->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
        SetColor({0.28f, 0.28f, 0.34f, 1.0f});
    } else {
        if (m_Label) m_Label->SetColor(m_TextColor);
        SetColor(m_Hovered ? m_BgHover : m_BgNormal);
    }
}

// ============================== UIMenuBar ==============================

UIMenuBar::UIMenuBar()
{
    SetName("MenuBar");
    SetColor({0.09f, 0.09f, 0.11f, 1.0f});
    SetLayoutMode(LayoutMode::Row);
    SetPadding(6.0f, 0.0f, 6.0f, 0.0f);
    SetSpacing(0.0f);
}

UIMenuBar::~UIMenuBar()
{
    for (auto* item : m_Items) {
        RemoveChild(item);
        delete item;
    }
    m_Items.clear();
    m_OpenItem = nullptr;
}

void UIMenuBar::SetFont(Font* font)
{
    m_Font = font;
    for (auto* item : m_Items) item->SetFont(font);
}

void UIMenuBar::SetSubMenuIcon(std::shared_ptr<Texture2D> icon)
{
    for (auto* item : m_Items) item->SetSubMenuIcon(icon);
}

UIMenuBarItem* UIMenuBar::AddItem(const std::string& label)
{
    auto* item = new UIMenuBarItem(label);
    AddItem(item);
    return item;
}

UIMenuBarItem* UIMenuBar::AddItem(UIMenuBarItem* item)
{
    if (!item) return nullptr;
    item->SetFont(m_Font);
    item->SetOnToggle([this, item](bool open) { OnItemToggle(item, open); });
    AddChild(item);
    m_Items.push_back(item);
    return item;
}

void UIMenuBar::RemoveItem(UIMenuBarItem* item)
{
    auto it = std::find(m_Items.begin(), m_Items.end(), item);
    if (it != m_Items.end()) {
        if (m_OpenItem == item) m_OpenItem = nullptr;
        RemoveChild(item);
        m_Items.erase(it);
    }
}

UIMenuBarItem* UIMenuBar::GetItem(int index) const
{
    if (index < 0 || index >= (int)m_Items.size()) return nullptr;
    return m_Items[(size_t)index];
}

void UIMenuBar::CloseMenus()
{
    for (auto* item : m_Items) item->CloseMenu();
    m_OpenItem = nullptr;
}

Vector2 UIMenuBar::GetMinSize() const
{
    return {0.0f, 28.0f};
}

bool UIMenuBar::OwnsChild(const UIElement* child) const
{
    return std::find(m_Items.begin(), m_Items.end(), child) != m_Items.end();
}

void UIMenuBar::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
    // If the open item's menu was closed by an outside click / ESC (its own
    // EventQueue hook), sync the open-state highlight.
    if (m_OpenItem && !m_OpenItem->IsMenuOpen()) {
        UIMenuBarItem* item = m_OpenItem;
        m_OpenItem = nullptr;
        if (m_OnItemOpened) m_OnItemOpened(item, false);
    }
}

bool UIMenuBar::OnPointerDown(const Vector2&)
{
    // Click on empty bar area closes all menus (consumed so it doesn't fall
    // through to the dock below).
    if (m_OpenItem) {
        CloseMenus();
        return true;
    }
    return false;
}

void UIMenuBar::OnItemToggle(UIMenuBarItem* item, bool open)
{
    if (open) {
        if (m_OpenItem && m_OpenItem != item)
            m_OpenItem->CloseMenu();
        m_OpenItem = item;
    } else if (m_OpenItem == item) {
        m_OpenItem = nullptr;
    }
    if (m_OnItemOpened) m_OnItemOpened(item, open);
}

} // namespace Leir