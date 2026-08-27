#include "HierarchyPanel.h"
#include <LeirEngine/UI/UITreeView.h>
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/UITextureCache.h>
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
    for (const auto& kv : m_ItemMap)
        if (std::find(sel.begin(), sel.end(), kv.second) != sel.end())
            objs.push_back(kv.first);
    return objs;
}

void HierarchyPanel::NotifySelectionChanged()
{
    if (m_OnSelectionChanged)
        m_OnSelectionChanged(GetSelectedObjects());
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

    // Tear down the previous tree. Items are caller-owned: ClearItems detaches
    // them from the internal viewport, then we free them here. (Final teardown
    // is the editor's DeleteUiSubtree, which recurses into the tree's viewport;
    // this path only runs for rebuilds during the panel's life.)
    m_TreeView->ClearItems();
    for (auto* it : m_OwnedItems) delete it;
    m_OwnedItems.clear();
    m_ItemMap.clear();

    auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
    if (!scene) return;

    // 3 family roots, expanded by default. NOT filter-excluded: they behave like
    // normal items while filtering — they match by their own text (searching "UI"
    // or "[" shows them) and the core's Godot rule still collapses empty families
    // when the filter matches nothing in them (e.g. "cube" hides [Object2D]/[UI]).
    Leir::UITreeViewItem* roots[3] = {nullptr, nullptr, nullptr};
    for (int f = 0; f < 3; ++f) {
        auto* r = new Leir::UITreeViewItem();
        r->SetText(FamilyName((Family)f));
        r->SetShowIcon(true);
        r->SetIcon(f == 0 ? m_Icon3D : (f == 1 ? m_Icon2D : m_IconUI));
        m_OwnedItems.push_back(r);
        m_TreeView->AddItem(r);
        roots[f] = r;
    }

    // Items for every scene object (icon by family). Wiring happens next so a
    // child that appears before its parent in the list still lands correctly.
    for (const auto& obj : scene->GetObjects()) {
        auto* item = new Leir::UITreeViewItem();
        item->SetText(obj->GetName());
        item->SetShowIcon(true);
        const Family f = FamilyOf(obj.get());
        item->SetIcon(f == Family::Object3D ? m_Icon3D : (f == Family::Object2D ? m_Icon2D : m_IconUI));
        m_OwnedItems.push_back(item);
        m_ItemMap[obj.get()] = item;
    }

    // Wire parent -> child; orphans go under their family root.
    for (const auto& obj : scene->GetObjects()) {
        auto* it = m_ItemMap[obj.get()];
        auto* parent = obj->GetParent();
        if (parent) {
            auto pit = m_ItemMap.find(parent);
            m_TreeView->AddItem(it, pit != m_ItemMap.end() ? pit->second : roots[(int)FamilyOf(obj.get())]);
        } else {
            m_TreeView->AddItem(it, roots[(int)FamilyOf(obj.get())]);
        }
    }

    // Re-apply the active filter to the freshly built items (new items default
    // to unfiltered; SetFilter is O(N) and idempotent for the empty string).
    if (!m_FilterText.empty())
        m_TreeView->SetFilter(m_FilterText);
}