#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <functional>

namespace Leir {

class LEIR_API UISlider : public UIElement {
public:
    UISlider();
    ~UISlider() override;

    void SetRange(float min, float max) { m_Min = min; m_Max = max; }
    float GetMin() const { return m_Min; }
    float GetMax() const { return m_Max; }

    void SetValue(float value);
    float GetValue() const { return m_Value; }

    void SetOnChange(std::function<void(float)> cb) { m_OnChange = cb; }

    glm::vec2 GetMinSize() const override;

    void OnPointerEnter(const glm::vec2& pos) override;
    void OnPointerExit() override;
    bool OnPointerDown(const glm::vec2& pos) override;
    bool OnPointerUp(const glm::vec2& pos) override;
    void OnPointerMove(const glm::vec2& pos) override;

    bool IsDragging() const { return m_Dragging; }
    bool IsHovered() const { return m_Hovered; }
    float HandlePos() const;
    float ValueFromPos(float x) const;

private:
    float m_Min = 0.0f;
    float m_Max = 1.0f;
    float m_Value = 0.5f;
    std::function<void(float)> m_OnChange;
    bool m_Dragging = false;
    bool m_Hovered = false;
};

} // namespace Leir
