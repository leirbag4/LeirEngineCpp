#include "ConsolePanel.h"
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UICanvas.h>
#include <algorithm>

namespace {

constexpr size_t kMaxLines = 300;

Leir::Vector4 LevelColor(Leir::LogLevel level)
{
    switch (level) {
        case Leir::LogLevel::Warning: return {1.0f, 0.83f, 0.30f, 1.0f};
        case Leir::LogLevel::Error:   return {1.0f, 0.40f, 0.40f, 1.0f};
        default:                      return {0.85f, 0.85f, 0.88f, 1.0f};
    }
}

void DeleteUiSubtree(Leir::UIElement* e)
{
    if (!e)
        return;
    auto children = e->GetChildren();
    for (auto* c : children)
        DeleteUiSubtree(c);
    delete e;
}

} // namespace

ConsolePanel::ConsolePanel()
{
    SetName("ConsolePanel");
    SetColor({0.10f, 0.10f, 0.12f, 1.0f});
    SetPadding(4.0f, 4.0f, 4.0f, 4.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(4.0f);

    // Header: filter toggles + clear
    auto* header = new Leir::UIPanel();
    header->SetName("ConsoleHeader");
    header->SetColor({0.13f, 0.13f, 0.15f, 1.0f});
    header->SetLayoutMode(Leir::LayoutMode::Row);
    header->SetSpacing(4.0f);
    header->SetPadding(2.0f, 2.0f, 2.0f, 2.0f);
    header->SetSizePolicy(Leir::SizePolicy::Content);
    AddChild(header);

    m_BtnInfo = new Leir::UIButton();
    m_BtnInfo->SetName("ConsoleFilterInfo");
    m_BtnInfo->SetText("Info");
    m_BtnInfo->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_BtnInfo->SetOnClick([this]() { ToggleFilter(Leir::LogLevel::Info, !m_ShowInfo); });
    header->AddChild(m_BtnInfo);

    m_BtnWarning = new Leir::UIButton();
    m_BtnWarning->SetName("ConsoleFilterWarning");
    m_BtnWarning->SetText("Warning");
    m_BtnWarning->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_BtnWarning->SetOnClick([this]() { ToggleFilter(Leir::LogLevel::Warning, !m_ShowWarning); });
    header->AddChild(m_BtnWarning);

    m_BtnError = new Leir::UIButton();
    m_BtnError->SetName("ConsoleFilterError");
    m_BtnError->SetText("Error");
    m_BtnError->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_BtnError->SetOnClick([this]() { ToggleFilter(Leir::LogLevel::Error, !m_ShowError); });
    header->AddChild(m_BtnError);

    m_BtnClear = new Leir::UIButton();
    m_BtnClear->SetName("ConsoleClear");
    m_BtnClear->SetText("Clear");
    m_BtnClear->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_BtnClear->SetColors(
        {0.30f, 0.16f, 0.16f, 1.0f},
        {0.40f, 0.22f, 0.22f, 1.0f},
        {0.20f, 0.10f, 0.10f, 1.0f});
    m_BtnClear->SetOnClick([this]() { Leir::XConsole::Clear(); });
    header->AddChild(m_BtnClear);

    SyncButtonColors();

    // Body: scrollable lines
    m_ScrollView = new Leir::ScrollView();
    m_ScrollView->SetName("ConsoleScrollView");
    m_ScrollView->SetSizePolicy(Leir::SizePolicy::Fill);
    m_ScrollView->SetLineHeight(16.0f);
    AddChild(m_ScrollView);

    m_LineColumn = new Leir::UIElement();
    m_LineColumn->SetName("ConsoleLines");
    m_LineColumn->SetLayoutMode(Leir::LayoutMode::Column);
    m_LineColumn->SetPadding(0.0f, 2.0f, 0.0f, 0.0f);
    m_LineColumn->SetSpacing(1.0f);
    m_ScrollView->SetContent(m_LineColumn);
}

ConsolePanel::~ConsolePanel() = default;

void ConsolePanel::SetFont(Leir::Font* font)
{
    m_Font = font;
    if (m_Font)
        m_ScrollView->SetLineHeight(m_Font->GetLineHeight());
    m_BtnInfo->SetFont(font);
    m_BtnWarning->SetFont(font);
    m_BtnError->SetFont(font);
    m_BtnClear->SetFont(font);
    // Force a rebuild so existing labels get the new font.
    ++m_FilterStamp;
}

void ConsolePanel::Refresh()
{
    const uint64_t version = Leir::XConsole::GetVersion();
    if (version == m_LastVersion && m_FilterStamp == m_LastFilterStamp)
        return;

    m_LastVersion = version;
    m_LastFilterStamp = m_FilterStamp;

    bool atBottom = false;
    if (m_ScrollView) {
        const float maxY = m_ScrollView->GetMaxScrollY();
        atBottom = m_ScrollView->GetScrollOffset().y >= maxY - 1.0f;
    }

    ClearHoverIfInside();
    RebuildLines();

    // Auto-follow: if the user was at the bottom before rebuilding, stay there.
    if (m_ScrollView && atBottom)
        m_ScrollView->SetScrollOffset({0.0f, m_ScrollView->GetMaxScrollY()});
}

void ConsolePanel::RebuildLines()
{
    const auto msgs = Leir::XConsole::GetMessages();

    auto* newColumn = new Leir::UIElement();
    newColumn->SetName("ConsoleLines");
    newColumn->SetLayoutMode(Leir::LayoutMode::Column);
    newColumn->SetPadding(0.0f, 2.0f, 0.0f, 0.0f);
    newColumn->SetSpacing(1.0f);

    const size_t start = msgs.size() > kMaxLines ? msgs.size() - kMaxLines : 0;
    for (size_t i = start; i < msgs.size(); ++i) {
        const auto& m = msgs[i];
        if (m.level == Leir::LogLevel::Trace || m.level == Leir::LogLevel::Debug)
            continue; // Trace/Debug are never shown in the console
        if (m.level == Leir::LogLevel::Info && !m_ShowInfo) continue;
        if (m.level == Leir::LogLevel::Warning && !m_ShowWarning) continue;
        if (m.level == Leir::LogLevel::Error && !m_ShowError) continue;

        auto* lbl = new Leir::UILabel();
        lbl->SetName("ConsoleLine");
        lbl->SetFont(m_Font);
        lbl->SetText("[" + m.time + "] " + m.text);
        lbl->SetColor(LevelColor(m.level));
        lbl->SetSizePolicy(Leir::SizePolicy::Fixed);
        newColumn->AddChild(lbl);
    }

    if (m_ScrollView)
        m_ScrollView->SetContent(newColumn);
    DeleteUiSubtree(m_LineColumn);
    m_LineColumn = newColumn;

    Leir::XConsole::Trace("[Console] rebuilt {} lines (info={} warning={} error={})",
        newColumn->GetChildren().size(), m_ShowInfo, m_ShowWarning, m_ShowError);
}

void ConsolePanel::ToggleFilter(Leir::LogLevel level, bool on)
{
    if (level == Leir::LogLevel::Info) m_ShowInfo = on;
    else if (level == Leir::LogLevel::Warning) m_ShowWarning = on;
    else if (level == Leir::LogLevel::Error) m_ShowError = on;
    ++m_FilterStamp;
    SyncButtonColors();
}

void ConsolePanel::SyncButtonColors()
{
    auto apply = [](Leir::UIButton* b, bool on) {
        if (on)
            b->SetColors(
                {0.35f, 0.35f, 0.42f, 1.0f},
                {0.45f, 0.45f, 0.52f, 1.0f},
                {0.25f, 0.25f, 0.30f, 1.0f});
        else
            b->SetColors(
                {0.16f, 0.16f, 0.19f, 1.0f},
                {0.24f, 0.24f, 0.28f, 1.0f},
                {0.13f, 0.13f, 0.15f, 1.0f});
        b->SetTextColor(on ? Leir::Vector4{1.0f, 1.0f, 1.0f, 1.0f}
                           : Leir::Vector4{0.55f, 0.55f, 0.58f, 1.0f});
    };
    apply(m_BtnInfo, m_ShowInfo);
    apply(m_BtnWarning, m_ShowWarning);
    apply(m_BtnError, m_ShowError);
}

void ConsolePanel::ClearHoverIfInside()
{
    UIElement* e = this;
    while (e) {
        if (auto* c = dynamic_cast<Leir::UICanvas*>(e)) {
            auto* hovered = c->GetHoveredElement();
            if (hovered) {
                for (UIElement* p = hovered; p; p = p->GetParent()) {
                    if (p == this) {
                        c->ClearHoverAndFocus();
                        break;
                    }
                }
            }
            return;
        }
        e = e->GetParent();
    }
}
