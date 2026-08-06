#include "LeirEngine/UI/UIDebugOverlay.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Pointer.h"
#include "LeirEngine/Core/Settings.h"
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <cstdio>
#include <cstring>
#endif

namespace Leir {

namespace {

// Process memory in bytes (working set + peak). Fallbacks return 0 when the
// platform can't provide the info.
void GetProcessMemory(std::uint64_t& workingSet, std::uint64_t& peakWorkingSet)
{
    workingSet = 0;
    peakWorkingSet = 0;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        workingSet = pmc.WorkingSetSize;
        peakWorkingSet = pmc.PeakWorkingSetSize;
    }
#elif defined(__APPLE__)
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        workingSet = info.resident_size;
        peakWorkingSet = info.virtual_size;
    }
#else
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) return;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        std::uint64_t kb = 0;
        if (std::strncmp(line, "VmRSS:", 6) == 0 && std::sscanf(line + 6, "%llu", &kb) == 1)
            workingSet = kb * 1024ull;
        else if (std::strncmp(line, "VmHWM:", 6) == 0 && std::sscanf(line + 6, "%llu", &kb) == 1)
            peakWorkingSet = kb * 1024ull;
    }
    std::fclose(f);
#endif
}

std::string FormatBytes(std::uint64_t bytes)
{
    if (bytes >= 1024ull * 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull * 1024ull)) + "." +
               std::to_string((bytes % (1024ull * 1024ull * 1024ull)) / (1024ull * 1024ull) / 10) + " GB";
    if (bytes >= 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull)) + "." +
               std::to_string((bytes % (1024ull * 1024ull)) / (1024ull * 1024ull / 10)) + " MB";
    if (bytes >= 1024ull)
        return std::to_string(bytes / 1024ull) + " KB";
    return std::to_string(bytes) + " B";
}

} // namespace

UIDebugOverlay::UIDebugOverlay(Font* font, UICanvas* canvas)
    : m_Canvas(canvas)
{
    CreatePanel(font);
}

UIDebugOverlay::~UIDebugOverlay()
{
    if (m_Panel && m_Canvas) {
        m_Canvas->RemoveChild(m_Panel);
        delete m_Panel;
    }
}

void UIDebugOverlay::CreatePanel(Font* font)
{
    m_Panel = new UIPanel();
    m_Panel->SetName("DebugOverlay");
    m_Panel->GetRect().anchor = {0.0f, 0.0f, 0.0f, 0.0f};
    m_Panel->GetRect().offset = {274.0f, 10.0f, 590.0f, 340.0f};
    m_Panel->SetColor({0.08f, 0.08f, 0.1f, 0.85f});
    m_Panel->SetOverlayLayer(true);   // always on top of the dock/viewports
    m_Panel->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    m_Panel->SetLayoutMode(LayoutMode::Column);
    m_Panel->SetSpacing(3.0f);

    auto makeLabel = [&](const std::string& name, const std::string& text) -> UILabel* {
        auto* label = new UILabel();
        label->SetName(name);
        label->SetFont(font);
        label->SetText(text);
        label->SetColor({0.9f, 0.9f, 0.9f, 1.0f});
        label->SetFontSize(14);
        label->SetSizePolicy(SizePolicy::Fixed);
        m_Panel->AddChild(label);
        return label;
    };

    m_FpsLabel = makeLabel("DebugFps", "FPS: --");
    m_FrameTimeLabel = makeLabel("DebugFrameTime", "Frame: -- ms");
    m_DrawCallsLabel = makeLabel("DebugDrawCalls", "DrawCalls: --  (-- quads)");
    m_MemoryLabel = makeLabel("DebugMemory", "Mem: -- MB  (peak -- MB)");
    m_MouseLabel = makeLabel("DebugMouse", "Mouse: --, --");
    m_ButtonsLabel = makeLabel("DebugButtons", "Btns: [L] [R] [M]");
    m_KeysLabel = makeLabel("DebugKeys", "Keys: --");
    m_HoverLabel = makeLabel("DebugHover", "Hover: --");
    m_LastEventLabel = makeLabel("DebugLastEvent", "Event: --");

    m_Canvas->AddChild(m_Panel);
}

void UIDebugOverlay::Update(float deltaTime)
{
    if (!m_Active || !m_Panel) return;

    // FPS
    m_FpsAccum += deltaTime;
    m_FrameCount++;
    if (m_FpsAccum >= 0.5f) {
        m_CurrentFps = m_FrameCount / m_FpsAccum;
        m_FpsAccum = 0.0f;
        m_FrameCount = 0;
    }
    m_FpsLabel->SetText("FPS: " + std::to_string((int)m_CurrentFps));

    // Frame time
    float frameMs = m_CurrentFps > 0.0f ? 1000.0f / m_CurrentFps : 0.0f;
    char frameBuf[32];
    snprintf(frameBuf, sizeof(frameBuf), "%.2f", frameMs);
    m_FrameTimeLabel->SetText(std::string("Frame: ") + frameBuf + " ms");

    // Render stats (from the previous frame's Flush)
    if (m_StatsProvider) {
        UIRenderStats stats = m_StatsProvider();
        m_DrawCallsLabel->SetText(
            "DrawCalls: " + std::to_string(stats.drawCalls) +
            "  (" + std::to_string(stats.quads) + " quads, " +
            std::to_string(stats.vertices) + " verts)");
    } else {
        m_DrawCallsLabel->SetText("DrawCalls: --  (-- quads)");
    }

    // Process memory
    std::uint64_t workingSet = 0, peakWorkingSet = 0;
    GetProcessMemory(workingSet, peakWorkingSet);
    m_MemoryLabel->SetText(
        "Mem: " + FormatBytes(workingSet) +
        "  (peak " + FormatBytes(peakWorkingSet) + ")");

    // Mouse position
    auto mousePos = Mouse::GetPos();
    m_MouseLabel->SetText(
        "Mouse: " + std::to_string((int)mousePos.x) +
        ", " + std::to_string((int)mousePos.y)
    );

    // Buttons
    std::string btns = "Btns: ";
    btns += Mouse::IsDown(PointerButton::Left)   ? "[L] " : "[ ] ";
    btns += Mouse::IsDown(PointerButton::Right)  ? "[R] " : "[ ] ";
    btns += Mouse::IsDown(PointerButton::Middle) ? "[M]"  : "[ ]";
    m_ButtonsLabel->SetText(btns);

    // Keys
    std::string keys = Keyboard::GetPressedKeysString();
    m_KeysLabel->SetText(keys.empty() ? "Keys: --" : "Keys: " + keys);

    // Hovered element
    UIElement* hovered = m_Canvas->GetHoveredElement();
    if (hovered)
        m_HoverLabel->SetText("Hover: " + hovered->GetName());
    else
        m_HoverLabel->SetText("Hover: --");

    // Event tracking. Uses the hovered element's NAME rather than retaining a
    // pointer to it: when the user docks a panel the previously-hovered element
    // can be deleted, and dereferencing a freed element corrupts its std::string
    // (→ bad_alloc on the next concatenation).
    std::string hoveredName = hovered ? hovered->GetName() : std::string();
    if (hoveredName != m_LastHoveredName) {
        if (!hoveredName.empty())
            m_LastEvent = "Hover: " + hoveredName;
        else if (!m_LastHoveredName.empty())
            m_LastEvent = "Exit: " + m_LastHoveredName;
        m_LastHoveredName = hoveredName;
        m_LastEventFrames = 0;
    }

    if (Pointer::WasPressed(PointerButton::Primary)) {
        std::string name = hovered ? hovered->GetName() : "bg";
        m_LastEvent = "Press: " + name;
        m_LastEventFrames = 0;
    }
    if (Pointer::WasReleased(PointerButton::Primary)) {
        std::string name = hovered ? hovered->GetName() : "bg";
        m_LastEvent = "Release: " + name;
        m_LastEventFrames = 0;
    }

    Key lastKey = Keyboard::GetLastPressedKey();
    if (lastKey != Key::Unknown) {
        std::string keyName;
        auto k = static_cast<int32_t>(lastKey);
        if (k >= 65 && k <= 90) keyName = static_cast<char>(k);
        else if (k >= 48 && k <= 57) keyName = static_cast<char>(k);
        else if (lastKey == Key::Space) keyName = "Space";
        else if (lastKey == Key::Enter) keyName = "Enter";
        else if (lastKey == Key::Escape) keyName = "Esc";
        else keyName = "[" + std::to_string(k) + "]";
        m_LastEvent = "Key: " + keyName;
        m_LastEventFrames = 0;
    }

    if (m_LastEventFrames < 120)
        m_LastEventFrames++;
    if (m_LastEventFrames < 120) {
        m_LastEventLabel->SetText("Event: " + m_LastEvent);
    } else {
        m_LastEventLabel->SetText("Event: --");
    }

    // Check settings toggle
    m_Active = LeirSettings::Get().debug.show_overlay;
    m_Panel->SetActive(m_Active);
}

void UIDebugOverlay::SetActive(bool active)
{
    m_Active = active;
    if (m_Panel)
        m_Panel->SetActive(active);
}

} // namespace Leir
