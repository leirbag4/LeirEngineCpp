#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/Core/CoreObject.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leir {
class UITreeView;
class UITreeViewItem;
class Texture2D;
class Font;
namespace RHI { class RenderBackend; }
}

// Real scene hierarchy panel (Fase 0.2, see TODO_HIERARCHY_SYSTEM.md). Replaces
// the placeholder "Hierarchy" panel: a single virtualized UITreeView that shows
// the active scene grouped by FAMILY — one collapsible root per family
// ([Object3D] / [Object2D] / [UI]) — with each object's family icon.
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
// engine's ObjectFamily enum + UINode land (Fase 1). Refresh() rebuilds the
// tree only when a cheap signature (object names + parent wiring) changes, so
// a static scene costs nothing per frame.
class HierarchyPanel : public Leir::UIPanel {
public:
    HierarchyPanel();
    ~HierarchyPanel() override;

    void SetFont(Leir::Font* font);
    void SetBackend(Leir::RHI::RenderBackend* backend);
    void SetContentScale(float scale);

    void Refresh();

    Leir::UITreeView* GetTreeView() const { return m_TreeView; }
    Leir::Vector2 GetMinSize() const override;

protected:
    void OnLayoutComputed() override;

private:
    enum class Family { Object3D, Object2D, UI };
    static Family FamilyOf(Leir::CoreObject* obj);
    static const char* FamilyName(Family f);
    void EnsureIcons();
    void RebuildAll();
    std::string BuildSignature() const;

    Leir::UITreeView* m_TreeView = nullptr;
    Leir::RHI::RenderBackend* m_Backend = nullptr;
    float m_ContentScale = 1.0f;
    bool m_IconsLoaded = false;
    std::string m_LastSignature;
    std::vector<Leir::UITreeViewItem*> m_OwnedItems;
    std::unordered_map<Leir::CoreObject*, Leir::UITreeViewItem*> m_ItemMap;
    std::shared_ptr<Leir::Texture2D> m_Icon3D;
    std::shared_ptr<Leir::Texture2D> m_Icon2D;
    std::shared_ptr<Leir::Texture2D> m_IconUI;
};