#pragma once

/**
 * @file UIContextMenu.h
 * @brief Context menu / popup with clickable rows (overlay layer).
 * @ingroup UI
 */

#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leir {

class UILabel;
class Font;

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
     * @brief Returns whether the menu owns the child for subtree-delete.
     * @param[in] child Child to query.
     * @return True if owned (rows).
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

    std::vector<Item> m_Items;                          ///< Item descriptors.
    std::vector<UIElement*> m_Rows;                     ///< Row widgets (MenuItem* and separators).
    Font* m_Font = nullptr;                             ///< Font (not owned).
    float m_MinWidth = 160.0f;                          ///< Minimum width.
    float m_MaxWidth = 320.0f;                          ///< Maximum width.
    bool m_Open = false;                                ///< Open flag.
    std::shared_ptr<bool> m_Alive = std::make_shared<bool>(true); ///< Alive flag for EventQueue hooks.
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

private:
    void UpdateColors();

    UILabel* m_Label = nullptr;                         ///< Label (owned).
    std::function<void()> m_Activate;                   ///< Activate callback.
    bool m_Disabled = false;                            ///< Disabled flag.
    bool m_Hovered = false;                             ///< Hovered flag.
    Vector4 m_BgNormal = {0.0f, 0.0f, 0.0f, 0.0f};        ///< Normal background (transparent).
    Vector4 m_BgHover = {0.263f, 0.159f, 0.527f, 1.0f};   ///< Hover background.
    Vector4 m_TextNormal = {0.88f, 0.88f, 0.90f, 1.0f};   ///< Normal text color.
    Vector4 m_TextDisabled = {0.55f, 0.55f, 0.58f, 1.0f}; ///< Disabled text color.
};

} // namespace Leir
