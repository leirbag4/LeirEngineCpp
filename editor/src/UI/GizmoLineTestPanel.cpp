#include "GizmoLineTestPanel.h"
#include <algorithm>

GizmoLineTestPanel::GizmoLineTestPanel()
{
    SetName("GizmoLineTestPanel");
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

    makeTitle("-- Gizmo Line --");

    auto* colorRow = makeRow();
    AddField(colorRow, "R:", m_R, m_RVal,
        [this](float v) { m_RVal = std::clamp(v, 0.0f, 1.0f); });
    AddField(colorRow, "G:", m_G, m_GVal,
        [this](float v) { m_GVal = std::clamp(v, 0.0f, 1.0f); });
    AddField(colorRow, "B:", m_B, m_BVal,
        [this](float v) { m_BVal = std::clamp(v, 0.0f, 1.0f); });

    auto* alphaRow = makeRow();
    AddField(alphaRow, "Alpha:", m_A, m_AVal,
        [this](float v) { m_AVal = std::clamp(v, 0.0f, 1.0f); });

    auto* widthRow = makeRow();
    AddField(widthRow, "Width:", m_Width, m_WidthVal,
        [this](float v) { m_WidthVal = std::max(0.0f, v); });
}

GizmoLineTestPanel::~GizmoLineTestPanel() = default;

void GizmoLineTestPanel::SetFont(Leir::Font* font)
{
    for (auto* child : GetChildren()) {
        if (auto* label = dynamic_cast<Leir::UILabel*>(child)) {
            label->SetFont(font);
        } else if (auto* panel = dynamic_cast<Leir::UIPanel*>(child)) {
            for (auto* sub : panel->GetChildren()) {
                if (auto* dfi = dynamic_cast<UIDragFloatInput*>(sub))
                    dfi->SetFont(font);
            }
        }
    }
}

void GizmoLineTestPanel::AddField(Leir::UIPanel* parent,
                                  const std::string& labelText,
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

Leir::Vector2 GizmoLineTestPanel::GetMinSize() const
{
    return {280.0f, 175.0f};
}

void GizmoLineTestPanel::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}