#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIFloatInput.h>
#include <LeirEngine/UI/Font.h>
#include <LeirEngine/Scene/Scene.h>
#include <LeirEngine/Objects/Object3D.h>
#include "UIDragFloatInput.h"
#include <vector>

class UITestPanel : public Leir::UIPanel {
public:
    UITestPanel();
    ~UITestPanel() override;

    void SetFont(Leir::Font* font);
    void SetTargetObject(Leir::Object3D* obj);
    void SetCameraObject(Leir::Object3D* cam);
    void Refresh();

    glm::vec2 GetMinSize() const override;

protected:
    void OnLayoutComputed() override;

private:
    void AddField(Leir::UIPanel* parent, const std::string& labelText, UIDragFloatInput*& outInput);

    Leir::Object3D* m_Target = nullptr;
    Leir::Object3D* m_Camera = nullptr;

    Leir::UILabel* m_TitleLabel = nullptr;
    Leir::UILabel* m_PosTitle = nullptr;
    UIDragFloatInput* m_PosX = nullptr;
    UIDragFloatInput* m_PosY = nullptr;
    UIDragFloatInput* m_PosZ = nullptr;

    Leir::UILabel* m_RotTitle = nullptr;
    UIDragFloatInput* m_RotX = nullptr;
    UIDragFloatInput* m_RotY = nullptr;
    UIDragFloatInput* m_RotZ = nullptr;

    Leir::UILabel* m_ScaleTitle = nullptr;
    UIDragFloatInput* m_ScaleX = nullptr;
    UIDragFloatInput* m_ScaleY = nullptr;
    UIDragFloatInput* m_ScaleZ = nullptr;

    Leir::UILabel* m_CamTitle = nullptr;
    UIDragFloatInput* m_CamPosX = nullptr;
    UIDragFloatInput* m_CamPosY = nullptr;
    UIDragFloatInput* m_CamPosZ = nullptr;
};
