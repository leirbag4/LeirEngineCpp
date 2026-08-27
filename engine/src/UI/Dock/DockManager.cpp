#include "LeirEngine/UI/Dock/DockManager.h"
#include "LeirEngine/UI/Dock/DockNode.h"
#include "LeirEngine/UI/Dock/DockSplitNode.h"
#include "LeirEngine/UI/Dock/DockPane.h"
#include "LeirEngine/UI/Dock/DockTabBar.h"
#include "LeirEngine/UI/Dock/DockDropOverlay.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/Font.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <exception>
#include "LeirEngine/Core/Log.h"

namespace Leir {

namespace {

Vector4 ZoneRectFor(DockPane* pane, DockDropZone zone)
{
    const auto& cr = pane->GetComputedRect();
    const float w = cr.z;
    const float h = cr.w;
    switch (zone) {
        case DockDropZone::Left:   return {cr.x, cr.y, w * 0.5f, h};
        case DockDropZone::Right:  return {cr.x + w * 0.5f, cr.y, w * 0.5f, h};
        case DockDropZone::Top:    return {cr.x, cr.y, w, h * 0.5f};
        case DockDropZone::Bottom: return {cr.x, cr.y + h * 0.5f, w, h * 0.5f};
        case DockDropZone::Center:
        default:                   return {cr.x, cr.y, w, h};
    }
}

DockPane* FindPaneRecursive(DockNode* node, DockPanel* panel)
{
    if (!node)
        return nullptr;
    if (auto* pane = dynamic_cast<DockPane*>(node))
        return pane->Contains(panel) ? pane : nullptr;

    auto* split = dynamic_cast<DockSplitNode*>(node);
    for (size_t i = 0; i < split->GetNodeCount(); ++i) {
        if (auto* p = FindPaneRecursive(split->GetNode(i), panel))
            return p;
    }
    return nullptr;
}

DockPane* FindFirstPaneRecursive(DockNode* node)
{
    if (!node)
        return nullptr;
    if (auto* pane = dynamic_cast<DockPane*>(node))
        return pane;
    auto* split = dynamic_cast<DockSplitNode*>(node);
    for (size_t i = 0; i < split->GetNodeCount(); ++i) {
        if (auto* p = FindFirstPaneRecursive(split->GetNode(i)))
            return p;
    }
    return nullptr;
}

// True when `pos` falls on the pane's tab bar strip (vs. its content area).
bool PosOverTabBar(const DockPane* pane, const Vector2& pos)
{
    const DockTabBar* bar = pane->GetTabBar();
    if (!bar)
        return false;
    const auto& cr = bar->GetComputedRect();
    return pos.y >= cr.y && pos.y <= cr.y + cr.w;
}

nlohmann::json SerializeNodeJson(const DockNode* node)
{
    if (auto* split = dynamic_cast<const DockSplitNode*>(node)) {
        nlohmann::json j;
        j["type"] = "split";
        j["orientation"] = (split->GetOrientation() == DockOrientation::Horizontal) ? "H" : "V";
        j["ratios"] = split->GetRatios();
        j["children"] = nlohmann::json::array();
        for (size_t i = 0; i < split->GetNodeCount(); ++i)
            j["children"].push_back(SerializeNodeJson(split->GetNode(i)));
        return j;
    }

    auto* pane = dynamic_cast<const DockPane*>(node);
    nlohmann::json j;
    j["type"] = "pane";
    j["tabs"] = nlohmann::json::array();
    for (size_t i = 0; i < pane->GetTabCount(); ++i)
        j["tabs"].push_back(pane->GetTab(i)->id);
    j["active"] = pane->GetActiveIndex();
    return j;
}

DockNode* DeserializeNodeJson(DockManager* mgr, const nlohmann::json& j)
{
    if (j.is_null())
        return nullptr;

    const std::string type = j.value("type", "pane");
    if (type == "split") {
        const DockOrientation ori = (j.value("orientation", "H") == "H")
            ? DockOrientation::Horizontal : DockOrientation::Vertical;
        auto* split = new DockSplitNode(ori);
        std::vector<float> ratios = j.value("ratios", std::vector<float>{});
        const auto& children = j.value("children", nlohmann::json::array());
        for (size_t i = 0; i < children.size(); ++i) {
            DockNode* c = DeserializeNodeJson(mgr, children[i]);
            if (!c)
                continue;
            float ratio = (i < ratios.size()) ? ratios[i] : 1.0f;
            split->AddNode(c, ratio);
        }
        return split;
    }

    auto* pane = new DockPane(mgr);
    pane->SetName("DockPane");
    const auto tabs = j.value("tabs", std::vector<std::string>{});
    for (const auto& id : tabs) {
        if (auto* p = mgr->FindPanelById(id))
            if (p->active)
                pane->AddTab(p, false);
    }
    const int active = j.value("active", 0);
    if (pane->GetTabCount() > 0)
        pane->SetActiveTab(std::clamp(active, 0, (int)pane->GetTabCount() - 1));
    return pane;
}

} // namespace

DockManager::DockManager()
{
    SetName("DockManager");
    SetColor({0.12f, 0.12f, 0.14f, 1.0f});

    m_Overlay = new DockDropOverlay();
    m_Overlay->SetName("DockDropOverlay");
    AddChild(m_Overlay);
}

DockManager::~DockManager()
{
    if (m_Root) {
        RemoveChild(m_Root);
        delete m_Root;
    }
    if (m_Overlay) {
        RemoveChild(m_Overlay);
        delete m_Overlay;
    }
}

DockPanel* DockManager::RegisterPanel(const std::string& id, const std::string& title,
                                      UIElement* content, bool closeable)
{
    auto panel = std::make_unique<DockPanel>();
    panel->id = id;
    panel->title = title;
    panel->content = content;
    panel->closeable = closeable;
    DockPanel* raw = panel.get();
    m_Panels.push_back(std::move(panel));
    return raw;
}

DockPanel* DockManager::FindPanelById(const std::string& id) const
{
    for (auto& p : m_Panels)
        if (p->id == id)
            return p.get();
    return nullptr;
}

void DockManager::BuildDefaultLayout()
{
    DestroyTree();

    auto makePane = [this](const char* name) -> DockPane* {
        auto* pane = new DockPane(this);
        pane->SetName(name);
        return pane;
    };

    DockPane* hierPane = makePane("Pane:Hierarchy");
    DockPane* viewportPane = makePane("Pane:Viewport");
    DockPane* debugPane = makePane("Pane:Debug");
    DockPane* inspPane = makePane("Pane:Inspector");

    if (auto* p = FindPanelById("Hierarchy"))
        hierPane->AddTab(p);
    if (auto* p = FindPanelById("Viewport"))
        viewportPane->AddTab(p);
    if (auto* p = FindPanelById("Inspector"))
        inspPane->AddTab(p);

    static const char* kDebugIds[] = {
        "TestPanel", "CameraTestPanel", "DebugTextPanel", "TextAreaDebugPanel",
        "TextAreaWrapPanel",
        "GridPanel", "ConsolePanel", "DebugPanel", "GizmoLogPanel",
    };
    for (const char* id : kDebugIds)
        if (auto* p = FindPanelById(id))
            if (p->active)
                debugPane->AddTab(p);

    auto* vSplit = new DockSplitNode(DockOrientation::Vertical);
    vSplit->AddNode(viewportPane, 0.8f);
    vSplit->AddNode(debugPane, 0.2f);

    auto* root = new DockSplitNode(DockOrientation::Horizontal);
    root->AddNode(hierPane, 0.17f);
    root->AddNode(vSplit, 0.66f);
    root->AddNode(inspPane, 0.17f);

    m_Root = root;
    root->GetRect().anchor = AnchorSet::Stretch();
    root->GetRect().offset = {};
    AddChild(root);
    PushOverlayToTop();
    CleanupEmptyPanes();
}

std::string DockManager::SerializeLayout() const
{
    nlohmann::json j = m_Root ? SerializeNodeJson(m_Root) : nlohmann::json(nullptr);
    return j.dump();
}

bool DockManager::LoadLayout(const std::string& json)
{
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        DestroyTree();
        m_Root = DeserializeNodeJson(this, j);
        if (m_Root) {
            m_Root->GetRect().anchor = AnchorSet::Stretch();
            m_Root->GetRect().offset = {};
            AddChild(m_Root);
        }

        CleanupEmptyPanes();
        PlaceMissingPanels();

        if (!m_Root) {
            BuildDefaultLayout();
        } else {
            PushOverlayToTop();
        }
        return true;
    } catch (const std::exception& e) {
        XConsole::PrintError("DockManager: failed to parse dock layout: {}", e.what());
        BuildDefaultLayout();
        return false;
    }
}

DockPane* DockManager::FindPaneByPanel(DockPanel* panel) const
{
    return FindPaneRecursive(m_Root, panel);
}

DockPane* DockManager::FindPaneAt(DockNode* node, const Vector2& pos) const
{
    if (!node)
        return nullptr;
    if (auto* pane = dynamic_cast<DockPane*>(node)) {
        const auto& cr = pane->GetComputedRect();
        if (pos.x >= cr.x && pos.x <= cr.x + cr.z &&
            pos.y >= cr.y && pos.y <= cr.y + cr.w)
            return pane;
        return nullptr;
    }
    auto* split = dynamic_cast<DockSplitNode*>(node);
    for (size_t i = split->GetNodeCount(); i-- > 0;) {
        if (auto* found = FindPaneAt(split->GetNode(i), pos))
            return found;
    }
    return nullptr;
}

void DockManager::ClosePanel(DockPanel* panel)
{
    if (!panel)
        return;
    RemovePanelFromPane(panel);
    panel->active = false;
    CleanupEmptyPanes();
    ClearDanglingPointers();
    NotifyLayoutChanged();
}

void DockManager::RequestClosePanel(DockPanel* panel)
{
    m_PendingClosePanel = panel;
}

void DockManager::Process()
{
    if (m_PendingClosePanel) {
        DockPanel* p = m_PendingClosePanel;
        m_PendingClosePanel = nullptr;
        ClosePanel(p);
    }
}

void DockManager::ClearDanglingPointers()
{
    UIElement* e = this;
    while (e) {
        if (auto* canvas = dynamic_cast<UICanvas*>(e)) {
            canvas->ClearHoverAndFocus();
            break;
        }
        e = e->GetParent();
    }
}

void DockManager::MergeIntoPane(DockPanel* panel, DockPane* target)
{
    if (!panel || !target)
        return;
    if (target->Contains(panel)) {
        target->SetActivePanel(panel);
        NotifyLayoutChanged();
        return;
    }
    RemovePanelFromPane(panel);
    target->AddTab(panel);
    CleanupEmptyPanes();
    ClearDanglingPointers();
    NotifyLayoutChanged();
}

void DockManager::SplitPane(DockPanel* panel, DockPane* target, DockDropZone zone)
{
    if (!panel || !target)
        return;

    // Dropping a panel onto its own pane with no other tabs: just focus it
    // (pure self-drop, same as a center drop). When the pane shares tabs with
    // other panels, edge drops still split the pane so the dragged tab moves
    // into its own new zone (RemovePanelFromPane strips it from the shared
    // pane, leaving the sibling tabs behind).
    if (target->Contains(panel) && target->GetTabCount() <= 1) {
        target->SetActivePanel(panel);
        NotifyLayoutChanged();
        return;
    }

    const DockOrientation ori = (zone == DockDropZone::Left || zone == DockDropZone::Right)
        ? DockOrientation::Horizontal : DockOrientation::Vertical;
    const bool newFirst = (zone == DockDropZone::Left || zone == DockDropZone::Top);

    // Capture target's ORIGINAL parent before the new split re-parents it.
    // AddNode(target, ...) moves target under `split` (UIElement::AddChild
    // auto-reparents), so computing the parent later would resolve to the new
    // split itself and create a self-referencing tree (panel disappears).
    DockSplitNode* parentSplit = dynamic_cast<DockSplitNode*>(target->GetParent());
    const float parentRatio = parentSplit ? parentSplit->GetRatioForChild(target) : 0.5f;

    DockPane* newPane = new DockPane(this);
    newPane->SetName("Pane:" + panel->id);

    RemovePanelFromPane(panel);
    newPane->AddTab(panel);

    auto* split = new DockSplitNode(ori);
    if (newFirst) {
        split->AddNode(newPane, 0.5f);
        split->AddNode(target, 0.5f);
    } else {
        split->AddNode(target, 0.5f);
        split->AddNode(newPane, 0.5f);
    }

    if (parentSplit) {
        // Swap target for the new split inside the ORIGINAL parent. target is
        // no longer a UI child of parentSplit (AddNode moved it), but it is
        // still in parentSplit's m_NodeChildren, so ReplaceChild swaps the
        // node entry and adopts `split` as a UI child. The ratio is preserved
        // so the surrounding layout does not shift.
        parentSplit->ReplaceChild(target, split, parentRatio);
    } else {
        // target was the root; the manager adopts the new split directly.
        RemoveChild(target);
        m_Root = split;
        split->GetRect().anchor = AnchorSet::Stretch();
        split->GetRect().offset = {};
        AddChild(split);
        PushOverlayToTop();
    }

    CleanupEmptyPanes();
    ClearDanglingPointers();
    NotifyLayoutChanged();
}

void DockManager::BeginTabDrag(DockTab* tab, const Vector2& pos)
{
    m_DragTab = tab;
    m_DragPanel = tab ? tab->GetPanel() : nullptr;
    m_DragStartPos = pos;
    m_Dragging = false;

    if (tab) {
        const auto& cr = tab->GetComputedRect();
        m_GrabOffset = {pos.x - cr.x, pos.y - cr.y};
    }

    UIElement* e = this;
    while (e) {
        if (auto* canvas = dynamic_cast<UICanvas*>(e)) {
            XConsole::Trace("[DockManager] capturing pointer for tab drag");
            canvas->CapturePointer(this);
            break;
        }
        e = e->GetParent();
    }
}

void DockManager::OnPointerMove(const Vector2& pos)
{
    if (!m_DragPanel)
        return;

    if (!m_Dragging) {
        const float dx = pos.x - m_DragStartPos.x;
        const float dy = pos.y - m_DragStartPos.y;
        if (dx * dx + dy * dy <= 4.0f * 4.0f)
            return;
        m_Dragging = true;
        if (m_Overlay)
            m_Overlay->SetVisible(true);
    }

    m_HoverPane = FindPaneAt(m_Root, pos);
    m_HoverZone = m_HoverPane ? ComputeZone(m_HoverPane, pos) : DockDropZone::None;

    // Reorder gesture: dragging a tab over its own pane's tab bar (with
    // sibling tabs present) reorders instead of split/merge, so show only the
    // ghost — no misleading zone highlight.
    const bool reorderGesture = m_HoverPane && m_HoverPane->Contains(m_DragPanel)
        && m_HoverPane->GetTabCount() > 1 && PosOverTabBar(m_HoverPane, pos);

    if (m_DragTab) {
        const auto& cr = m_DragTab->GetComputedRect();
        if (m_Overlay)
            m_Overlay->SetGhostRect({pos.x - m_GrabOffset.x, pos.y - m_GrabOffset.y, cr.z, cr.w});
    }

    if (reorderGesture) {
        if (m_Overlay)
            m_Overlay->HideZone();
    } else if (m_HoverPane && m_HoverZone != DockDropZone::None && m_Overlay) {
        m_Overlay->SetZoneRect(ZoneRectFor(m_HoverPane, m_HoverZone), {0.3f, 0.6f, 1.0f, 0.30f});
    } else if (m_Overlay) {
        m_Overlay->HideZone();
    }
}

bool DockManager::OnPointerUp(const Vector2& pos)
{
    if (!m_DragPanel)
        return false;

    if (m_Dragging && m_HoverPane && m_HoverZone != DockDropZone::None) {
        DockPane* pane = m_HoverPane;
        if (pane->Contains(m_DragPanel)) {
            // Dropping on the pane that already hosts the dragged tab.
            if (pane->GetTabCount() > 1 && PosOverTabBar(pane, pos)) {
                // Tab-bar reorder (same pane).
                if (pane->ReorderTabTo(m_DragPanel, pos)) {
                    ClearDanglingPointers();
                    NotifyLayoutChanged();
                }
            } else if (m_HoverZone == DockDropZone::Center) {
                pane->SetActivePanel(m_DragPanel);
                NotifyLayoutChanged();
            } else {
                // Edge drop on the shared pane: split it (Feature 1).
                SplitPane(m_DragPanel, pane, m_HoverZone);
            }
        } else if (m_HoverZone == DockDropZone::Center) {
            MergeIntoPane(m_DragPanel, pane);
        } else {
            SplitPane(m_DragPanel, pane, m_HoverZone);
        }
    }

    if (m_Overlay)
        m_Overlay->SetVisible(false);

    m_DragTab = nullptr;
    m_DragPanel = nullptr;
    m_HoverPane = nullptr;
    m_HoverZone = DockDropZone::None;
    m_Dragging = false;
    return true;
}

void DockManager::NotifyLayoutChanged()
{
    if (m_OnLayoutChanged)
        m_OnLayoutChanged();
}

void DockManager::ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);
    m_ComputedRect.x += parentOffset.x;
    m_ComputedRect.y += parentOffset.y;

    // Lay out the dock tree with ABSOLUTE rects (set, not +=). (The base Free
    // layout used to permanently += this panel's position into every child
    // offset, accumulating when the manager was not at (0,0) — the dock below
    // the toolbar at y=30 slid down 30px/frame; that core bug is fixed in
    // UIElement.cpp, and this override passes parentOffset={} since the child
    // offsets below are already absolute.)
    const float innerX = m_ComputedRect.x + m_Padding[0];
    const float innerY = m_ComputedRect.y + m_Padding[1];
    const float innerW = m_ComputedRect.z - m_Padding[0] - m_Padding[2];
    const float innerH = m_ComputedRect.w - m_Padding[1] - m_Padding[3];

    for (auto* child : GetChildren()) {
        if (!child->IsActive())
            continue;
        // The root dock node and the drop overlay both fill the dock area.
        child->GetRect().anchor = AnchorSet::TopLeft();
        child->GetRect().offset = {innerX, innerY, innerX + innerW, innerY + innerH};
        child->ComputeLayout({innerW, innerH});
    }
    OnLayoutComputed();
}

void DockManager::DestroyTree()
{
    if (m_Root) {
        RemoveChild(m_Root);
        delete m_Root;
        m_Root = nullptr;
    }
}

void DockManager::ReplaceNode(DockNode* oldNode, DockNode* newNode, DockSplitNode* parentSplit)
{
    if (parentSplit) {
        parentSplit->ReplaceChild(oldNode, newNode, parentSplit->GetRatioForChild(oldNode));
        return;
    }

    // oldNode is the root
    RemoveChild(oldNode);
    m_Root = newNode;
    newNode->GetRect().anchor = AnchorSet::Stretch();
    newNode->GetRect().offset = {};
    AddChild(newNode);
    PushOverlayToTop();
}

void DockManager::RemovePanelFromPane(DockPanel* panel)
{
    if (auto* pane = FindPaneByPanel(panel))
        pane->RemoveTab(panel);
}

void DockManager::CleanupEmptyPanes()
{
    if (!m_Root)
        return;
    if (CleanupNode(m_Root)) {
        RemoveChild(m_Root);
        delete m_Root;
        m_Root = nullptr;
    }
    if (m_Root)
        PushOverlayToTop();
}

bool DockManager::CleanupNode(DockNode* node)
{
    if (auto* pane = dynamic_cast<DockPane*>(node))
        return pane->GetTabCount() == 0;

    auto* split = dynamic_cast<DockSplitNode*>(node);

    std::vector<DockNode*> dead;
    for (size_t i = 0; i < split->GetNodeCount(); ++i) {
        DockNode* c = split->GetNode(i);
        if (CleanupNode(c))
            dead.push_back(c);
    }
    for (auto* c : dead) {
        split->RemoveNode(c);
        delete c;
    }

    if (split->GetNodeCount() == 1) {
        DockNode* only = split->GetNode(0);
        split->RemoveNode(only);
        ReplaceNode(split, only, dynamic_cast<DockSplitNode*>(split->GetParent()));
        delete split;
        return false;
    }
    if (split->GetNodeCount() == 0)
        return true;
    return false;
}

void DockManager::PlaceMissingPanels()
{
    DockPane* first = FindFirstPaneRecursive(m_Root);
    if (!first)
        return;
    for (auto& p : m_Panels) {
        if (!p->active)
            continue;
        if (FindPaneByPanel(p.get()))
            continue;
        first->AddTab(p.get());
    }
}

void DockManager::PushOverlayToTop()
{
    if (!m_Overlay)
        return;
    for (auto* c : GetChildren()) {
        if (c == m_Overlay) {
            RemoveChild(m_Overlay);
            break;
        }
    }
    AddChild(m_Overlay);
}

DockDropZone DockManager::ComputeZone(DockPane* pane, const Vector2& pos) const
{
    const auto& cr = pane->GetComputedRect();
    if (cr.z <= 0.0f || cr.w <= 0.0f)
        return DockDropZone::None;

    const float fx = (pos.x - cr.x) / cr.z;
    const float fy = (pos.y - cr.y) / cr.w;

    if (fx >= 0.25f && fx <= 0.75f && fy >= 0.25f && fy <= 0.75f)
        return DockDropZone::Center;

    const float dx = fx - 0.5f;
    const float dy = fy - 0.5f;
    if (std::fabs(dx) > std::fabs(dy))
        return dx < 0.0f ? DockDropZone::Left : DockDropZone::Right;
    return dy < 0.0f ? DockDropZone::Top : DockDropZone::Bottom;
}

} // namespace Leir
