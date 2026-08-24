#include "LeirEngine/UI/UIDebugOverlay.h"
#include "LeirEngine/UI/UICanvas.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/UIButton.h"
#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Input/Mouse.h"
#include "LeirEngine/Input/Keyboard.h"
#include "LeirEngine/Input/Pointer.h"
#include "LeirEngine/Core/Settings.h"
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <climits>

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

// Draggable title bar of the Stats panel. Forwards pointer events to the owner
// overlay; dragging only starts while the panel is maximized (the minimized
// panel is pinned to the viewport bottom-right). The minimize/maximize button
// is a child, so clicking it is consumed by the button and never reaches here.
class OverlayTitleBar : public UIPanel {
public:
    explicit OverlayTitleBar(UIDebugOverlay* owner) : m_Owner(owner) { SetName("StatsTitleBar"); }
    bool OnPointerDown(const Vector2& pos) override {
        if (m_Owner && !m_Owner->IsMinimized()) { m_Owner->BeginTitleDrag(this, pos); return true; }
        return false;
    }
    void OnPointerMove(const Vector2& pos) override {
        if (m_Owner) m_Owner->TitleDragTo(pos);
    }
    bool OnPointerUp(const Vector2& pos) override {
        if (m_Owner) m_Owner->EndTitleDrag(pos);
        return false;
    }
private:
    UIDebugOverlay* m_Owner = nullptr;
};

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
    m_Panel->SetLayoutMode(LayoutMode::Column);
    m_Panel->SetSpacing(0.0f);

    // Title bar (draggable when maximized): "Stats" + spacer + -/+ toggle.
    m_HeaderRow = new OverlayTitleBar(this);
    m_HeaderRow->SetName("StatsTitleBar");
    m_HeaderRow->SetLayoutMode(LayoutMode::Row);
    m_HeaderRow->SetSizePolicy(SizePolicy::Fixed);
    m_HeaderRow->SetMinSize({0.0f, 24.0f});
    // Vertical padding leaves ~3px margin around the -/+ button (smaller than
    // the bar); translucent when expanded (ApplyMaximizedLayout), solid when
    // minimized (ApplyMinimizedLayout).
    m_HeaderRow->SetPadding(8.0f, 3.0f, 4.0f, 3.0f);
    m_HeaderRow->SetColor({0.13f, 0.13f, 0.16f, 0.60f});
    m_Panel->AddChild(m_HeaderRow);

    m_TitleLabel = new UILabel();
    m_TitleLabel->SetName("StatsTitle");
    m_TitleLabel->SetText("Stats");
    m_TitleLabel->SetFont(font);
    m_TitleLabel->SetFontSize(13);
    m_TitleLabel->SetColor({0.9f, 0.9f, 0.9f, 1.0f});
    m_TitleLabel->SetSizePolicy(SizePolicy::Fixed);
    m_HeaderRow->AddChild(m_TitleLabel);

    auto* spacer = new UIPanel();
    spacer->SetName("StatsTitleSpacer");
    spacer->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    spacer->SetSizePolicy(SizePolicy::Fill);
    m_HeaderRow->AddChild(spacer);

    m_MinMaxButton = new UIButton();
    m_MinMaxButton->SetName("StatsMinMaxButton");
    m_MinMaxButton->SetFont(font);
    m_MinMaxButton->SetText("-");
    m_MinMaxButton->SetSizePolicy(SizePolicy::Fixed);
    m_MinMaxButton->SetMinSize({20.0f, 18.0f}); // smaller than the 24px bar (3px margin)
    m_MinMaxButton->SetTextAlign(ButtonTextAlign::Center);
    m_MinMaxButton->SetColors({0.25f, 0.25f, 0.30f, 1.0f},
                              {0.35f, 0.35f, 0.42f, 1.0f},
                              {0.15f, 0.15f, 0.20f, 1.0f});
    m_MinMaxButton->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
    m_MinMaxButton->SetOnClick([this]() { ToggleMinimized(); });
    m_HeaderRow->AddChild(m_MinMaxButton);

    // Content panel (hidden when minimized) holding the stat labels.
    m_ContentPanel = new UIPanel();
    m_ContentPanel->SetName("StatsContent");
    m_ContentPanel->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    m_ContentPanel->SetLayoutMode(LayoutMode::Column);
    m_ContentPanel->SetSizePolicy(SizePolicy::Fill);
    m_ContentPanel->SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    m_ContentPanel->SetSpacing(3.0f);
    m_Panel->AddChild(m_ContentPanel);

    auto makeLabel = [&](const std::string& name, const std::string& text) -> UILabel* {
        auto* label = new UILabel();
        label->SetName(name);
        label->SetFont(font);
        label->SetText(text);
        label->SetColor({0.9f, 0.9f, 0.9f, 1.0f});
        label->SetFontSize(14);
        label->SetSizePolicy(SizePolicy::Fixed);
        m_ContentPanel->AddChild(label);
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

    // Apply persisted position/state (maximized default), then the layout.
    RestoreState();
    if (m_Minimized)
        ApplyMinimizedLayout();
    else
        ApplyMaximizedLayout();
}

void UIDebugOverlay::SetFont(Font* font)
{
    if (!m_Panel || !font) return;
    UILabel* labels[] = {
        m_FpsLabel, m_FrameTimeLabel, m_DrawCallsLabel, m_MemoryLabel,
        m_MouseLabel, m_ButtonsLabel, m_KeysLabel, m_HoverLabel, m_LastEventLabel
    };
    for (UILabel* l : labels)
        if (l) l->SetFont(font);
    if (m_TitleLabel) m_TitleLabel->SetFont(font);
    if (m_MinMaxButton) m_MinMaxButton->SetFont(font);
}

void UIDebugOverlay::ApplyMaximizedLayout()
{
    m_Minimized = false;
    if (m_ContentPanel) m_ContentPanel->SetActive(true);
    if (m_MinMaxButton) m_MinMaxButton->SetText("-");
    // Translucent title bar when expanded (matches the panel's translucent bg).
    if (m_HeaderRow) m_HeaderRow->SetColor({0.13f, 0.13f, 0.16f, 0.60f});
    const auto& r = m_MaximizedRect;
    m_Panel->GetRect().anchor = {0, 0, 0, 0};
    m_Panel->GetRect().offset = {r.x, r.y, r.x + r.z, r.y + r.w};
}

void UIDebugOverlay::ApplyMinimizedLayout()
{
    m_Minimized = true;
    if (m_ContentPanel) m_ContentPanel->SetActive(false);
    if (m_MinMaxButton) m_MinMaxButton->SetText("+");
    // Solid title bar when minimized (a solid little collapsed bar).
    if (m_HeaderRow) m_HeaderRow->SetColor({0.13f, 0.13f, 0.16f, 1.0f});
    // Pin to the viewport's bottom-right corner (inside the viewport rect).
    const float kW = 170.0f, kH = 24.0f, kMargin = 8.0f;
    float x = 0.0f, y = 0.0f;
    if (m_ViewportRectProvider) {
        Vector4 vp = m_ViewportRectProvider();
        x = vp.x + vp.z - kW - kMargin;
        y = vp.y + vp.w - kH - kMargin;
        if (x < vp.x) x = vp.x;
        if (y < vp.y) y = vp.y;
    }
    m_Panel->GetRect().anchor = {0, 0, 0, 0};
    m_Panel->GetRect().offset = {x, y, x + kW, y + kH};
}

void UIDebugOverlay::RestoreState()
{
    const auto& s = LeirSettings::Get().debug.stats;
    const float w = m_MaximizedRect.z, h = m_MaximizedRect.w;
    m_MaximizedRect.x = (s.pos_x != INT_MIN) ? (float)s.pos_x : 274.0f;
    m_MaximizedRect.y = (s.pos_y != INT_MIN) ? (float)s.pos_y : 10.0f;
    m_MaximizedRect.z = w;
    m_MaximizedRect.w = h;
    m_Minimized = s.minimized;
}

void UIDebugOverlay::SaveState()
{
    auto& s = LeirSettings::Get().debug.stats;
    s.pos_x = (int)m_MaximizedRect.x;
    s.pos_y = (int)m_MaximizedRect.y;
    s.minimized = m_Minimized;
    LeirSettings::Get().Save();
}

void UIDebugOverlay::BeginTitleDrag(UIPanel* titleBar, const Vector2& pos)
{
    if (m_Minimized || !titleBar) return;
    m_Dragging = true;
    m_DragStart = pos;
    m_StartOffset = {m_Panel->GetRect().offset.left, m_Panel->GetRect().offset.top};
    for (UIElement* e = titleBar; e; e = e->GetParent()) {
        if (auto* c = dynamic_cast<UICanvas*>(e)) { c->CapturePointer(titleBar); break; }
    }
}

void UIDebugOverlay::TitleDragTo(const Vector2& pos)
{
    if (!m_Dragging || m_Minimized) return;
    const auto& o = m_Panel->GetRect().offset;
    const float w = o.right - o.left;
    const float h = o.bottom - o.top;
    float nx = m_StartOffset.x + (pos.x - m_DragStart.x);
    float ny = m_StartOffset.y + (pos.y - m_DragStart.y);
    // Clamp to the canvas so the panel never goes off-screen (windows.h defines
    // a `max` macro, so avoid std::max here).
    float maxX = m_Canvas->GetScreenWidth() - w;
    float maxY = m_Canvas->GetScreenHeight() - h;
    if (maxX < 0.0f) maxX = 0.0f;
    if (maxY < 0.0f) maxY = 0.0f;
    nx = std::clamp(nx, 0.0f, maxX);
    ny = std::clamp(ny, 0.0f, maxY);
    m_Panel->GetRect().offset = {nx, ny, nx + w, ny + h};
    m_MaximizedRect = {nx, ny, w, h};
}

void UIDebugOverlay::EndTitleDrag(const Vector2& pos)
{
    (void)pos;
    if (!m_Dragging) return;
    m_Dragging = false;
    if (m_HeaderRow) {
        for (UIElement* e = m_HeaderRow; e; e = e->GetParent()) {
            if (auto* c = dynamic_cast<UICanvas*>(e)) { c->ReleasePointer(); break; }
        }
    }
    SaveState();
}

void UIDebugOverlay::ToggleMinimized()
{
    if (m_Minimized) {
        ApplyMaximizedLayout();
    } else {
        const auto& cr = m_Panel->GetComputedRect();
        m_MaximizedRect = {cr.x, cr.y, cr.z, cr.w};
        ApplyMinimizedLayout();
    }
    SaveState();
}

void UIDebugOverlay::Update(float deltaTime)
{
    if (!m_Active || !m_Panel) return;

    // Keep the minimized panel pinned to the viewport's bottom-right corner
    // (it follows window/viewport resizes; not draggable while minimized).
    if (m_Minimized && m_Panel->IsActive() && m_ViewportRectProvider) {
        Vector4 vp = m_ViewportRectProvider();
        if (vp.z > 0.0f && vp.w > 0.0f) {
            const auto& o = m_Panel->GetRect().offset;
            const float w = o.right - o.left, h = o.bottom - o.top;
            const float kMargin = 8.0f;
            float x = vp.x + vp.z - w - kMargin;
            float y = vp.y + vp.w - h - kMargin;
            if (x < vp.x) x = vp.x;
            if (y < vp.y) y = vp.y;
            if (o.left != x || o.top != y)
                m_Panel->GetRect().offset = {x, y, x + w, y + h};
        }
    }

    // Render stats (from the previous frame's Flush)
    UIRenderStats stats = m_StatsProvider ? m_StatsProvider() : UIRenderStats{};

    // FPS + per-frame stat averages share one 0.5s smoothing window.
    m_FpsAccum += deltaTime;
    m_FrameCount++;
    m_DrawCallsAccum += stats.drawCalls;
    m_QuadsAccum += stats.quads;
    if (m_FpsAccum >= 0.5f) {
        m_CurrentFps = m_FrameCount / m_FpsAccum;
        if (m_FrameCount > 0) {
            m_AvgDrawCalls = (uint32_t)(m_DrawCallsAccum / m_FrameCount);
            m_AvgQuads = (uint32_t)(m_QuadsAccum / m_FrameCount);
        }
        m_FpsAccum = 0.0f;
        m_FrameCount = 0;
        m_DrawCallsAccum = 0;
        m_QuadsAccum = 0;
    }
    m_FpsLabel->SetText("FPS: " + std::to_string((int)m_CurrentFps));

    // Frame time
    float frameMs = m_CurrentFps > 0.0f ? 1000.0f / m_CurrentFps : 0.0f;
    char frameBuf[32];
    snprintf(frameBuf, sizeof(frameBuf), "%.2f", frameMs);
    m_FrameTimeLabel->SetText(std::string("Frame: ") + frameBuf + " ms");

    if (m_StatsProvider) {
        m_DrawCallsLabel->SetText(
            "DrawCalls: " + std::to_string(stats.drawCalls) +
            " (avg " + std::to_string(m_AvgDrawCalls) + ")" +
            "  (" + std::to_string(stats.quads) + " quads)");
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
