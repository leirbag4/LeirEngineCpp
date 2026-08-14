#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UITextInput.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/Font.h>
#include <functional>
#include <string>

// UITextInput that submits its content on Enter (like UIFloatInput): calls the
// callback with the current text, then clears the input.
class DebugSubmitInput : public Leir::UITextInput {
public:
    void SetOnSubmit(std::function<void(const std::string&)> cb) { m_OnSubmit = std::move(cb); }
    bool OnKeyDown(int key) override;

private:
    std::function<void(const std::string&)> m_OnSubmit;
};

// Console stress-test panel: type a message, press Enter to send it to the
// console once, or use X10/X50/X100 to flood the console with copies (one per
// line) to measure layout/render cost with lots of text.
class DebugPanel : public Leir::UIPanel {
public:
    DebugPanel();
    ~DebugPanel() override;

    void SetFont(Leir::Font* font);

    // Shader tooling hooks (wired from main.cpp when LEIR_EDITOR_SLANG).
    void SetOnExportShaders(std::function<void()> cb) { m_OnExportShaders = std::move(cb); }
    void SetOnReloadShaders(std::function<void()> cb) { m_OnReloadShaders = std::move(cb); }

private:
    void SendToConsole(const std::string& text, int count);

    Leir::UILabel* m_TitleLabel = nullptr;
    DebugSubmitInput* m_Input = nullptr;
    Leir::UIButton* m_BtnX10 = nullptr;
    Leir::UIButton* m_BtnX50 = nullptr;
    Leir::UIButton* m_BtnX100 = nullptr;
    Leir::UIButton* m_BtnExport = nullptr;
    Leir::UIButton* m_BtnReload = nullptr;
    std::function<void()> m_OnExportShaders;
    std::function<void()> m_OnReloadShaders;
};
