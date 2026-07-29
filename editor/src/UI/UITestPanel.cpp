#include "UITestPanel.h"
#include <LeirEngine/Core/Transform.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

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

    m_PosTitle = makeTitle("Position");
    auto* posRow = makeRow();
    AddField(posRow, "X:", m_PosX);
    AddField(posRow, "Y:", m_PosY);
    AddField(posRow, "Z:", m_PosZ);

    m_RotTitle = makeTitle("Rotation");
    auto* rotRow = makeRow();
    AddField(rotRow, "X:", m_RotX);
    AddField(rotRow, "Y:", m_RotY);
    AddField(rotRow, "Z:", m_RotZ);

    m_ScaleTitle = makeTitle("Scale");
    auto* scaleRow = makeRow();
    AddField(scaleRow, "X:", m_ScaleX);
    AddField(scaleRow, "Y:", m_ScaleY);
    AddField(scaleRow, "Z:", m_ScaleZ);

    m_CamTitle = makeTitle("Camera");
    auto* camRow = makeRow();
    AddField(camRow, "X:", m_CamPosX);
    AddField(camRow, "Y:", m_CamPosY);
    AddField(camRow, "Z:", m_CamPosZ);
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

void UITestPanel::AddField(Leir::UIPanel* parent, const std::string& labelText, UIDragFloatInput*& outInput)
{
    auto* field = new UIDragFloatInput();
    field->SetLabel(labelText);
    field->SetValue(0.0f);
    field->SetSizePolicy(Leir::SizePolicy::Fill);
    parent->AddChild(field);
    outInput = field;
}

void UITestPanel::SetTargetObject(Leir::Object3D* obj)
{
    m_Target = obj;
}

void UITestPanel::SetCameraObject(Leir::Object3D* cam)
{
    m_Camera = cam;
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

    if (m_Camera) {
        auto& t = m_Camera->GetTransform();
        auto pos = t.GetLocalPosition();
        m_CamPosX->SetValue(pos.x);
        m_CamPosY->SetValue(pos.y);
        m_CamPosZ->SetValue(pos.z);
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
