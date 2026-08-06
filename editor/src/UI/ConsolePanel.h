#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/ScrollView.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Core/Log.h>
#include <cstdint>
#include <string>

// Unity-style editor console: filter buttons (Info/Warn/Error) + Clear, and a
// ScrollView with per-level colored lines (timestamp + message). Lines are
// rebuilt lazily only when new messages arrive, the filters change, or Clear.
// Auto-follows the bottom while the user is at the bottom.
class ConsolePanel : public Leir::UIPanel {
public:
    ConsolePanel();
    ~ConsolePanel() override;

    void SetFont(Leir::Font* font);
    void Refresh();

private:
    void RebuildLines();
    void ToggleFilter(Leir::LogLevel level, bool on);
    void SyncButtonColors();
    void ClearHoverIfInside();

    Leir::ScrollView* m_ScrollView = nullptr;
    Leir::UIElement* m_LineColumn = nullptr;
    Leir::UIButton* m_BtnInfo = nullptr;
    Leir::UIButton* m_BtnWarning = nullptr;
    Leir::UIButton* m_BtnError = nullptr;
    Leir::UIButton* m_BtnClear = nullptr;
    Leir::Font* m_Font = nullptr;

    bool m_ShowInfo = true;
    bool m_ShowWarning = true;
    bool m_ShowError = true;

    uint64_t m_LastVersion = UINT64_MAX;
    int m_FilterStamp = 0;
    int m_LastFilterStamp = -1;
};
