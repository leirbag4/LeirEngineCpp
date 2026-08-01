#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/Dock/DockPanel.h"

namespace Leir {

class DockPane;
class DockManager;
class DockTab;
class Font;

// Row of tabs shown at the top of a DockPane.
class LEIR_API DockTabBar : public UIPanel {
public:
    DockTabBar();
    ~DockTabBar() override;

    void Init(DockPane* pane, DockManager* manager);
    void SetFont(Font* font) { m_Font = font; }
    Font* GetFont() const { return m_Font; }

    DockTab* AddTab(DockPanel* panel);
    // Inserts a tab at a specific index in the row (used for tab reordering).
    DockTab* InsertTab(DockPanel* panel, size_t index);
    void RemoveTab(DockPanel* panel);
    DockTab* FindTab(DockPanel* panel) const;
    Vector2 GetMinSize() const override;

private:
    DockPane* m_Pane = nullptr;
    DockManager* m_Manager = nullptr;
    Font* m_Font = nullptr;
};

// A single tab: label + optional close button. Pointer-down on the label
// activates the tab and starts a dock drag (via DockManager); pointer-down on
// the close area closes the panel.
class LEIR_API DockTab : public UIElement {
public:
    DockTab();
    ~DockTab() override;

    void Setup(DockPanel* panel, DockPane* pane, DockManager* manager, Font* font);
    DockPanel* GetPanel() const { return m_Panel; }
    DockManager* GetManager() const { return m_Manager; }
    Font* GetFont() const { return m_Font; }
    bool IsActive() const;

    Vector2 GetMinSize() const override;

    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;

private:
    DockPanel* m_Panel = nullptr;
    DockPane* m_Pane = nullptr;
    DockManager* m_Manager = nullptr;
    Font* m_Font = nullptr;
};

} // namespace Leir
