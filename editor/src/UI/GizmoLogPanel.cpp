#include "GizmoLogPanel.h"
#include "LeirEngine/Core/Settings.h"
#include "LeirEngine/Core/Log.h"
#include "LeirEngine/UI/Font.h"

#include <chrono>
#include <filesystem>

namespace {

// <configDir>/LeirEngine/records/record_gizmo_log.txt — same config dir as
// settings.json (GetPath() returns <configDir>/LeirEngine/settings.json).
std::string LogFilePath()
{
    std::error_code ec;
    std::filesystem::path settingsPath = Leir::LeirSettings::Get().GetPath();
    std::filesystem::path dir = settingsPath.parent_path() / "records";
    std::filesystem::create_directories(dir, ec);
    return (dir / "record_gizmo_log.txt").string();
}

std::string Timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return buf;
}

} // namespace

GizmoLogPanel::GizmoLogPanel()
{
    SetName("GizmoLogPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(6.0f);

    auto* title = new Leir::UILabel();
    title->SetText("-- DBG --");
    title->SetFontSize(11);
    title->SetColor({0.7f, 0.7f, 0.7f, 1.0f});
    title->SetSizePolicy(Leir::SizePolicy::Fixed);
    AddChild(title);

    // Row wrapper with Content size: lets the button keep its NATURAL width.
    // A Fixed child inside a Column always stretches to the panel's full width
    // (ComputeColumnLayout forces childW = innerW), which is why the record
    // button grew in width; the Row lets it size to its text.
    auto* btnRow = new Leir::UIPanel();
    btnRow->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    btnRow->SetLayoutMode(Leir::LayoutMode::Row);
    btnRow->SetSizePolicy(Leir::SizePolicy::Content);
    AddChild(btnRow);

    m_Button = new Leir::UIButton();
    // Fixed + explicit min size: rectangular button, ~40% wider than its text
    // (no default min-size setter for buttons; this also avoids the Column
    // layout stretching the button to the panel's full width).
    m_Button->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_Button->SetMinSize({200.0f, 34.0f});
    m_Button->SetOnClick([this]() { ToggleRecording(); });
    btnRow->AddChild(m_Button);

    ApplyButtonState();
}

GizmoLogPanel::~GizmoLogPanel()
{
    // Close the log file WITHOUT touching child UI. SetRecording(false) ->
    // ApplyButtonState() dereferences m_Button, but during DeleteUiSubtree the
    // children (m_Button) are freed BEFORE this dtor runs, so it was a
    // use-after-free (crash 0xC0000005 at shutdown when recording was active).
    if (m_File) {
        std::fclose(m_File);
        m_File = nullptr;
    }
    m_Recording = false;
}

void GizmoLogPanel::SetFont(Leir::Font* font)
{
    // Recursive: children may be nested inside layout wrappers (e.g. the Row
    // that keeps the button at its natural width).
    for (auto* child : GetChildren()) {
        if (auto* b = dynamic_cast<Leir::UIButton*>(child))
            b->SetFont(font);
        else if (auto* l = dynamic_cast<Leir::UILabel*>(child))
            l->SetFont(font);
        else if (auto* p = dynamic_cast<Leir::UIPanel*>(child))
            for (auto* sub : p->GetChildren()) {
                if (auto* sb = dynamic_cast<Leir::UIButton*>(sub))
                    sb->SetFont(font);
                else if (auto* sl = dynamic_cast<Leir::UILabel*>(sub))
                    sl->SetFont(font);
            }
    }
}

void GizmoLogPanel::SetRecording(bool on)
{
    if (m_Recording == on)
        return;
    m_Recording = on;

    if (on) {
        const std::string path = LogFilePath();
        m_File = std::fopen(path.c_str(), "w");
        if (m_File) {
            Leir::XConsole::Println("Gizmo log recording started -> {}", path);
        } else {
            Leir::XConsole::PrintError("Gizmo log: cannot open {}", path);
            m_Recording = false;
        }
    } else {
        if (m_File) {
            std::fclose(m_File);
            m_File = nullptr;
            Leir::XConsole::Println("Gizmo log recording stopped -> {}",
                LogFilePath());
        }
    }
    ApplyButtonState();
}

void GizmoLogPanel::RecordLine(const std::string& line)
{
    if (!m_Recording || !m_File)
        return;
    std::fprintf(m_File, "[%s] %s\n", Timestamp().c_str(), line.c_str());
    std::fflush(m_File);
}

Leir::Vector2 GizmoLogPanel::GetMinSize() const
{
    return {220.0f, 90.0f};
}

void GizmoLogPanel::OnLayoutComputed()
{
    Leir::UIPanel::OnLayoutComputed();
}

void GizmoLogPanel::ApplyButtonState()
{
    if (!m_Button)
        return;
    if (m_Recording) {
        m_Button->SetText("recording...");
        m_Button->SetColors({0.55f, 0.12f, 0.12f, 1.0f},
                            {0.7f, 0.2f, 0.2f, 1.0f},
                            {0.4f, 0.08f, 0.08f, 1.0f});
        m_Button->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
    } else {
        m_Button->SetText("record gizmo log");
        m_Button->SetColors({0.7f, 0.2f, 0.2f, 1.0f},
                            {0.85f, 0.3f, 0.3f, 1.0f},
                            {0.5f, 0.12f, 0.12f, 1.0f});
        m_Button->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
    }
}