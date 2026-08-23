#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UITreeView.h>
#include <LeirEngine/UI/Font.h>

class TreeViewDebugPanel : public Leir::UIPanel {
public:
    TreeViewDebugPanel();
    ~TreeViewDebugPanel() override;

    void SetFont(Leir::Font* font);
    void Refresh();
    Leir::Vector2 GetMinSize() const override;
protected:
    void OnLayoutComputed() override;
private:
    void PopulateSample();

    Leir::UILabel* m_TitleLabel = nullptr;
    Leir::UITreeView* m_TreeView = nullptr;
    Leir::UILabel* m_StatusLabel = nullptr;
    int m_NextId = 0;
};
