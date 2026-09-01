#pragma once

#include <LeirEngine/UI/UIWindowExternal.h>
#include <LeirEngine/UI/UIButton.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/Math/Vector4.h>
#include <string>

// About dialog: shows engine + developer info in an external (undocked) window.
// Opened from Help -> About. OK closes with WindowResult::Ok.
class AboutWindow : public Leir::UIWindowExternal {
public:
    AboutWindow(Leir::RHI::RenderBackend* backend, const std::string& engineVersion);

protected:
    void OnShow() override;

public:
    void SetFont(Leir::Font* font);

private:
    void BuildContent();
    void ApplyFont();

    std::string m_EngineVersion;
    std::string m_BackendName;
    Leir::Font* m_Font = nullptr;   // not owned
    Leir::UILabel* m_VersionLabel = nullptr;
    Leir::UILabel* m_BackendLabel = nullptr;
};
