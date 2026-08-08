#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UITextArea.h>
#include <LeirEngine/UI/Font.h>

class TextAreaDebugPanel : public Leir::UIPanel {
public:
    TextAreaDebugPanel();
    ~TextAreaDebugPanel() override;

    void SetFont(Leir::Font* font);
    void Refresh();

private:
    Leir::UILabel* m_TitleLabel = nullptr;
    Leir::UITextArea* m_TextArea = nullptr;
    Leir::UITextArea* m_ReadOnlyArea = nullptr;
    Leir::UILabel* m_StatusLabel = nullptr;
};
