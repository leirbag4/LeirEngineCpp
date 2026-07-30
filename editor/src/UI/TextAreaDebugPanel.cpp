#include "TextAreaDebugPanel.h"

TextAreaDebugPanel::TextAreaDebugPanel()
{
    SetName("DebugTextAreaPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(3.0f);

    m_TitleLabel = new Leir::UILabel();
    m_TitleLabel->SetText("-- TextArea Debug --");
    m_TitleLabel->SetFontSize(11);
    m_TitleLabel->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
    m_TitleLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_TitleLabel);

    m_TextArea = new Leir::UITextArea();
    m_TextArea->SetName("DebugTextAreaPanelArea");
    m_TextArea->SetPlaceholder("multiline debug...");
    m_TextArea->SetSizePolicy(Leir::SizePolicy::Fill);
    AddChild(m_TextArea);

    m_StatusLabel = new Leir::UILabel();
    m_StatusLabel->SetText("area: cursor=0 line=0 col=0 sel=-1");
    m_StatusLabel->SetFontSize(10);
    m_StatusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
    m_StatusLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_StatusLabel);
}

TextAreaDebugPanel::~TextAreaDebugPanel() = default;

void TextAreaDebugPanel::SetFont(Leir::Font* font)
{
    if (m_TextArea) m_TextArea->SetFont(font);
    if (m_TitleLabel) m_TitleLabel->SetFont(font);
    if (m_StatusLabel) m_StatusLabel->SetFont(font);
}

void TextAreaDebugPanel::Refresh()
{
    if (!m_TextArea || !m_StatusLabel) return;
    const auto& text = m_TextArea->GetText();
    std::string escaped;
    for (char c : text) {
        if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else escaped += c;
    }
    m_StatusLabel->SetText("area: \"" + escaped + "\" cursor=" + std::to_string(m_TextArea->GetCursorPos())
        + " line=" + std::to_string(m_TextArea->GetCursorLine())
        + " col=" + std::to_string(m_TextArea->GetCursorCol())
        + " sel=" + std::to_string(m_TextArea->GetSelBegin()));
}
