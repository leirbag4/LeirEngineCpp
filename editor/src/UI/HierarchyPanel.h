#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/Core/CoreObject.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leir {
class UITreeView;
class UITreeViewItem;
class Texture2D;
class Font;
class UIButton;
class UITextInput;
class UIContextMenu;
namespace RHI { class RenderBackend; }
}

// Real scene hierarchy panel (Fase 0.2, see TODO_HIERARCHY_SYSTEM.md). Replaces
// the placeholder "Hierarchy" panel: a single virtualized UITreeView that shows
// the active scene Unity-style — every scene ROOT is a top-level item (in
// m_Objects order) and children recurse; there are NO family group headers, the
// family is shown only via each item's icon. Any mix of families coexists at
// level 0; the FAMILY RULE (a parent only accepts children of its own family) is
// enforced by the drag callback and, in Fase 1, by CoreObject::SetParent.
//
// Layout (Paso 2.5): a Column with a header bar (#55555E) on top and the tree
// below. The header holds a "+" add button (placeholder for the future
// UIContextMenu with Object2D/Object3D/UIElement) and a filter UITextInput
// (Godot-style search: nodes that match OR have a matching descendant stay,
// branches without matches are hidden).
//
// Performance (thousands of objects): Refresh() rebuilds ONLY when a cheap
// structural signature (object count + parent wiring, FNV-1a over raw parent
// pointers — no allocation) changes. The filter triggers a rebuild only on text
// change. Rebuilds are O(N); the per-frame cost with a static scene is O(N)
// hash + an O(N) name-sync (short string compares). No per-frame string build.
//
// Ownership / teardown:
//   * The UITreeView is a normal UI child of this panel.
//   * Tree ITEMS are caller-owned (UITreeView never deletes them). Rebuilds
//     during the panel's life free the previous items here (m_OwnedItems). The
//     editor's DeleteUiSubtree(m_HierarchyPanel) handles final teardown: it
//     recurses into the tree's internal viewport and deletes every item.
//     So this destructor does NOT touch the tree or the items (no double free).
//
// Family detection is a dynamic_cast (Object3D/Object2D/else -> UI) until the
// engine's ObjectFamily enum + UINode land (Fase 1).
class HierarchyPanel : public Leir::UIPanel {
public:
    HierarchyPanel();
    ~HierarchyPanel() override;

    void SetFont(Leir::Font* font);
    void SetBackend(Leir::RHI::RenderBackend* backend);
    void SetContentScale(float scale);

    void Refresh();

    Leir::UITreeView* GetTreeView() const { return m_TreeView; }

    // Selection (Fase 0.2 Paso 3): bidirectional sync with the editor's gizmo/
    // inspector. The panel maps UITreeViewItem* <-> CoreObject* via m_ItemMap
    // (family-root items are not in the map and never participate).
    void SetOnSelectionChanged(std::function<void(const std::vector<Leir::CoreObject*>&)> cb)
    { m_OnSelectionChanged = std::move(cb); }
    std::vector<Leir::CoreObject*> GetSelectedObjects() const;
    void SetSelectedObjects(const std::vector<Leir::CoreObject*>& objs);

    // "+" context menu actions (wired by the editor): creating scene objects.
    void SetOnAddObject3D(std::function<void()> cb) { m_OnAddObject3D = std::move(cb); }
    void SetOnAddObject2D(std::function<void()> cb) { m_OnAddObject2D = std::move(cb); }

    Leir::Vector2 GetMinSize() const override;

private:
    enum class Family { Object3D, Object2D, UI };
    void NotifySelectionChanged();
    Leir::CoreObject* ObjectOfItem(Leir::UITreeViewItem* item) const;
    static bool IsDescendantOf(Leir::CoreObject* ancestor, Leir::CoreObject* node);
    static Family FamilyOf(Leir::CoreObject* obj);
    static const char* FamilyName(Family f);
    void EnsureIcons();
    void ApplyIcons();
    void Reconcile();
    size_t BuildSignature() const;

    Leir::UIPanel* m_Header = nullptr;
    Leir::UIButton* m_AddButton = nullptr;
    Leir::UITextInput* m_FilterInput = nullptr;
    Leir::UITreeView* m_TreeView = nullptr;
    Leir::RHI::RenderBackend* m_Backend = nullptr;
    Leir::Font* m_Font = nullptr;
    float m_ContentScale = 1.0f;
    bool m_IconsLoaded = false;
    std::string m_FilterText; // raw filter text (re-applied to the tree after rebuilds)
    size_t m_LastSignature = 0;
    std::unordered_map<Leir::CoreObject*, Leir::UITreeViewItem*> m_ItemMap;
    std::function<void(const std::vector<Leir::CoreObject*>&)> m_OnSelectionChanged;
    Leir::UIContextMenu* m_AddMenu = nullptr;
    std::function<void()> m_OnAddObject3D;
    std::function<void()> m_OnAddObject2D;
    std::shared_ptr<Leir::Texture2D> m_Icon3D;
    std::shared_ptr<Leir::Texture2D> m_Icon2D;
    std::shared_ptr<Leir::Texture2D> m_IconUI;
};