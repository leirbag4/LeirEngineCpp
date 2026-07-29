#pragma once
#include <LeirEngine/UI/UIPanel.h>
#include <LeirEngine/UI/UILabel.h>
#include <LeirEngine/UI/UIFloatInput.h>
#include <LeirEngine/UI/UICanvas.h>
#include <functional>

class UIDragFloatInput : public Leir::UIPanel {
public:
    UIDragFloatInput();
    ~UIDragFloatInput() override;

    void SetLabel(const std::string& text);
    void SetValue(float v);
    float GetValue() const;

    void SetOnValueChanged(std::function<void(float)> cb) { m_OnValueChanged = cb; }

    bool OnPointerDown(const glm::vec2& pos) override;
    void OnPointerMove(const glm::vec2& pos) override;
    bool OnPointerUp(const glm::vec2& pos) override;

    glm::vec2 GetMinSize() const override;

    Leir::UIFloatInput* GetInput() const { return m_Input; }
    Leir::UILabel* GetLabel() const { return m_Label; }
    void SetFont(Leir::Font* font);

protected:
    void OnLayoutComputed() override;

private:
    Leir::UILabel* m_Label = nullptr;
    Leir::UIFloatInput* m_Input = nullptr;

    bool m_Dragging = false;
    float m_DragStartX = 0.0f;
    float m_DragStartValue = 0.0f;

    std::function<void(float)> m_OnValueChanged;
};
