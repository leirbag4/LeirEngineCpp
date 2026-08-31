#pragma once

/**
 * @file UIMenuBar.h
 * @brief Top menu bar: horizontal row of UIMenuBarItem (File, Help, etc.)
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIContextMenu.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class Font;
class Texture2D;

/**
 * @brief One clickable item in the top menu bar (File, Help, …).
 * @ingroup UI
 * @details Owns a UIContextMenu as its dropdown. On click the dropdown is
 *  positioned below the item on the overlay layer. The parent UIMenuBar
 *  ensures at most one item is open at a time.
 */
class LEIR_API UIMenuBarItem : public UIPanel {
public:
    /**
     * @brief Constructs a menu bar item.
     * @param[in] label Display text.
     */
    UIMenuBarItem(const std::string& label);

    /**
     * @brief Destroys the item and its dropdown menu.
     */
    ~UIMenuBarItem() override;

    /**
     * @brief Sets the item text.
     * @param[in] text UTF-8 text.
     */
    void SetText(const std::string& text);

    /**
     * @brief Returns the item text.
     * @return Text string.
     */
    const std::string& GetText() const;

    /**
     * @brief Sets the font for the label and the dropdown.
     * @param[in] font Font pointer (not owned).
     */
    void SetFont(Font* font);

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Sets background colors for the normal and hovered states.
     * @param[in] normal Normal state color.
     * @param[in] hover Hovered state color.
     */
    void SetColors(const Vector4& normal, const Vector4& hover);

    /**
     * @brief Sets the text color.
     * @param[in] color RGBA color.
     */
    void SetTextColor(const Vector4& color) { m_TextColor = color; if (m_Label) m_Label->SetColor(color); }

    /**
     * @brief Returns the text color.
     * @return Current color.
     */
    const Vector4& GetTextColor() const { return m_TextColor; }

    /**
     * @brief Returns the normal background color.
     * @return Color.
     */
    const Vector4& GetBgNormal() const { return m_BgNormal; }

    /**
     * @brief Returns the hover background color.
     * @return Color.
     */
    const Vector4& GetBgHover() const { return m_BgHover; }

    // ---------- Dropdown builder ----------

    /**
     * @brief Returns the dropdown menu (UIContextMenu) owned by this item.
     * @return Menu pointer (always valid).
     */
    UIContextMenu* GetMenu() const { return m_Menu; }

    /**
     * @brief Adds a menu item to the dropdown.
     * @param[in] label Item label.
     * @param[in] action Callback on click.
     */
    void AddMenuItem(const std::string& label, std::function<void()> action)
    { if (m_Menu) m_Menu->AddItem(label, std::move(action)); }

    /**
     * @brief Adds a separator to the dropdown.
     */
    void AddMenuSeparator() { if (m_Menu) m_Menu->AddSeparator(); }

    /**
     * @brief Adds a disabled item to the dropdown.
     * @param[in] label Item label.
     */
    void AddMenuDisabled(const std::string& label)
    { if (m_Menu) m_Menu->AddItemDisabled(label); }

    /**
     * @brief Adds a submenu item to the dropdown.
     * @param[in] label Item label.
     * @param[in] sub Submenu (owned by the dropdown).
     */
    void AddSubMenu(const std::string& label, UIContextMenu* sub)
    { if (m_Menu) m_Menu->AddSubMenu(label, sub); }

    /**
     * @brief Sets the submenu arrow icon for the dropdown (and its submenus).
     * @param[in] icon Arrow texture (shared, from UITextureCache).
     */
    void SetSubMenuIcon(std::shared_ptr<Texture2D> icon)
    { if (m_Menu) m_Menu->SetSubMenuIcon(std::move(icon)); }

    // ---------- Open/close ----------

    /**
     * @brief Opens the dropdown menu below this item.
     */
    void OpenMenu();

    /**
     * @brief Closes the dropdown menu.
     */
    void CloseMenu();

    /**
     * @brief Whether the dropdown is open.
     * @return True if open.
     */
    bool IsMenuOpen() const { return m_Menu && m_Menu->IsActive(); }

    /**
     * @brief Sets a callback for open/close state changes.
     * @param[in] cb Called with true=opened, false=closed.
     */
    void SetOnToggle(std::function<void(bool open)> cb) { m_OnToggle = std::move(cb); }

    // ---------- Input ----------

    /**
     * @brief Handles pointer press: toggles the dropdown.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

    /**
     * @brief Called when pointer enters.
     * @param[in] pos Pointer position.
     */
    void OnPointerEnter(const Vector2& pos) override;

    /**
     * @brief Called when pointer leaves.
     */
    void OnPointerExit() override;

    // ---------- Layout ----------

    /**
     * @brief Returns minimum size for the menu bar row.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Returns whether this item owns the child (for subtree teardown).
     * @param[in] child Child to query.
     * @return True if owned.
     */
    bool OwnsChild(const UIElement* child) const override;

protected:
    void OnLayoutComputed() override;

private:
    std::string m_Text;                                     ///< Item text.
    Font* m_Font = nullptr;                                 ///< Font (not owned).
    UILabel* m_Label = nullptr;                             ///< Label (owned).
    UIContextMenu* m_Menu = nullptr;                        ///< Dropdown menu (owned).
    std::function<void(bool)> m_OnToggle;                   ///< Toggle callback.
    bool m_Hovered = false;                                 ///< Hovered flag.
    Vector4 m_BgNormal = {0.12f, 0.12f, 0.14f, 1.0f};         ///< Normal background.
    Vector4 m_BgHover = {0.22f, 0.22f, 0.26f, 1.0f};          ///< Hover background.
    Vector4 m_TextColor = {0.88f, 0.88f, 0.90f, 1.0f};         ///< Text color.
};

// ============================================================================

/**
 * @brief Top menu bar: horizontal container of UIMenuBarItem (File, Help, …).
 * @ingroup UI
 * @details Sibling of ToolbarPanel, positioned at the very top of the editor
 *  (above the toolbar). Manages at most one open dropdown at a time.
 */
class LEIR_API UIMenuBar : public UIPanel {
public:
    /**
     * @brief Constructs an empty menu bar.
     */
    UIMenuBar();

    /**
     * @brief Destroys the bar and its items.
     */
    ~UIMenuBar() override;

    /**
     * @brief Sets the font for all items.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font);

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Creates a new item with the given label and adds it to the bar.
     * @param[in] label Item label.
     * @return The new item.
     */
    UIMenuBarItem* AddItem(const std::string& label);

    /**
     * @brief Adds an already-created item to the bar (takes ownership).
     * @param[in] item Item to add.
     */
    UIMenuBarItem* AddItem(UIMenuBarItem* item);

    /**
     * @brief Removes an item from the bar (does not delete it).
     * @param[in] item Item to remove.
     */
    void RemoveItem(UIMenuBarItem* item);

    /**
     * @brief Returns the item at the given index.
     * @param[in] index 0-based index.
     * @return Item pointer or nullptr.
     */
    UIMenuBarItem* GetItem(int index) const;

    /**
     * @brief Returns the number of items.
     * @return Count.
     */
    int GetItemCount() const { return (int)m_Items.size(); }

    /**
     * @brief Returns the items vector.
     * @return Const reference.
     */
    const std::vector<UIMenuBarItem*>& GetItems() const { return m_Items; }

    /**
     * @brief Closes all open menus in the bar.
     */
    void CloseMenus();

    /**
     * @brief Returns the currently open item, or nullptr.
     * @return Open item pointer.
     */
    UIMenuBarItem* GetOpenItem() const { return m_OpenItem; }

    /**
     * @brief Sets a callback when an item is opened/closed.
     * @param[in] cb Callback (item, isOpen).
     */
    void SetOnItemOpened(std::function<void(UIMenuBarItem*, bool)> cb) { m_OnItemOpened = std::move(cb); }

    /**
     * @brief Sets the submenu arrow icon for all items (propagates to their menus).
     * @param[in] icon Arrow texture (shared, from UITextureCache).
     */
    void SetSubMenuIcon(std::shared_ptr<Texture2D> icon);

    // ---------- Layout ----------

    /**
     * @brief Returns minimum size (~28px height).
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Whether this bar owns the child (for subtree teardown).
     * @param[in] child Child to query.
     * @return True if owned.
     */
    bool OwnsChild(const UIElement* child) const override;

    /**
     * @brief Called when the menu bar is laid out.
     */
    void OnLayoutComputed() override;

    /**
     * @brief Click on empty area of the bar closes all menus.
     * @param[in] pos Pointer position.
     * @return True if consumed.
     */
    bool OnPointerDown(const Vector2& pos) override;

private:
    // Called by UIMenuBarItem toggle.
    friend class UIMenuBarItem;
    void OnItemToggle(UIMenuBarItem* item, bool open);

    Font* m_Font = nullptr;                         ///< Font (not owned).
    std::vector<UIMenuBarItem*> m_Items;            ///< Owned items.
    UIMenuBarItem* m_OpenItem = nullptr;            ///< Currently open item.
    std::function<void(UIMenuBarItem*, bool)> m_OnItemOpened; ///< Item open callback.
};

} // namespace Leir