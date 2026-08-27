#include "HierarchyPanel.h"
#include <LeirEngine/UI/UITreeView.h>
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/UITextureCache.h>
#include <LeirEngine/UI/UICanvas.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Core/Log.h>
#include <algorithm>
#include <cmath>

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
    // Placeholder (Paso 2.5): the intended behavior is to open a UIContextMenu
    // with Object2D / Object3D / UIElement that creates the object in the scene.
    // Programmed together with the ContextMenu (P1, TODO_UI_CONTEXT_MENU.md).
    m_AddButton->SetOnClick([this]() {
        Leir::XConsole::Debug("Hierarchy: '+' pressed — UIContextMenu pendiente (Object2D/Object3D/UIElement)");
    });
    m_Header->AddChild(m_AddButton);

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

    // Fase 0.2 Paso 4 — drag&drop (3-zone). Returns whether the scene accepted the
    // change: the tree only mutates its structure when true, so a rejected drop
    // never desyncs tree from scene. targetItem is the row, mode its zone:
    //   * Onto   -> nest into the target (SetParent, append — matches the tree).
    //   * Above  -> insert as a sibling BEFORE the target (post-removal index).
    //   * Below  -> insert as a sibling AFTER the target (post-removal index).
    //   * target = FAMILY ROOT ([Object3D]/[Object2D]/[UI]) -> make the dragged
    //     a root of that family; Above = front, Below/Onto = end (Unity style).
    //     The tree's own mutation is not trusted there -> we do NOT skip the next
    //     rebuild, so the tree re-syncs.
    // Warnings on every rejection (family root dragged / cross-family / cycle).
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
            if (it == rev.end()) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot drag the group header '{}'", di->GetText());
                return false;
            }
            objs.push_back(it->second);
        }
        // Resolve the target: a real object, or a family-root group header.
        Leir::CoreObject* targetObj = nullptr;
        bool familyRootTarget = false;
        Family targetFamily = Family::Object3D;
        {
            auto it = rev.find(targetItem);
            if (it != rev.end()) {
                targetObj = it->second;
            } else {
                auto fit = m_FamilyRootItems.find(targetItem);
                if (fit == m_FamilyRootItems.end()) {
                    Leir::XConsole::PrintWarning("Hierarchy: invalid drop target");
                    return false;
                }
                familyRootTarget = true;
                targetFamily = fit->second;
            }
        }
        const bool above = (mode == Leir::UITreeView::DropMode::Above);
        // Validate: no self/cycle, family match.
        for (auto* obj : objs) {
            if (obj == targetObj) return false;
            if (targetObj && FamilyOf(obj) != FamilyOf(targetObj)) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot reparent '{}' to '{}' — cross-family drop",
                    obj->GetName(), targetObj->GetName());
                return false;
            }
            if (familyRootTarget && FamilyOf(obj) != targetFamily) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot move '{}' into the {} group — cross-family drop",
                    obj->GetName(), FamilyName(targetFamily));
                return false;
            }
            if (targetObj && IsDescendantOf(obj, targetObj)) {
                Leir::XConsole::PrintWarning("Hierarchy: cannot reparent '{}' into its own descendant '{}'",
                    obj->GetName(), targetObj->GetName());
                return false;
            }
        }
        auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
        if (!scene) return false;

        if (familyRootTarget) {
            // Group-header target: the dragged becomes a root of that family.
            // Above = front (0), Below/Onto = end (large k).
            int k = above ? 0 : 0x3FFFFFFF;
            for (auto* obj : objs) {
                obj->SetParent(nullptr, false);
                scene->MoveObject(obj, RootInsertIndex(scene, targetFamily, k));
                if (k < 0x3FFFFFFF) ++k;
            }
            return true; // do NOT skip the rebuild (the tree re-syncs from the scene)
        }

        if (mode == Leir::UITreeView::DropMode::Onto) {
            for (auto* obj : objs) obj->SetParent(targetObj, false);
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
                // Target is a root object: reorder among the family's roots.
                const Family f = FamilyOf(targetObj);
                for (auto* obj : objs) {
                    obj->SetParent(nullptr, false);
                    // Recompute the target's index among the family roots (fresh,
                    // post-removal of obj) and insert before/after it.
                    int ti = -1, seen = 0;
                    for (const auto& o : scene->GetObjects()) {
                        if (o->GetParent()) continue;
                        if (FamilyOf(o.get()) != f) continue;
                        if (o.get() == targetObj) { ti = seen; break; }
                        ++seen;
                    }
                    if (ti < 0) return false;
                    const int k = above ? ti : ti + 1;
                    scene->MoveObject(obj, RootInsertIndex(scene, f, k));
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

HierarchyPanel::~HierarchyPanel() = default; // teardown via editor DeleteUiSubtree

void HierarchyPanel::SetFont(Leir::Font* font)
{
    if (m_TreeView) m_TreeView->SetFont(font);
    if (m_AddButton) m_AddButton->SetFont(font);
    if (m_FilterInput) m_FilterInput->SetFont(font);
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
    m_LastSignature = 0;     // force a rebuild so items pick up the new icons
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

// m_Objects index where a root of family f should go to land at position k among
// that family's roots (in m_Objects order). k >= the family's root count (or a
// large value) -> end of the list. O(N).
size_t HierarchyPanel::RootInsertIndex(Leir::Scene* scene, Family f, int k) const
{
    const auto& objs = scene->GetObjects();
    int seen = 0;
    for (size_t i = 0; i < objs.size(); ++i) {
        if (objs[i]->GetParent()) continue;
        if (FamilyOf(objs[i].get()) != f) continue;
        if (seen == k) return i;
        ++seen;
    }
    return objs.size();
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
        RebuildAll();
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

void HierarchyPanel::RebuildAll()
{
    EnsureIcons();
    if (!m_TreeView) return;

    // Clear the canvas hover BEFORE deleting the old items: the editor's OnUpdate
    // walks the hovered element's ancestors (UIElement::GetParent) and a stale
    // pointer to a just-freed item crashed (drag onto the hierarchy, 2026-08-27).
    for (Leir::UIElement* e = this; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<Leir::UICanvas*>(e)) { c->ClearHoverAndFocus(); break; }
    }

    // Tear down the previous tree. Items are caller-owned: ClearItems detaches
    // EVERY item (visible or not) from the internal viewport, then we free them
    // here. (Final teardown is the editor's DeleteUiSubtree, which recurses into
    // the tree's viewport; this path only runs for rebuilds during the panel's life.)
    m_TreeView->ClearItems();
    for (auto* it : m_OwnedItems) delete it;
    m_OwnedItems.clear();
    m_ItemMap.clear();
    m_FamilyRootItems.clear();

    auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
    if (!scene) return;

    // 3 family roots, expanded by default. NOT filter-excluded: they behave like
    // normal items while filtering — they match by their own text (searching "UI"
    // or "[" shows them) and the core's Godot rule still collapses empty families
    // when the filter matches nothing in them (e.g. "cube" hides [Object2D]/[UI]).
    // Tracked in m_FamilyRootItems so the drag callback can resolve them as
    // pseudo-group targets.
    Leir::UITreeViewItem* roots[3] = {nullptr, nullptr, nullptr};
    for (int f = 0; f < 3; ++f) {
        auto* r = new Leir::UITreeViewItem();
        r->SetText(FamilyName((Family)f));
        r->SetShowIcon(true);
        r->SetIcon(f == 0 ? m_Icon3D : (f == 1 ? m_Icon2D : m_IconUI));
        m_OwnedItems.push_back(r);
        m_FamilyRootItems[r] = (Family)f;
        m_TreeView->AddItem(r);
        roots[f] = r;
    }

    // Items for every scene object, built by DFS in m_Children order so the tree
    // mirrors the scene hierarchy faithfully (sibling order = CoreObject child
    // order, which InsertChildAt reorders on Below drops). Roots are the objects
    // with no parent, in m_Objects order.
    std::function<Leir::UITreeViewItem*(Leir::CoreObject*, Leir::UITreeViewItem*)> build =
        [&](Leir::CoreObject* obj, Leir::UITreeViewItem* parentItem) -> Leir::UITreeViewItem* {
        auto* item = new Leir::UITreeViewItem();
        item->SetText(obj->GetName());
        item->SetShowIcon(true);
        const Family f = FamilyOf(obj);
        item->SetIcon(f == Family::Object3D ? m_Icon3D : (f == Family::Object2D ? m_Icon2D : m_IconUI));
        m_OwnedItems.push_back(item);
        m_ItemMap[obj] = item;
        m_TreeView->AddItem(item, parentItem ? parentItem : roots[(int)f]);
        for (auto* c : obj->GetChildren()) build(c, item);
        return item;
    };
    for (const auto& obj : scene->GetObjects())
        if (!obj->GetParent()) build(obj.get(), roots[(int)FamilyOf(obj.get())]);

    // Re-apply the active filter to the freshly built items (new items default
    // to unfiltered; SetFilter is O(N) and idempotent for the empty string).
    if (!m_FilterText.empty())
        m_TreeView->SetFilter(m_FilterText);
}