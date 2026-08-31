#pragma once

/**
 * @file UIContextMenu.h
 * @brief Context menu / popup with clickable rows (overlay layer).
 * @ingroup UI
 */

#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UIImage.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class UILabel;
class Font;
class Texture2D;

/**
 * @brief Context menu / popup shown on demand (right-click or hierarchy "+").
 * @ingroup UI
 * @details A Column of clickable rows on the overlay layer; closes on item
 *  click, click outside or ESC. Must be added as the last child of a canvas.
 */
class LEIR_API UIContextMenu : public UIPanel {
public:
    /**
     * @brief Menu item descriptor.
     * @ingroup UI
     */
    struct Item {
        std::string label;                  ///< Display label.
        std::function<void()> action;       ///< Action on click.
        bool disabled = false;              ///< Disabled flag.
        bool separator = false;             ///< Separator flag.
        UIContextMenu* subMenu = nullptr;   ///< Submenu opened on hover/click (owned).
    };

    /**
     * @brief Constructs an empty context menu.
     */
    UIContextMenu();

    /**
     * @brief Destroys the menu and its rows.
     */
    ~UIContextMenu() override;

    /**
     * @brief Adds an item.
     * @param[in] label Display label.
     * @param[in] action Action on click.
     */
    void AddItem(const std::string& label, std::function<void()> action);

    /**
     * @brief Adds a separator.
     */
    void AddSeparator();

    /**
     * @brief Adds a disabled item.
     * @param[in] label Label.
     */
    void AddItemDisabled(const std::string& label);

    /**
     * @brief Adds a submenu item (renders a "›" arrow, opens on hover/click).
     * @details Takes ownership of subMenu; it is deleted with this menu. The
     *  submenu is opened to the right of the row, clamped to the canvas.
     * @param[in] label Row label.
     * @param[in] subMenu Child menu to open.
     */
    void AddSubMenu(const std::string& label, UIContextMenu* subMenu);

    /**
     * @brief Sets minimum width.
     * @param[in] v Width in logical pixels (clamped >= 40).
     */
    void SetMinWidth(float v) { m_MinWidth = std::max(40.0f, v); if (m_MaxWidth < m_MinWidth) m_MaxWidth = m_MinWidth; }

    /**
     * @brief Sets maximum width.
     * @param[in] v Width in logical pixels.
     */
    void SetMaxWidth(float v) { m_MaxWidth = std::max(m_MinWidth, v); }

    /**
     * @brief Returns minimum width.
     * @return Min width.
     */
    float GetMinWidth() const { return m_MinWidth; }

    /**
     * @brief Returns maximum width.
     * @return Max width.
     */
    float GetMaxWidth() const { return m_MaxWidth; }

    /**
     * @brief Opens the menu at a canvas position.
     * @param[in] canvasPos Position in canvas coordinates.
     */
    void OpenAt(const Vector2& canvasPos);

    /**
     * @brief Closes the menu.
     */
    void Close();

    /**
     * @brief Sets the font for items.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font);

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Sets the submenu arrow icon (PNG via UITextureCache) applied to
     * all rows with a submenu. Propagated to owned submenus too.
     * @param[in] icon Arrow texture (shared, cached).
     */
    void SetSubMenuIcon(std::shared_ptr<Texture2D> icon);

    /**
     * @brief Returns the submenu arrow icon.
     * @return Texture pointer (shared) or nullptr.
     */
    const std::shared_ptr<Texture2D>& GetSubMenuIcon() const { return m_SubMenuIcon; }

    /**
     * @brief Returns whether the menu owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if owned (rows or submenus).
     */
    bool OwnsChild(const UIElement* child) const override;

    /**
     * @brief Returns minimum size for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    class MenuItem;

private:
    void RebuildItems();
    Vector2 GetContentSize() const override;

    // Submenu management (called by MenuItem).
    void OpenSubMenu(MenuItem* item);
    void CloseSubMenu();
    void CloseAllMenus();
    UIContextMenu* GetOpenSubMenu() const { return m_OpenSubMenu; }

    // Hit-test that includes this menu and any open submenu (recursively), so
    // clicks on a submenu never close an ancestor menu.
    bool HitTestPoint(const Vector2& p) const;

    std::vector<Item> m_Items;                          ///< Item descriptors.
    std::vector<UIElement*> m_Rows;                     ///< Row widgets (MenuItem* and separators).
    std::vector<UIContextMenu*> m_SubMenus;             ///< Owned submenus (added as canvas children).
    UIContextMenu* m_OpenSubMenu = nullptr;             ///< Currently open submenu.
    UIContextMenu* m_OwnerMenu = nullptr;               ///< Menu that owns this submenu (for CloseAllMenus).
    Font* m_Font = nullptr;                             ///< Font (not owned).
    std::shared_ptr<Texture2D> m_SubMenuIcon;           ///< Submenu arrow icon (shared, not owned).
    float m_MinWidth = 160.0f;                          ///< Minimum width.
    float m_MaxWidth = 320.0f;                          ///< Maximum width.
    bool m_Open = false;                                ///< Open flag.
    bool m_IgnoreOutsideClick = false;                  ///< Ignore the next outside Press (the one that opened the menu).
    std::shared_ptr<bool> m_Alive = std::make_shared<bool>(true); ///< Alive flag for EventQueue hooks.

    friend class MenuItem;
};

/**
 * @brief One clickable row: label with hover highlight and activation.
 * @ingroup UI
 */
class UIContextMenu::MenuItem : public UIPanel {
public:
    /**
     * @brief Constructs a menu item.
     * @param[in] label Display label.
     * @param[in] activate Action on click.
     * @param[in] disabled True if disabled.
     */
    MenuItem(const std::string& label, std::function<void()> activate, bool disabled);

    /**
     * @brief Destroys the menu item.
     */
    ~MenuItem() override;

    /**
     * @brief Sets the font.
     * @param[in] font Font pointer.
     */
    void SetFont(Font* font);

    /**
     * @brief Sets the submenu opened by this row (adds the "›" arrow label).
     * @param[in] owner The menu that owns this row.
     * @param[in] sub Submenu to open on hover/click.
     */
    void SetSubMenu(UIContextMenu* owner, UIContextMenu* sub);

    /**
     * @brief Returns the submenu owned by this row.
     * @return Submenu pointer or nullptr.
     */
    UIContextMenu* GetSubMenu() const { return m_SubMenu; }

    /**
     * @brief Sets the submenu arrow icon for this row (PNG via UITextureCache).
     * @param[in] icon Arrow texture (shared, cached).
     */
    void SetSubMenuIcon(std::shared_ptr<Texture2D> icon);

    /**
     * @brief Handles pointer press.
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

    /**
     * @brief Returns whether the item owns the child.
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
     * @brief Applies width limit (truncates label with "…" if overflows).
     * @param[in] itemWidth Available width.
     */
    void ApplyWidthLimit(float itemWidth);

protected:
    void OnLayoutComputed() override;

private:
    void UpdateColors();

    UILabel* m_Label = nullptr;                         ///< Label (owned).
    UIImage* m_ArrowImage = nullptr;                    ///< Submenu arrow image (owned, only if submenu).
    std::shared_ptr<Texture2D> m_ArrowTexture;          ///< Arrow texture (shared, keeps it alive).
    UIContextMenu* m_Owner = nullptr;                   ///< Owning menu (for open/close).
    UIContextMenu* m_SubMenu = nullptr;                 ///< Submenu to open (owned by menu).
    std::function<void()> m_Activate;                   ///< Activate callback.
    bool m_Disabled = false;                            ///< Disabled flag.
    bool m_Hovered = false;                             ///< Hovered flag.
    Vector4 m_BgNormal = {0.0f, 0.0f, 0.0f, 0.0f};        ///< Normal background (transparent).
    Vector4 m_BgHover = {0.263f, 0.159f, 0.527f, 1.0f};   ///< Hover background.
    Vector4 m_TextNormal = {0.88f, 0.88f, 0.90f, 1.0f};   ///< Normal text color.
    Vector4 m_TextDisabled = {0.55f, 0.55f, 0.58f, 1.0f}; ///< Disabled text color.
};

} // namespace Leir
