#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UITextArea.h>
#include <LeirEngine/UI/Font.h>

class TextAreaWrapPanel : public Leir::UIPanel {
public:
    TextAreaWrapPanel();
    ~TextAreaWrapPanel() override;

    void SetFont(Leir::Font* font);
    void Refresh();

private:
    void UpdateWrapButton();

    Leir::UILabel* m_TitleLabel = nullptr;
    Leir::UILabel* m_StatusLabel = nullptr;
    Leir::UIButton* m_WrapButton = nullptr;
    Leir::UITextArea* m_TextArea = nullptr;
};