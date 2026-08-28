#pragma once
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

// Context menu / popup shown on demand (right-click, or the hierarchy "+" button).
// A Column of clickable rows on the OVERLAY layer; closes on item click, click
// outside or ESC. It must be added as the LAST child of a canvas so it hit-tests
// first (on top). The caller owns it; the menu owns its rows (OwnsChild).
//
// Click-outside / ESC use persistent EventQueue hooks guarded by an "alive"
// shared flag (so a destroyed menu can never be dereferenced by a late event).
class LEIR_API UIContextMenu : public UIPanel {
public:
    struct Item {
        std::string label;
        std::function<void()> action;
        bool disabled = false;
        bool separator = false;
    };

    UIContextMenu();
    ~UIContextMenu() override;

    void AddItem(const std::string& label, std::function<void()> action);
    void AddSeparator();
    void AddItemDisabled(const std::string& label);

    void OpenAt(const Vector2& canvasPos);
    void Close();

    void SetFont(Font* font);
    Font* GetFont() const { return m_Font; }

    bool OwnsChild(const UIElement* child) const override;
    Vector2 GetMinSize() const override;

    class MenuItem;

private:
    void RebuildItems();
    Vector2 GetContentSize() const override;

    std::vector<Item> m_Items;
    std::vector<UIElement*> m_Rows; // MenuItem* and separator UIPanel*
    Font* m_Font = nullptr;
    bool m_Open = false;
    std::shared_ptr<bool> m_Alive = std::make_shared<bool>(true);
};

// One clickable row: a UIPanel with a label, hover highlight, and activation.
class UIContextMenu::MenuItem : public UIPanel {
public:
    MenuItem(const std::string& label, std::function<void()> activate, bool disabled);
    ~MenuItem() override;
    void SetFont(Font* font);
    bool OnPointerDown(const Vector2& pos) override;
    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;
    bool OwnsChild(const UIElement* child) const override;
    Vector2 GetMinSize() const override;

private:
    void UpdateColors();

    UILabel* m_Label = nullptr;
    std::function<void()> m_Activate;
    bool m_Disabled = false;
    bool m_Hovered = false;
    Vector4 m_BgNormal = {0.20f, 0.20f, 0.23f, 1.0f};
    Vector4 m_BgHover = {0.32f, 0.34f, 0.40f, 1.0f};
    Vector4 m_TextNormal = {0.88f, 0.88f, 0.90f, 1.0f};
    Vector4 m_TextDisabled = {0.55f, 0.55f, 0.58f, 1.0f};
};

} // namespace Leir