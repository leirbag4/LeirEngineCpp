#include "TreeViewDebugPanel.h"
#include <LeirEngine/UI/UITreeViewItem.h>
#include <LeirEngine/UI/UILabel.h>

TreeViewDebugPanel::TreeViewDebugPanel()
{
    SetName("TreeViewDebugPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(4.0f);

    m_TitleLabel = new Leir::UILabel();
    m_TitleLabel->SetName("TreeTitle");
    m_TitleLabel->SetText("TreeView (virtualized, F2 rename, drag, multi-select)");
    m_TitleLabel->SetFontSize(11);
    m_TitleLabel->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
    m_TitleLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_TitleLabel);

    m_TreeView = new Leir::UITreeView();
    m_TreeView->SetName("DebugTreeView");
    m_TreeView->SetMultipleSelectionEnabled(true);
    m_TreeView->SetEditable(true);
    AddChild(m_TreeView);

    m_StatusLabel = new Leir::UILabel();
    m_StatusLabel->SetName("TreeStatus");
    m_StatusLabel->SetText("status");
    m_StatusLabel->SetFontSize(10);
    m_StatusLabel->SetColor({0.5f, 0.5f, 0.5f, 1.0f});
    m_StatusLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_StatusLabel);

    PopulateSample();

    m_TreeView->SetOnSelectedItemChanged([this](Leir::UITreeViewItem* item) {
        if (item) m_StatusLabel->SetText("selected: " + item->GetText());
    });
    m_TreeView->SetOnItemDoubleClicked([this](Leir::UITreeViewItem* item) {
        m_StatusLabel->SetText("double-click: " + item->GetText());
    });
    m_TreeView->SetOnItemRenamed([this](Leir::UITreeViewItem* item, const std::string& oldT, const std::string& newT) {
        m_StatusLabel->SetText("renamed: " + oldT + " -> " + newT);
    });
}

TreeViewDebugPanel::~TreeViewDebugPanel() = default;

void TreeViewDebugPanel::SetFont(Leir::Font* font)
{
    for (auto* child : GetChildren()) {
        if (auto* lbl = dynamic_cast<Leir::UILabel*>(child)) lbl->SetFont(font);
    }
    if (m_TreeView) m_TreeView->SetFont(font);
}

void TreeViewDebugPanel::Refresh()
{
    if (!m_TreeView || !m_StatusLabel) return;
    auto sel = m_TreeView->GetSelectedItems();
    std::string s = "count=" + std::to_string(m_TreeView->GetItemCount()) + " sel=" + std::to_string(sel.size());
    if (!sel.empty()) s += " first=" + sel[0]->GetText();
    // Keep status if not overwritten by events? Just update if no recent event
    // For now always update
    // m_StatusLabel->SetText(s);
}

void TreeViewDebugPanel::PopulateSample()
{
    // Small test mode for scrollbar-less validation (pedido usuario 2026-08-23).
    // Cambiá a false para volver al stress test de 2000 items.
    constexpr bool kSmallTest = true;
    if (kSmallTest) {
        for (int i = 0; i < 2; ++i) {
            auto* root = new Leir::UITreeViewItem();
            root->SetText("Root " + std::to_string(i));
            m_TreeView->AddItem(root, nullptr);
            for (int j = 0; j < 2; ++j) {
                auto* child = new Leir::UITreeViewItem();
                child->SetText("Child " + std::to_string(i) + "." + std::to_string(j));
                m_TreeView->AddItem(child, root);
                // No grand para mantener total < 20 (sin scrollbar)
            }
        }
        auto* smallRoot = new Leir::UITreeViewItem();
        smallRoot->SetText("Small List (10)");
        m_TreeView->AddItem(smallRoot, nullptr);
        for (int i = 0; i < 10; ++i) {
            auto* it = new Leir::UITreeViewItem();
            it->SetText("Item " + std::to_string(i));
            m_TreeView->AddItem(it, smallRoot);
        }
        smallRoot->SetExpanded(false);
        return;
    }
    // Create a sample hierarchy with varied depths and a large flat list for perf test
    for (int i = 0; i < 5; ++i) {
        auto* root = new Leir::UITreeViewItem();
        root->SetText("Root " + std::to_string(i));
        m_TreeView->AddItem(root, nullptr);
        for (int j = 0; j < 4; ++j) {
            auto* child = new Leir::UITreeViewItem();
            child->SetText("Child " + std::to_string(i) + "." + std::to_string(j));
            m_TreeView->AddItem(child, root);
            for (int k = 0; k < 3; ++k) {
                auto* grand = new Leir::UITreeViewItem();
                grand->SetText("Grand " + std::to_string(i) + "." + std::to_string(j) + "." + std::to_string(k));
                m_TreeView->AddItem(grand, child);
            }
        }
        if (i == 1) root->SetItemEnabled(false); // test grayed
    }
    // Large flat list under a separate root for virtualization stress (2000 items)
    auto* bigRoot = new Leir::UITreeViewItem();
    bigRoot->SetText("Big List (2000)");
    m_TreeView->AddItem(bigRoot, nullptr);
    for (int i = 0; i < 2000; ++i) {
        auto* it = new Leir::UITreeViewItem();
        it->SetText("Item " + std::to_string(i) + " - some longer text to test horizontal scroll width calculation");
        m_TreeView->AddItem(it, bigRoot);
    }
    bigRoot->SetExpanded(false); // collapsed by default
}

Leir::Vector2 TreeViewDebugPanel::GetMinSize() const
{
    return {300.0f, 400.0f};
}

void TreeViewDebugPanel::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}
