#include "DebugTextPanel.h"

DebugTextPanel::DebugTextPanel()
{
    SetName("DebugTextPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(3.0f);

    auto addLabel = [&](const std::string& text) -> Leir::UILabel* {
        auto* lbl = new Leir::UILabel();
        lbl->SetText(text);
        lbl->SetFontSize(11);
        lbl->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
        lbl->SetSizePolicy(Leir::SizePolicy::Fixed);
        AddChild(lbl);
        return lbl;
    };

    m_TitleLabel = addLabel("-- Text Input Debug --");

    // Single-line text input
    m_SingleInput = new Leir::UITextInput();
    m_SingleInput->SetName("DebugSingleInput");
    m_SingleInput->SetPlaceholder("single line...");
    m_SingleInput->SetSizePolicy(Leir::SizePolicy::Fill);
    AddChild(m_SingleInput);

    m_SingleStatus = new Leir::UILabel();
    m_SingleStatus->SetText("single: cursor=0 sel=-1");
    m_SingleStatus->SetFontSize(10);
    m_SingleStatus->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
    m_SingleStatus->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_SingleStatus);

    // Float input
    m_FloatInput = new Leir::UIFloatInput();
    m_FloatInput->SetName("DebugFloatInput");
    m_FloatInput->SetSizePolicy(Leir::SizePolicy::Fill);
    AddChild(m_FloatInput);

    m_FloatStatus = new Leir::UILabel();
    m_FloatStatus->SetText("float: 0.0");
    m_FloatStatus->SetFontSize(10);
    m_FloatStatus->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
    m_FloatStatus->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_FloatStatus);
}

DebugTextPanel::~DebugTextPanel() = default;

void DebugTextPanel::SetFont(Leir::Font* font)
{
    for (auto* child : GetChildren()) {
        if (auto* input = dynamic_cast<Leir::UITextInput*>(child))
            input->SetFont(font);
        if (auto* label = dynamic_cast<Leir::UILabel*>(child))
            label->SetFont(font);
    }
}

void DebugTextPanel::Refresh()
{
    // Single-line status
    {
        const auto& text = m_SingleInput->GetText();
        m_SingleStatus->SetText("single: \"" + text + "\" cursor=" + std::to_string(m_SingleInput->GetCursorPos())
            + " sel=" + std::to_string(m_SingleInput->GetSelBegin()));
    }

    // Float status
    {
        m_FloatStatus->SetText("float: " + std::to_string(m_FloatInput->GetValue()));
    }
}
