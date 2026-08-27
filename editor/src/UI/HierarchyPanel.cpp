#include "HierarchyPanel.h"
#include <LeirEngine/UI/UITreeView.h>
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UITextureCache.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Objects/Object3D.h>
#include <LeirEngine/Objects/Object2D.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Scene/SceneManager.h>
#include <LeirEngine/Core/Log.h>
#include <cmath>

HierarchyPanel::HierarchyPanel()
{
    SetName("Hierarchy");
    SetColor({0.16f, 0.16f, 0.18f, 1.0f});

    m_TreeView = new Leir::UITreeView();
    m_TreeView->SetName("HierarchyTreeView");
    m_TreeView->SetMultipleSelectionEnabled(true);
    m_TreeView->SetEditable(true); // F2 rename (wired to SetName in Fase 4)
    m_TreeView->SetIconsEnabled(true);
    AddChild(m_TreeView);
}

HierarchyPanel::~HierarchyPanel() = default; // teardown via editor DeleteUiSubtree

void HierarchyPanel::SetFont(Leir::Font* font)
{
    if (m_TreeView) m_TreeView->SetFont(font);
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
    m_IconsLoaded = false;  // reload icons at the new DPI
    m_LastSignature.clear(); // force a rebuild so items pick up the new icons
}

Leir::Vector2 HierarchyPanel::GetMinSize() const
{
    return {160.0f, 120.0f};
}

void HierarchyPanel::OnLayoutComputed()
{
    Leir::UIPanel::OnLayoutComputed();
    if (!m_TreeView) return;

    // Pin the tree to the FULL panel rect every frame, with an ABSOLUTE offset
    // (anchor TopLeft). This mirrors how UITreeView itself positions its
    // internal viewport/items (UITreeView.cpp OnLayoutComputed) and is REQUIRED
    // because the UI core's ComputeFreeLayout accumulates the parent's position
    // into child offsets with `+=` each pass (it assumes Free-layout children
    // re-assign their offsets every frame — see UIElement.cpp). Using a Stretch
    // anchor here made the tree's offset grow by the panel position every frame
    // ("elements fly downward" as soon as the panel becomes visible). Column/
    // Row parents are safe (they re-assign with `=`); a Free parent must too.
    const auto& cr = GetComputedRect();
    m_TreeView->GetRect().anchor = {0.0f, 0.0f, 0.0f, 0.0f};
    m_TreeView->GetRect().offset = {
        std::round(cr.x), std::round(cr.y),
        std::round(cr.x + cr.z), std::round(cr.y + cr.w)
    };
    m_TreeView->ComputeLayout({cr.z, cr.w});
}

void HierarchyPanel::Refresh()
{
    if (!m_TreeView) return;
    const std::string sig = BuildSignature();
    if (sig != m_LastSignature) {
        RebuildAll();
        m_LastSignature = sig;
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

std::string HierarchyPanel::BuildSignature() const
{
    auto* scene = Leir::SceneManager::GetInstance().GetActiveScene();
    if (!scene) return "";
    std::string sig;
    sig.reserve(256);
    // Cheap change detection: names + parent wiring. Any structural change or
    // rename rebuilds; a static scene yields the same string every frame.
    for (const auto& obj : scene->GetObjects()) {
        sig += obj->GetName();
        sig += obj->GetParent() ? (">" + obj->GetParent()->GetName()) : "^";
        sig += ';';
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

    // 3 family roots, expanded by default (UITreeViewItem default).
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

    // One item per scene object (icon by family). Wiring happens next so a
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
}