#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/UIFloatInput.h>
#include <LeirEngine/UI/Font.h>

class DebugTextPanel : public Leir::UIPanel {
public:
    DebugTextPanel();
    ~DebugTextPanel() override;

    void SetFont(Leir::Font* font);
    void Refresh();

private:
    Leir::UILabel* m_TitleLabel = nullptr;
    Leir::UITextInput* m_SingleInput = nullptr;
    Leir::UIFloatInput* m_FloatInput = nullptr;
    Leir::UILabel* m_SingleStatus = nullptr;
    Leir::UILabel* m_FloatStatus = nullptr;
};
