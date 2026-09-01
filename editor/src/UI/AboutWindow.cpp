#include "AboutWindow.h"
#include <LeirEngine/UI/UICanvas.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/RHI/RenderBackend.h>
#include <LeirEngine/Core/Log.h>
#include <cstdio>

AboutWindow::AboutWindow(Leir::RHI::RenderBackend* backend, const std::string& engineVersion)
    : Leir::UIWindowExternal(backend, "About LeirEngine")
    , m_EngineVersion(engineVersion)
    , m_BackendName(backend ? backend->GetBackendName() : "?")
{
    SetMinSize({360.0f, 280.0f});
    SetSize({360.0f, 280.0f});
    SetResizable(false);
}

void AboutWindow::OnShow()
{
    BuildContent();
    ApplyFont();
}

void AboutWindow::SetFont(Leir::Font* font)
{
    m_Font = font;
    // If the canvas already exists (Show() was called), apply now.
    if (GetCanvas()) {
        if (m_VersionLabel) m_VersionLabel->SetFont(font);
        if (m_BackendLabel) m_BackendLabel->SetFont(font);
        ApplyFont();
    }
}

void AboutWindow::BuildContent()
{
    Leir::UICanvas* canvas = GetCanvas();
    if (!canvas) return;

    // --- Title ---
    auto* title = new Leir::UILabel();
    title->SetName("AboutTitle");
    title->SetText("LeirEngine");
    title->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    title->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    title->GetRect().offset = {-100.0f, 24.0f, 100.0f, 52.0f};
    canvas->AddChild(title);

    // --- Version ---
    auto* verTitle = new Leir::UILabel();
    verTitle->SetName("AboutVerTitle");
    verTitle->SetText("Version:");
    verTitle->SetColor({0.7f, 0.7f, 0.75f, 1.0f});
    verTitle->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    verTitle->GetRect().offset = {-120.0f, 60.0f, -20.0f, 80.0f};
    canvas->AddChild(verTitle);

    m_VersionLabel = new Leir::UILabel();
    m_VersionLabel->SetName("AboutVersion");
    m_VersionLabel->SetText(m_EngineVersion);
    m_VersionLabel->SetColor({0.9f, 0.9f, 0.95f, 1.0f});
    m_VersionLabel->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    m_VersionLabel->GetRect().offset = {-20.0f, 60.0f, 120.0f, 80.0f};
    canvas->AddChild(m_VersionLabel);

    // --- Backend ---
    auto* bkTitle = new Leir::UILabel();
    bkTitle->SetName("AboutBkTitle");
    bkTitle->SetText("Backend:");
    bkTitle->SetColor({0.7f, 0.7f, 0.75f, 1.0f});
    bkTitle->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    bkTitle->GetRect().offset = {-120.0f, 88.0f, -20.0f, 108.0f};
    canvas->AddChild(bkTitle);

    // Build backend string from the actual backend.
    m_BackendLabel = new Leir::UILabel();
    m_BackendLabel->SetName("AboutBackend");
    m_BackendLabel->SetText(m_BackendName);
    m_BackendLabel->SetColor({0.9f, 0.9f, 0.95f, 1.0f});
    m_BackendLabel->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    m_BackendLabel->GetRect().offset = {-20.0f, 88.0f, 120.0f, 108.0f};
    canvas->AddChild(m_BackendLabel);

    // --- Developer ---
    auto* devLabel = new Leir::UILabel();
    devLabel->SetName("AboutDev");
    devLabel->SetText("Developer: LeirEngine Team");
    devLabel->SetColor({0.6f, 0.6f, 0.65f, 1.0f});
    devLabel->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    devLabel->GetRect().offset = {-120.0f, 120.0f, 120.0f, 140.0f};
    canvas->AddChild(devLabel);

    // --- OK button ---
    auto* okBtn = new Leir::UIButton();
    okBtn->SetName("AboutOK");
    okBtn->SetText("OK");
    okBtn->SetColors({0.35f, 0.45f, 0.65f, 1.0f},
                     {0.45f, 0.55f, 0.75f, 1.0f},
                     {0.25f, 0.35f, 0.55f, 1.0f});
    okBtn->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
    okBtn->GetRect().anchor = {0.5f, 0.0f, 0.5f, 0.0f};
    okBtn->GetRect().offset = {-50.0f, 210.0f, 50.0f, 240.0f};
    okBtn->SetOnClick([this]() {
        SetResult(Leir::WindowResult::Ok);
        Close();
    });
    canvas->AddChild(okBtn);
}

void AboutWindow::ApplyFont()
{
    if (!m_Font) return;
    // Apply font to all labels and buttons in the canvas.
    auto applyTo = [](Leir::UIElement* elem, Leir::Font* font) {
        if (auto* label = dynamic_cast<Leir::UILabel*>(elem)) {
            label->SetFont(font);
        } else if (auto* btn = dynamic_cast<Leir::UIButton*>(elem)) {
            btn->SetFont(font);
        }
    };
    // Walk direct children of the canvas (the background is the first child).
    for (auto* child : GetCanvas()->GetChildren()) {
        applyTo(child, m_Font);
    }
}