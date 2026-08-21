#include "GridPanel.h"

#include <algorithm>

GridPanel::GridPanel()
{
    SetName("GridPanel");
    SetColor({0.08f, 0.08f, 0.10f, 0.85f});
    SetPadding(6.0f, 6.0f, 6.0f, 6.0f);
    SetLayoutMode(Leir::LayoutMode::Column);
    SetSpacing(3.0f);

    auto makeTitle = [&](const std::string& text) -> Leir::UILabel* {
        auto* lbl = new Leir::UILabel();
        lbl->SetText(text);
        lbl->SetFontSize(11);
        lbl->SetColor({0.6f, 0.6f, 0.6f, 1.0f});
        lbl->SetSizePolicy(Leir::SizePolicy::Fixed);
        AddChild(lbl);
        return lbl;
    };

    auto makeRow = [&]() -> Leir::UIPanel* {
        auto* row = new Leir::UIPanel();
        row->SetColor({0, 0, 0, 0});
        row->SetLayoutMode(Leir::LayoutMode::Row);
        row->SetSpacing(4.0f);
        row->SetSizePolicy(Leir::SizePolicy::Fill);
        AddChild(row);
        return row;
    };

    makeTitle("-- Grid LOD --");

    // Manual/auto toggle: ON = inputs drive the grid; OFF = inputs greyed out,
    // grid uses defaults + an auto horizon fade from the camera height.
    m_ManualButton = new Leir::UIButton();
    m_ManualButton->SetSizePolicy(Leir::SizePolicy::Fixed);
    m_ManualButton->SetOnClick([this]() { SetManual(!m_Manual); });
    AddChild(m_ManualButton);

    auto* densityRow = makeRow();
    AddField(densityRow, "px/unit:", m_Density, m_DensityVal,
        [this](float v) { if (m_Manual) m_DensityVal = v; });

    auto* fadeRow = makeRow();
    AddField(fadeRow, "fadeStart:", m_FadeStart, m_FadeStartVal,
        [this](float v) { if (m_Manual) m_FadeStartVal = v; });
    AddField(fadeRow, "fadeEnd:", m_FadeEnd, m_FadeEndVal,
        [this](float v) { if (m_Manual) m_FadeEndVal = v; });

    auto* thickRow = makeRow();
    AddField(thickRow, "thickWidth:", m_ThickWidth, m_ChunkWidthVal,
        [this](float v) { if (m_Manual) m_ChunkWidthVal = v; });

    auto* horizonRow = makeRow();
    AddField(horizonRow, "horizonStart:", m_HorizonStart, m_HorizonStartVal,
        [this](float v) { if (m_Manual) m_HorizonStartVal = v; });
    AddField(horizonRow, "horizonEnd:", m_HorizonEnd, m_HorizonEndVal,
        [this](float v) { if (m_Manual) m_HorizonEndVal = v; });

    UpdateEnabledVisuals();
}

GridPanel::~GridPanel() = default;

void GridPanel::SetFont(Leir::Font* font)
{
    for (auto* child : GetChildren()) {
        if (auto* lbl = dynamic_cast<Leir::UILabel*>(child)) {
            lbl->SetFont(font);
        } else if (auto* btn = dynamic_cast<Leir::UIButton*>(child)) {
            btn->SetFont(font);
        } else if (auto* panel = dynamic_cast<Leir::UIPanel*>(child)) {
            for (auto* sub : panel->GetChildren()) {
                if (auto* dfi = dynamic_cast<UIDragFloatInput*>(sub))
                    dfi->SetFont(font);
            }
        }
    }
}

void GridPanel::SetManual(bool manual)
{
    if (m_Manual == manual)
        return;
    m_Manual = manual;
    // Restore the stored manual values into the widgets when going back to
    // manual (auto had overwritten the display with the auto/default values).
    if (m_Manual) {
        m_Density->SetValue(m_DensityVal);
        m_FadeStart->SetValue(m_FadeStartVal);
        m_FadeEnd->SetValue(m_FadeEndVal);
        m_ThickWidth->SetValue(m_ChunkWidthVal);
        m_HorizonStart->SetValue(m_HorizonStartVal);
        m_HorizonEnd->SetValue(m_HorizonEndVal);
    }
    UpdateEnabledVisuals();
}

void GridPanel::SetAutoValues(float fadeStartPx, float fadeEndPx, float chunkWidth,
                              float density, float horizonStart, float horizonEnd)
{
    // Display-only: programmatic SetValue does not fire the onChanged callbacks,
    // so the stored manual values are preserved.
    m_FadeStart->SetValue(fadeStartPx);
    m_FadeEnd->SetValue(fadeEndPx);
    m_ThickWidth->SetValue(chunkWidth);
    m_Density->SetValue(density);
    m_HorizonStart->SetValue(horizonStart);
    m_HorizonEnd->SetValue(horizonEnd);
}

void GridPanel::ComputeAutoHorizon(float camH, float& outStart, float& outEnd)
{
    // User-tuned breakpoints (camera height -> horizonStart : horizonEnd), in
    // view depth (world units). Piecewise-linear; clamped at the extremes. The
    // far plane is 2000, so end caps there.
    struct Pt { float h, start, end; };
    static const Pt pts[] = {
        { 30.0f, 100.0f, 400.0f },
        { 100.0f, 200.0f, 800.0f },
        { 200.0f, 400.0f, 1400.0f },
        { 300.0f, 600.0f, 1800.0f },
        { 500.0f, 800.0f, 2000.0f },
        { 1000.0f, 900.0f, 2000.0f },
        { 2000.0f, 1000.0f, 2000.0f },
    };
    const int n = (int)(sizeof(pts) / sizeof(pts[0]));
    const float h = std::clamp(camH, pts[0].h, pts[n - 1].h);
    for (int i = 0; i + 1 < n; ++i) {
        if (h <= pts[i + 1].h) {
            const float t = (h - pts[i].h) / (pts[i + 1].h - pts[i].h);
            outStart = pts[i].start + t * (pts[i + 1].start - pts[i].start);
            outEnd = pts[i].end + t * (pts[i + 1].end - pts[i].end);
            return;
        }
    }
    outStart = pts[n - 1].start;
    outEnd = pts[n - 1].end;
}

void GridPanel::AddField(Leir::UIPanel* parent, const std::string& labelText,
                         UIDragFloatInput*& outInput, float initial,
                         std::function<void(float)> onChanged)
{
    auto* field = new UIDragFloatInput();
    field->SetLabel(labelText);
    field->SetValue(initial);
    field->SetSizePolicy(Leir::SizePolicy::Fill);
    if (onChanged)
        field->SetOnValueChanged(onChanged);
    parent->AddChild(field);
    outInput = field;
}

Leir::Vector2 GridPanel::GetMinSize() const
{
    return {280.0f, 200.0f};
}

void GridPanel::UpdateEnabledVisuals()
{
    if (m_ManualButton) {
        m_ManualButton->SetText(m_Manual ? "Manual: ON" : "Manual: OFF");
        m_ManualButton->SetTextColor(
            m_Manual ? Leir::Vector4(1.0f, 1.0f, 1.0f, 1.0f)
                     : Leir::Vector4(0.7f, 0.7f, 0.7f, 1.0f));
        if (m_Manual) {
            m_ManualButton->SetColors(
                {0.2f, 0.45f, 0.25f, 1.0f}, {0.3f, 0.6f, 0.35f, 1.0f}, {0.12f, 0.3f, 0.18f, 1.0f});
        } else {
            m_ManualButton->SetColors(
                {0.45f, 0.35f, 0.25f, 1.0f}, {0.6f, 0.45f, 0.3f, 1.0f}, {0.3f, 0.2f, 0.15f, 1.0f});
        }
    }

    // Grey out (and make read-only) the inputs when auto.
    const Leir::Vector4 labelColor =
        m_Manual ? Leir::Vector4(0.8f, 0.8f, 0.8f, 1.0f) : Leir::Vector4(0.42f, 0.42f, 0.42f, 1.0f);
    const Leir::Vector4 textColor =
        m_Manual ? Leir::Vector4(1.0f, 1.0f, 1.0f, 1.0f) : Leir::Vector4(0.45f, 0.45f, 0.45f, 1.0f);

    UIDragFloatInput* fields[] = {
        m_Density, m_FadeStart, m_FadeEnd, m_ThickWidth, m_HorizonStart, m_HorizonEnd,
    };
    for (auto* f : fields) {
        if (!f)
            continue;
        if (f->GetLabel())
            f->GetLabel()->SetColor(labelColor);
        if (f->GetInput()) {
            f->GetInput()->SetTextColor(textColor);
            f->GetInput()->SetEditable(m_Manual);
        }
    }
}