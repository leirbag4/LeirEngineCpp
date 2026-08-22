#include "InspectorTransformPanel.h"
#include <LeirEngine/Core/Transform.h>
#include <functional>
#include <cmath>

InspectorTransformPanel::InspectorTransformPanel()
{
    SetName("InspectorTransformPanel");
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
        row->SetSizePolicy(Leir::SizePolicy::Content);
        AddChild(row);
        return row;
    };

    m_TitleLabel = makeTitle("Transform");

    // ---- Position ----
    m_PosTitle = makeTitle("Position");
    auto* posRow = makeRow();
    AddField(posRow, "X:", m_PosX, [this](float v) {
        if (!m_Target) return;
        auto p = m_Target->GetTransform().GetLocalPosition(); p.x = v;
        m_Target->GetTransform().SetLocalPosition(p);
    });
    AddField(posRow, "Y:", m_PosY, [this](float v) {
        if (!m_Target) return;
        auto p = m_Target->GetTransform().GetLocalPosition(); p.y = v;
        m_Target->GetTransform().SetLocalPosition(p);
    });
    AddField(posRow, "Z:", m_PosZ, [this](float v) {
        if (!m_Target) return;
        auto p = m_Target->GetTransform().GetLocalPosition(); p.z = v;
        m_Target->GetTransform().SetLocalPosition(p);
    });

    // ---- Rotation (Euler degrees) ----
    // Editing a field changes only that axis. The other two keep the last
    // displayed values (m_RotEuler cache) — they are NOT re-derived from the
    // live quaternion, which would inject an Euler alias (Euler->quat->ToEuler
    // is not an exact inverse) and make the numbers drift on every edit.
    m_RotTitle = makeTitle("Rotation");
    auto* rotRow = makeRow();
    AddField(rotRow, "X:", m_RotX, [this](float v) {
        if (!m_Target) return;
        m_RotEuler.x = v;
        m_Target->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(m_RotEuler));
    });
    AddField(rotRow, "Y:", m_RotY, [this](float v) {
        if (!m_Target) return;
        m_RotEuler.y = v;
        m_Target->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(m_RotEuler));
    });
    AddField(rotRow, "Z:", m_RotZ, [this](float v) {
        if (!m_Target) return;
        m_RotEuler.z = v;
        m_Target->GetTransform().SetLocalRotation(Leir::Quaternion::Euler(m_RotEuler));
    });

    // ---- Scale ----
    m_ScaleTitle = makeTitle("Scale");
    auto* scaleRow = makeRow();
    AddField(scaleRow, "X:", m_ScaleX, [this](float v) {
        if (!m_Target) return;
        auto s = m_Target->GetTransform().GetLocalScale(); s.x = v;
        m_Target->GetTransform().SetLocalScale(s);
    });
    AddField(scaleRow, "Y:", m_ScaleY, [this](float v) {
        if (!m_Target) return;
        auto s = m_Target->GetTransform().GetLocalScale(); s.y = v;
        m_Target->GetTransform().SetLocalScale(s);
    });
    AddField(scaleRow, "Z:", m_ScaleZ, [this](float v) {
        if (!m_Target) return;
        auto s = m_Target->GetTransform().GetLocalScale(); s.z = v;
        m_Target->GetTransform().SetLocalScale(s);
    });

}

InspectorTransformPanel::~InspectorTransformPanel() = default;

void InspectorTransformPanel::SetFont(Leir::Font* font)
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

void InspectorTransformPanel::AddField(Leir::UIPanel* parent, const std::string& labelText, UIDragFloatInput*& outInput, std::function<void(float)> onChanged)
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

void InspectorTransformPanel::SetTargetObject(Leir::Object3D* obj)
{
    m_Target = obj;
    if (m_Target)
        m_RotEuler = Leir::Quaternion::ToEuler(m_Target->GetTransform().GetLocalRotation(), m_RotEuler);
}

void InspectorTransformPanel::Refresh()
{
    if (m_Target) {
        auto& t = m_Target->GetTransform();
        auto pos = t.GetLocalPosition();
        m_PosX->SetValue(pos.x);
        m_PosY->SetValue(pos.y);
        m_PosZ->SetValue(pos.z);

        // Rotation: if the transform's rotation still matches the last values we
        // wrote, keep showing them exactly (no alias round-trip). Otherwise the
        // rotation changed externally (gizmo drag, camera sync, code): re-sync
        // the cache picking the Euler branch closest to the previous display so
        // the numbers follow smoothly instead of jumping to a far-away branch.
        auto current = t.GetLocalRotation();
        auto expected = Leir::Quaternion::Euler(m_RotEuler);
        if (std::fabs(Leir::Quaternion::Dot(current, expected)) < 0.999999f) {
            m_RotEuler = Leir::Quaternion::ToEuler(current, m_RotEuler);
        }
        m_RotX->SetValue(m_RotEuler.x);
        m_RotY->SetValue(m_RotEuler.y);
        m_RotZ->SetValue(m_RotEuler.z);

        auto scale = t.GetLocalScale();
        m_ScaleX->SetValue(scale.x);
        m_ScaleY->SetValue(scale.y);
        m_ScaleZ->SetValue(scale.z);
    }
}

Leir::Vector2 InspectorTransformPanel::GetMinSize() const
{
    return {220.0f, 110.0f};
}

void InspectorTransformPanel::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}
