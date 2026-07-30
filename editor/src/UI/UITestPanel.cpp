#include "UITestPanel.h"
#include <LeirEngine/Core/Transform.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <functional>

UITestPanel::UITestPanel()
{
    SetName("TestPanel");
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

    m_TitleLabel = makeTitle("-- Properties --");

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
    m_RotTitle = makeTitle("Rotation");
    auto* rotRow = makeRow();
    AddField(rotRow, "X:", m_RotX, [this](float v) {
        if (!m_Target) return;
        auto euler = glm::degrees(glm::eulerAngles(m_Target->GetTransform().GetLocalRotation())); euler.x = v;
        m_Target->GetTransform().SetLocalRotation(glm::quat(glm::radians(euler)));
    });
    AddField(rotRow, "Y:", m_RotY, [this](float v) {
        if (!m_Target) return;
        auto euler = glm::degrees(glm::eulerAngles(m_Target->GetTransform().GetLocalRotation())); euler.y = v;
        m_Target->GetTransform().SetLocalRotation(glm::quat(glm::radians(euler)));
    });
    AddField(rotRow, "Z:", m_RotZ, [this](float v) {
        if (!m_Target) return;
        auto euler = glm::degrees(glm::eulerAngles(m_Target->GetTransform().GetLocalRotation())); euler.z = v;
        m_Target->GetTransform().SetLocalRotation(glm::quat(glm::radians(euler)));
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

UITestPanel::~UITestPanel() = default;

void UITestPanel::SetFont(Leir::Font* font)
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

void UITestPanel::AddField(Leir::UIPanel* parent, const std::string& labelText, UIDragFloatInput*& outInput, std::function<void(float)> onChanged)
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

void UITestPanel::SetTargetObject(Leir::Object3D* obj)
{
    m_Target = obj;
}

void UITestPanel::Refresh()
{
    if (m_Target) {
        auto& t = m_Target->GetTransform();
        auto pos = t.GetLocalPosition();
        m_PosX->SetValue(pos.x);
        m_PosY->SetValue(pos.y);
        m_PosZ->SetValue(pos.z);

        auto euler = glm::degrees(glm::eulerAngles(t.GetLocalRotation()));
        m_RotX->SetValue(euler.x);
        m_RotY->SetValue(euler.y);
        m_RotZ->SetValue(euler.z);

        auto scale = t.GetLocalScale();
        m_ScaleX->SetValue(scale.x);
        m_ScaleY->SetValue(scale.y);
        m_ScaleZ->SetValue(scale.z);
    }
}

glm::vec2 UITestPanel::GetMinSize() const
{
    return {280.0f, 220.0f};
}

void UITestPanel::OnLayoutComputed()
{
    UIPanel::OnLayoutComputed();
}
