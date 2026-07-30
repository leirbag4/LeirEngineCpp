#include "CameraTestPanel.h"
#include <LeirEngine/Core/Transform.h>

CameraTestPanel::CameraTestPanel()
{
    SetName("CameraTestPanel");
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

    m_TitleLabel = makeTitle("-- Camera --");

    // ---- Position ----
    m_PosTitle = makeTitle("Position");
    auto* posRow = makeRow();
    AddField(posRow, "X:", m_PosX, [this](float v) {
        if (!m_Camera) return;
        auto p = m_Camera->GetTransform().GetLocalPosition(); p.x = v;
        m_Camera->GetTransform().SetLocalPosition(p);
    });
    AddField(posRow, "Y:", m_PosY, [this](float v) {
        if (!m_Camera) return;
        auto p = m_Camera->GetTransform().GetLocalPosition(); p.y = v;
        m_Camera->GetTransform().SetLocalPosition(p);
    });
    AddField(posRow, "Z:", m_PosZ, [this](float v) {
        if (!m_Camera) return;
        auto p = m_Camera->GetTransform().GetLocalPosition(); p.z = v;
        m_Camera->GetTransform().SetLocalPosition(p);
    });

    // ---- Rotation (Euler degrees) ----
    m_RotTitle = makeTitle("Rotation");
    auto* rotRow = makeRow();
    AddField(rotRow, "X:", m_RotX, [this](float v) {
        if (!m_Camera) return;
        auto euler = Leir::Quaternion::ToEuler(m_Camera->GetTransform().GetLocalRotation()); euler.x = v;
        m_Camera->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(euler.x, euler.y, euler.z));
    });
    AddField(rotRow, "Y:", m_RotY, [this](float v) {
        if (!m_Camera) return;
        auto euler = Leir::Quaternion::ToEuler(m_Camera->GetTransform().GetLocalRotation()); euler.y = v;
        m_Camera->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(euler.x, euler.y, euler.z));
    });
    AddField(rotRow, "Z:", m_RotZ, [this](float v) {
        if (!m_Camera) return;
        auto euler = Leir::Quaternion::ToEuler(m_Camera->GetTransform().GetLocalRotation()); euler.z = v;
        m_Camera->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(euler.x, euler.y, euler.z));
    });
}

CameraTestPanel::~CameraTestPanel() = default;

void CameraTestPanel::SetFont(Leir::Font* font)
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

void CameraTestPanel::SetCameraObject(Leir::Object3D* cam)
{
    m_Camera = cam;
}

void CameraTestPanel::Refresh()
{
    if (!m_Camera) return;

    auto& t = m_Camera->GetTransform();
    auto pos = t.GetLocalPosition();
    m_PosX->SetValue(pos.x);
    m_PosY->SetValue(pos.y);
    m_PosZ->SetValue(pos.z);

    auto euler = Leir::Quaternion::ToEuler(t.GetLocalRotation());
    m_RotX->SetValue(euler.x);
    m_RotY->SetValue(euler.y);
    m_RotZ->SetValue(euler.z);
}

void CameraTestPanel::AddField(Leir::UIPanel* parent, const std::string& labelText, UIDragFloatInput*& outInput, std::function<void(float)> onChanged)
{
    auto* field = new UIDragFloatInput();
    field->SetLabel(labelText);
    field->SetValue(0.0f);
    field->SetSizePolicy(Leir::SizePolicy::Fill);
    if (onChanged)
        field->SetOnValueChanged(onChanged);
    parent->AddChild(field);
    outInput = field;
}

Leir::Vector2 CameraTestPanel::GetMinSize() const
{
    return {280.0f, 160.0f};
}

void CameraTestPanel::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}
