#include "TextAreaWrapPanel.h"

TextAreaWrapPanel::TextAreaWrapPanel()
{
    SetName("DebugTextAreaWrapPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(3.0f);

    m_TitleLabel = new Leir::UILabel();
    m_TitleLabel->SetText("-- Text Area Wrap --");
    m_TitleLabel->SetFontSize(11);
    m_TitleLabel->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
    m_TitleLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_TitleLabel);

    m_WrapButton = new Leir::UIButton();
    m_WrapButton->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_WrapButton->SetOnClick([this]() {
        if (m_TextArea) {
            m_TextArea->SetWordWrap(!m_TextArea->IsWordWrapEnabled());
            UpdateWrapButton();
        }
    });
    AddChild(m_WrapButton);

    m_TextArea = new Leir::UITextArea();
    m_TextArea->SetName("TextAreaWrapPanelArea");
    m_TextArea->SetPlaceholder("multiline wrap test...");
    m_TextArea->SetSizePolicy(Leir::SizePolicy::Fill);
    m_TextArea->SetWordWrap(true);
    AddChild(m_TextArea);

    // A long paragraph that wraps (or overflows horizontally when wrap is off).
    std::string text =
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
        "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, "
        "quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo "
        "consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse "
        "cillum dolore eu fugiat nulla pariatur.\n"
        "\n"
        "Segunda linea: supercalifragilisticoespialidoso y otras palabras muy largas "
        "que se cortan en la palabra cuando el wrap esta activo.\n"
        "Tercera linea corta.\n";
    m_TextArea->SetText(text);

    m_StatusLabel = new Leir::UILabel();
    m_StatusLabel->SetText("wrap=ON lines=1 visual=1");
    m_StatusLabel->SetFontSize(10);
    m_StatusLabel->SetColor({0.5f, 0.8f, 0.5f, 1.0f});
    m_StatusLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_StatusLabel);

    UpdateWrapButton();
}

TextAreaWrapPanel::~TextAreaWrapPanel() = default;

void TextAreaWrapPanel::SetFont(Leir::Font* font)
{
    if (m_TextArea) m_TextArea->SetFont(font);
    if (m_TitleLabel) m_TitleLabel->SetFont(font);
    if (m_WrapButton) m_WrapButton->SetFont(font);
    if (m_StatusLabel) m_StatusLabel->SetFont(font);
}

void TextAreaWrapPanel::Refresh()
{
    if (!m_TextArea) return;
    const bool wrap = m_TextArea->IsWordWrapEnabled();

    const std::string& text = m_TextArea->GetText();
    int logical = 1;
    for (char c : text)
        if (c == '\n') ++logical;

    const int visual = (int)m_TextArea->GetLineCount();
    m_StatusLabel->SetText("wrap=" + std::string(wrap ? "ON" : "off")
        + " lines=" + std::to_string(logical)
        + " visual=" + std::to_string(visual)
        + " cursor=" + std::to_string(m_TextArea->GetCursorPos()));
}

void TextAreaWrapPanel::UpdateWrapButton()
{
    if (!m_WrapButton) return;
    const bool wrap = m_TextArea && m_TextArea->IsWordWrapEnabled();
    m_WrapButton->SetText(wrap ? "Word Wrap: ON" : "Word Wrap: OFF");
    if (wrap)
        m_WrapButton->SetColors({0.2f, 0.45f, 0.25f, 1.0f}, {0.3f, 0.6f, 0.35f, 1.0f}, {0.12f, 0.3f, 0.18f, 1.0f});
    else
        m_WrapButton->SetColors({0.45f, 0.3f, 0.2f, 1.0f}, {0.6f, 0.4f, 0.3f, 1.0f}, {0.3f, 0.18f, 0.12f, 1.0f});
}
