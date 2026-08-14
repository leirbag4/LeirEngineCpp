#include "DebugPanel.h"
#include <LeirEngine/Core/Log.h>

bool DebugSubmitInput::OnKeyDown(int key)
{
    if (!IsFocused())
        return false;

    if (key == 257 || key == 335) { // Key::Enter / NumPadEnter
        const std::string text = GetText();
        SetText("");
        if (m_OnSubmit && !text.empty())
            m_OnSubmit(text);
        return true;
    }

    return Leir::UITextInput::OnKeyDown(key);
}

DebugPanel::DebugPanel()
{
    SetName("DebugPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(4.0f);

    m_TitleLabel = new Leir::UILabel();
    m_TitleLabel->SetText("-- Console Stress Test --");
    m_TitleLabel->SetFontSize(11);
    m_TitleLabel->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
    m_TitleLabel->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(m_TitleLabel);

    m_Input = new DebugSubmitInput();
    m_Input->SetName("DebugPanelInput");
    m_Input->SetPlaceholder("type a message, Enter to send...");
    m_Input->SetSizePolicy(Leir::SizePolicy::Fill);
    m_Input->SetOnSubmit([this](const std::string& text) { SendToConsole(text, 1); });
    AddChild(m_Input);

    auto* buttonRow = new Leir::UIPanel();
    buttonRow->SetName("DebugPanelButtons");
    buttonRow->SetLayoutMode(Leir::LayoutMode::Row);
    buttonRow->SetSpacing(4.0f);
    buttonRow->SetSizePolicy(Leir::SizePolicy::Content);
    AddChild(buttonRow);

    auto makeButton = [&](const std::string& label, int count) -> Leir::UIButton* {
        auto* btn = new Leir::UIButton();
        btn->SetText(label);
        btn->SetSizePolicy(Leir::SizePolicy::Fixed);
        btn->SetOnClick([this, count]() { SendToConsole(m_Input ? m_Input->GetText() : "", count); });
        buttonRow->AddChild(btn);
        return btn;
    };

    m_BtnX10 = makeButton("X10", 10);
    m_BtnX50 = makeButton("X50", 50);
    m_BtnX100 = makeButton("X100", 100);

    auto* shaderRow = new Leir::UIPanel();
    shaderRow->SetName("DebugPanelShaderButtons");
    shaderRow->SetLayoutMode(Leir::LayoutMode::Row);
    shaderRow->SetSpacing(4.0f);
    shaderRow->SetSizePolicy(Leir::SizePolicy::Content);
    AddChild(shaderRow);

    auto makeShaderButton = [&](const std::string& label, std::function<void()> cb) -> Leir::UIButton* {
        auto* btn = new Leir::UIButton();
        btn->SetText(label);
        btn->SetSizePolicy(Leir::SizePolicy::Fixed);
        btn->SetOnClick(std::move(cb));
        shaderRow->AddChild(btn);
        return btn;
    };

    m_BtnExport = makeShaderButton("Export Shaders", [this]() {
        if (m_OnExportShaders) m_OnExportShaders();
    });
    m_BtnReload = makeShaderButton("Reload Shaders", [this]() {
        if (m_OnReloadShaders) m_OnReloadShaders();
    });
}

DebugPanel::~DebugPanel() = default;

void DebugPanel::SetFont(Leir::Font* font)
{
    if (m_TitleLabel) m_TitleLabel->SetFont(font);
    if (m_Input) m_Input->SetFont(font);
    if (m_BtnX10) m_BtnX10->SetFont(font);
    if (m_BtnX50) m_BtnX50->SetFont(font);
    if (m_BtnX100) m_BtnX100->SetFont(font);
    if (m_BtnExport) m_BtnExport->SetFont(font);
    if (m_BtnReload) m_BtnReload->SetFont(font);
}

void DebugPanel::SendToConsole(const std::string& text, int count)
{
    if (text.empty())
        return;
    for (int i = 0; i < count; ++i)
        Leir::XConsole::Println("{}", text);
}
