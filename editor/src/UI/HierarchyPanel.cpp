#include "HierarchyPanel.h"
#include <LeirEngine/UI/UITreeView.h>
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/UITextureCache.h>
#include <LeirEngine/UI/UICanvas.h>
#include <LeirEngine/UI/UIContextMenu.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Core/Log.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

// Hierarchy background = the same gray as the TreeViewDebugPanel reference the
// user wants to match. NOTE: UI colors are LINEAR (UI.frag returns them as-is
// and the swapchain RTV is UNORM_SRGB, so the GPU encodes linear->sRGB on
// store). A literal #55555E value (0.333, 0.333, 0.369) would therefore render
// as ~#9C9CA4; the linear value below is what displays as the dark gray.
static const Leir::Vector4 kHierarchyBg = {0.08f, 0.08f, 0.10f, 0.85f};

HierarchyPanel::HierarchyPanel()
{
    SetName("Hierarchy");
    SetColor(kHierarchyBg);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetPadding(0.0f, 0.0f, 0.0f, 0.0f);
    SetSpacing(0.0f);

    // Header bar: "+" add button (left) + search filter (right), fills the row.
    m_Header = new Leir::UIPanel();
    m_Header->SetName("HierarchyHeader");
    m_Header->SetColor(kHierarchyBg);
    m_Header->SetLayoutMode(Leir::LayoutMode::Row);
    m_Header->SetPadding(4.0f, 4.0f, 4.0f, 4.0f);
    m_Header->SetSpacing(4.0f);
    // SizePolicy::Content: the Column above gives a Fixed UIPanel child its
    // GetMinSize().y height, and UIElement::GetMinSize defaults to {0,0} unless
    // overridden — the header would collapse to ~0px (tiny button/input, no
    // events). Content sizes the header from its children (~32px) automatically.
    m_Header->SetSizePolicy(Leir::SizePolicy::Content);
    AddChild(m_Header);

    m_AddButton = new Leir::UIButton();
    m_AddButton->SetName("HierarchyAddButton");
    m_AddButton->SetText("+");
    m_AddButton->SetTextAlign(Leir::ButtonTextAlign::Center);
    m_AddButton->SetMinSize({22.0f, 22.0f});
    // "+" opens a UIContextMenu (P1, TODO_UI_CONTEXT_MENU.md) to create scene
    // objects. The menu is added to the canvas lazily (the panel isn't in the
    // canvas yet at construction); OpenAt positions it below the button.
    m_AddButton->SetOnClick([this]() {
        if (!m_AddMenu) return;
        if (!m_AddMenu->GetParent()) {
            for (Leir::UIElement* e = this; e; e = e->GetParent()) {
                if (auto* c = dynamic_cast<Leir::UICanvas*>(e)) { c->AddChild(m_AddMenu); break; }
            }
        }
        const auto& cr = m_AddButton->GetComputedRect();
        m_AddMenu->OpenAt({cr.x, cr.y + cr.w});
    });
    m_Header->AddChild(m_AddButton);

    m_AddMenu = new Leir::UIContextMenu();
    m_AddMenu->SetFont(m_Font);
    m_AddMenu->AddItem("Object3D", [this]() {
        if (m_OnAddObject3D) m_OnAddObject3D();
    });
    m_AddMenu->AddItem("Object2D", [this]() {
        // Item present but no-op for now (pending scene support / testing).
        Leir::XConsole::Debug("Hierarchy: Object2D creation pendiente");
    });
    m_AddMenu->AddItemDisabled("UIElement (UINode pendiente)");
    m_AddMenu->SetActive(false);

    m_FilterInput = new Leir::UITextInput();
    m_FilterInput->SetName("HierarchyFilter");
    m_FilterInput->SetPlaceholder("Filter...");
    // Fill: in the header Row the input takes the leftover width, docking its
    // right edge to the header's right edge (follows the dock splitter resize).
    m_FilterInput->SetSizePolicy(Leir::SizePolicy::Fill);
    m_FilterInput->SetOnChange([this](const std::string& text) {
        // Godot-style filtering lives in the CORE (UITreeView::SetFilter): nodes
        // are hidden in place (no rebuild) so typing never flickers and
        // selection/expansion survive. Re-applied after rebuilds from m_FilterText.
        m_FilterText = text;
        if (m_TreeView) m_TreeView->SetFilter(text);
    });
    m_Header->AddChild(m_FilterInput);

    m_TreeView = new Leir::UITreeView();
    m_TreeView->SetName("HierarchyTreeView");
    m_TreeView->SetMultipleSelectionEnabled(true);
    m_TreeView->SetEditable(true); // F2 rename (wired to SetName in Fase 4)
    m_TreeView->SetIconsEnabled(true);
    // Fase 0.2 Paso 3: bridge the tree's selection events to the panel callback
    // (maps items -> CoreObjects, skipping family roots).
    m_TreeView->SetOnSelectedItemsChanged([this](const std::vector<Leir::UITreeViewItem*>&) {
        NotifySelectionChanged();
    });

    // Fase 0.2 Paso 4 — rename (F2): apply to the scene object's name. The tree
    // already updates its own label; the structural signature ignores names, so
    // this never rebuilds/collapses the tree. Family roots (no CoreObject) no-op.
    m_TreeView->SetOnItemRenamed([this](Leir::UITreeViewItem* item, const std::string&, const std::string& newText) {
        if (Leir::CoreObject* obj = ObjectOfItem(item)) obj->SetName(newText);
    });

    // Fase 0.2 Paso 4 — drag&drop (3-zone, Unity-style). Returns whether the scene
    // accepted the change: the tree only mutates its structure when true, so a
    // rejected drop never desyncs tree from scene. targetItem is the row, mode its
    // zone:
    //   * Onto   -> nest into the target (SetParent, append — matches the tree).
    //   * Above  -> insert as a sibling BEFORE the target (post-removal index).
    //   * Below  -> insert as a sibling AFTER the target (post-removal index).
    // There are NO family group headers: every scene root is a top-level item, so
    // the tree's generic behavior already matches the scene. The only guard is the
    // FAMILY RULE: a parent may only have children of its own family (an Object3D
    // can't have an Object2D child). At level 0 (no parent) any family mixes freely.
    // Warnings on every rejection (cross-family / cycle / invalid target).
    m_TreeView->SetOnItemDragged([this](const std::vector<Leir::UITreeViewItem*>& draggedItems,
        Leir::UITreeViewItem* targetItem, Leir::UITreeView::DropMode mode) -> bool {
        if (draggedItems.empty() || !targetItem) return false;
        // Reverse map item -> CoreObject (O(N), once per drop).
        std::unordered_map<Leir::UITreeViewItem*, Leir::CoreObject*> rev;
        rev.reserve(m_ItemMap.size());
        for (const auto& kv : m_ItemMap) rev[kv.second] = kv.first;
        std::vector<Leir::CoreObject*> objs;
        objs.reserve(draggedItems.size());
        for (auto* di : draggedItems) {
            auto it = rev.find(di);
            if (it == rev.end()) return false; // unknown / pseudo item (none exist now)
            objs.push_back(it->second);
        }
        auto tit = rev.find(targetItem);
        if (tit == rev.end()) {
            Leir::XConsole::PrintWarning("Hierarchy: invalid drop target");
            return false;
        }
        Leir::CoreObject* targetObj = tit->second;
        const bool above = (mode == Leir::UITreeView::DropMode::Above);
        // The parent that will own the dragged after the drop: the target itself
        // (Onto) or the target's parent (Above/Below = the dragged becomes a
        // sibling). null = level 0, where any family is allowed.
        Leir::CoreObject* guardParent = (mode == Leir::UITreeView::DropMode::Onto)
            ? targetObj : targetObj->GetParent();
        for (auto* obj : objs) {
            if (obj == targetObj) return false;
            // Cycle: the guard parent must not be a descendant of any dragged.
            if (guardParent && IsDescendantOf(obj, guardParent)) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot reparent '{}' into its own descendant '{}'",
                    obj->GetName(), guardParent->GetName());
                return false;
            }
            // Family rule: a parent only accepts children of its own family.
            if (guardParent && FamilyOf(obj) != FamilyOf(guardParent)) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot reparent '{}' to '{}' — cross-family ({} vs {})",
                    obj->GetName(), guardParent->GetName(),
                    FamilyName(FamilyOf(obj)), FamilyName(FamilyOf(guardParent)));
                return false;
            }
        }
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return false;

        if (mode == Leir::UITreeView::DropMode::Onto) {
            for (auto* obj : objs) obj->SetParent(targetObj, true);
        } else {
            Leir::CoreObject* parentObj = targetObj->GetParent();
            if (parentObj) {
                for (auto* obj : objs) {
                    // Remove obj first so the target's index is post-removal.
                    if (obj->GetParent()) obj->GetParent()->RemoveChild(obj);
                    const auto& kids = parentObj->GetChildren();
                    auto it = std::find(kids.begin(), kids.end(), targetObj);
                    size_t ti = (size_t)(it - kids.begin());
                    size_t idx = above ? ti : ti + 1;
                    parentObj->InsertChildAt(obj, idx);
                }
            } else {
                // Target is a level-0 root: reorder it in m_Objects (the free top
                // level). MoveObject adjusts the index for the dragged's removal.
                for (auto* obj : objs) {
                    obj->SetParent(nullptr, true);
                    const auto& mo = scene->GetObjects();
                    size_t ti = mo.size();
                    for (size_t i = 0; i < mo.size(); ++i)
                        if (mo[i].get() == targetObj) { ti = i; break; }
                    scene->MoveObject(obj, above ? ti : ti + 1);
                }
            }
        }
        // The tree already reflects the change (same relative semantics) -> skip
        // the next rebuild so the list never flickers.
        m_LastSignature = BuildSignature();
        return true;
    });
    AddChild(m_TreeView);
}

HierarchyPanel::~HierarchyPanel()
{
    // The add menu lives on the CANVAS (top overlay), not in this panel's subtree,
    // so DeleteUiSubtree won't free it. Remove + delete it here (its rows are
    // freed by the menu's own destructor).
    if (m_AddMenu) {
        if (m_AddMenu->GetParent())
            m_AddMenu->GetParent()->RemoveChild(m_AddMenu);
        delete m_AddMenu;
        m_AddMenu = nullptr;
    }
}

void HierarchyPanel::SetFont(Leir::Font* font)
{
    m_Font = font;
    if (m_TreeView) m_TreeView->SetFont(font);
    if (m_AddButton) m_AddButton->SetFont(font);
    if (m_FilterInput) m_FilterInput->SetFont(font);
    if (m_AddMenu) m_AddMenu->SetFont(font);
}

void HierarchyPanel::SetBackend(Leir::RHI::RenderBackend* backend)
{
    m_Backend = backend;
}

void HierarchyPanel::SetContentScale(float scale)
{
    if (scale < 1.0f) scale = 1.0f;
    if (std::fabs(scale - m_ContentScale) < 1e-4f) return;
    m_ContentScale = scale;
    m_IconsLoaded = false;   // reload icons at the new DPI
    m_LastSignature = 0;     // force a reconcile so items pick up the new icons
    ApplyIcons();            // re-apply the freshly loaded icons to existing items
}

Leir::Vector2 HierarchyPanel::GetMinSize() const
{
    return {160.0f, 120.0f};
}

std::vector<Leir::CoreObject*> HierarchyPanel::GetSelectedObjects() const
{
    std::vector<Leir::CoreObject*> objs;
    if (!m_TreeView) return objs;
    const auto sel = m_TreeView->GetSelectedItems();
    if (sel.empty()) return objs;
    // Preserve the TREE's selection order (last = most recently clicked) — NOT the
    // unordered_map iteration order, which is arbitrary and would make the editor's
    // "active object" (gizmo/inspector follow the last selected) pick a random one.
    // Reverse map item->obj once (O(N)) then walk the selection (O(K)).
    std::unordered_map<Leir::UITreeViewItem*, Leir::CoreObject*> rev;
    rev.reserve(m_ItemMap.size());
    for (const auto& kv : m_ItemMap) rev[kv.second] = kv.first;
    objs.reserve(sel.size());
    for (auto* item : sel) {
        auto it = rev.find(item);
        if (it != rev.end()) objs.push_back(it->second);
    }
    return objs;
}

void HierarchyPanel::NotifySelectionChanged()
{
    if (m_OnSelectionChanged)
        m_OnSelectionChanged(GetSelectedObjects());
}

Leir::CoreObject* HierarchyPanel::ObjectOfItem(Leir::UITreeViewItem* item) const
{
    for (const auto& kv : m_ItemMap)
        if (kv.second == item) return kv.first;
    return nullptr;
}

bool HierarchyPanel::IsDescendantOf(Leir::CoreObject* ancestor, Leir::CoreObject* node)
{
    for (auto* c : ancestor->GetChildren()) {
        if (c == node) return true;
        if (IsDescendantOf(c, node)) return true;
    }
    return false;
}

void HierarchyPanel::SetSelectedObjects(const std::vector<Leir::CoreObject*>& objs)
{
    if (!m_TreeView) return;
    std::vector<Leir::UITreeViewItem*> items;
    for (auto* obj : objs) {
        auto it = m_ItemMap.find(obj);
        if (it != m_ItemMap.end()) items.push_back(it->second);
    }
    // Fires the tree's selection callback -> NotifySelectionChanged -> the editor
    // (guarded there against feedback loops).
    m_TreeView->SetSelectedItems(items);
}

void HierarchyPanel::Refresh()
{
    if (!m_TreeView) return;
    const size_t sig = BuildSignature();
    if (sig != m_LastSignature) {
        Reconcile();
        m_LastSignature = sig;
    } else {
        // Name sync (cheap O(N) pass): renames don't change the structural
        // signature, so update item texts without rebuilding (no collapse).
        for (auto& kv : m_ItemMap) {
            auto* obj = kv.first;
            auto* item = kv.second;
            if (obj && item && item->GetText() != obj->GetName())
                item->SetText(obj->GetName());
        }
    }
}

HierarchyPanel::Family HierarchyPanel::FamilyOf(Leir::CoreObject* obj)
{
    if (dynamic_cast<Leir::Object3D*>(obj)) return Family::Object3D;
    if (dynamic_cast<Leir::Object2D*>(obj)) return Family::Object2D;
    return Family::UI; // UINode (future) / anything else
}

const char* HierarchyPanel::FamilyName(Family f)
{
    switch (f) {
        case Family::Object3D: return "[Object3D]";
        case Family::Object2D: return "[Object2D]";
        case Family::UI:       return "[UI]";
    }
    return "?";
}

// Structural signature (O(N), no allocation): object count + raw parent pointers
// (stable within a session — unique_ptr pointees never move). Detects creation/
// deletion/reparenting but NOT renames; renames are handled by the name-sync
// pass in Refresh so the tree never rebuilds/collapses on a rename.
size_t HierarchyPanel::BuildSignature() const
{
    auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
    if (!scene) return 0;
    const auto& objs = scene->GetObjects();
    size_t sig = 14695981039346656037ull; // FNV-1a offset basis
    sig = (sig ^ objs.size()) * 1099511628211ull;
    for (const auto& obj : objs) {
        Leir::CoreObject* p = obj->GetParent();
        sig = (sig ^ (uintptr_t)(p ? p : (Leir::CoreObject*)(uintptr_t)1)) * 1099511628211ull;
    }
    return sig;
}

void HierarchyPanel::EnsureIcons()
{
    if (!m_Backend || m_IconsLoaded) return;
    m_Icon3D = Leir::UITextureCache::Load(m_Backend, "assets/icons/object3d.png", m_ContentScale);
    m_Icon2D = Leir::UITextureCache::Load(m_Backend, "assets/icons/object2d.png", m_ContentScale);
    m_IconUI = Leir::UITextureCache::Load(m_Backend, "assets/icons/uielement.png", m_ContentScale);
    m_IconsLoaded = true;
}

void HierarchyPanel::ApplyIcons()
{
    for (const auto& kv : m_ItemMap) {
        const Family f = FamilyOf(kv.first);
        kv.second->SetIcon(f == Family::Object3D ? m_Icon3D : (f == Family::Object2D ? m_Icon2D : m_IconUI));
    }
}

// Incremental reconciliation (the professional way to avoid flicker): instead of
// destroying and recreating EVERY item on a structural change (which left the new
// items un-laid-out for a frame — the flicker — and lost selection/expansion),
// this walks the scene in DFS order (roots in m_Objects order, children in
// m_Children order) and:
//   * creates items for objects that don't have one yet (a single new cube adds
//     ONE item),
//   * re-appends items whose parent or sibling order changed (reparent/reorder),
//   * removes items whose scene object is gone.
// The walk order IS the tree order, so siblings stay in the scene's order. Only
// the CHANGED items are touched — O(changes), no full rebuild, no flicker, and
// selection/expansion survive.
void HierarchyPanel::Reconcile()
{
    EnsureIcons();
    if (!m_TreeView) return;
    auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
    if (!scene) return;

    // Stale-item deletion can free a hovered item -> clear the canvas hover first
    // (same crash guard as before).
    for (Leir::UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<Leir::UICanvas*>(e)) { c->ClearHoverAndFocus(); break; }
    }

    // Live scene objects (for removal detection).
    std::unordered_set<Leir::CoreObject*> live;
    live.reserve(scene->GetObjects().size());
    for (const auto& o : scene->GetObjects()) live.insert(o.get());

    // Remove items whose object is gone (depth-first so children die first).
    std::function<void(Leir::UITreeViewItem*)> destroyTree = [&](Leir::UITreeViewItem* item) {
        auto children = item->GetTreeChildren(); // copy (RemoveItem mutates)
        for (auto* c : children) destroyTree(c);
        m_TreeView->RemoveItem(item);
        delete item;
    };
    for (auto it = m_ItemMap.begin(); it != m_ItemMap.end();) {
        if (!live.count(it->first)) {
            destroyTree(it->second);
            it = m_ItemMap.erase(it);
        } else {
            ++it;
        }
    }

    // Walk the scene in DFS order, placing each object's item under its expected
    // parent (creating it if missing, re-appending if out of place).
    std::function<void(Leir::CoreObject*, Leir::UITreeViewItem*)> place =
        [&](Leir::CoreObject* obj, Leir::UITreeViewItem* parentItem) {
        auto it = m_ItemMap.find(obj);
        Leir::UITreeViewItem* item = nullptr;
        if (it != m_ItemMap.end()) {
            item = it->second;
        } else {
            item = new Leir::UITreeViewItem();
            m_ItemMap[obj] = item;
        }
        item->SetText(obj->GetName());
        item->SetShowIcon(true);
        const Family f = FamilyOf(obj);
        item->SetIcon(f == Family::Object3D ? m_Icon3D : (f == Family::Object2D ? m_Icon2D : m_IconUI));
        const bool wrongParent = item->GetTreeParent() != parentItem;
        const bool wrongOrder = parentItem
            ? (parentItem->GetTreeChildren().empty() || parentItem->GetTreeChildren().back() != item)
            : (m_TreeView->GetRoots().empty() || m_TreeView->GetRoots().back() != item);
        if (wrongParent || wrongOrder)
            m_TreeView->AddItem(item, parentItem); // detaches from the old parent, appends in walk order
        for (auto* c : obj->GetChildren()) place(c, item);
    };
    for (const auto& o : scene->GetObjects())
        if (!o->GetParent()) place(o.get(), nullptr);

    // Re-apply the active filter so newly added items respect it.
    if (!m_FilterText.empty())
        m_TreeView->SetFilter(m_FilterText);
}