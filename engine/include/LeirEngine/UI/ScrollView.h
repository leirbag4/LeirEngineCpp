#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <glm/glm.hpp>

namespace Leir {

class LEIR_API ScrollView : public UIElement {
public:
    ScrollView();
    ~ScrollView() override;

    void SetScrollOffset(const glm::vec2& offset) { m_ScrollOffset = offset; }
    const glm::vec2& GetScrollOffset() const { return m_ScrollOffset; }

    void SetContent(UIElement* content);
    UIElement* GetContent() const { return m_Content; }

    bool OnPointerDown(const glm::vec2& pos) override;
    bool OnPointerUp(const glm::vec2& pos) override;
    void OnPointerMove(const glm::vec2& pos) override;

protected:
    void OnLayoutComputed() override;

private:
    glm::vec2 m_ScrollOffset = {0.0f, 0.0f};
    UIElement* m_Content = nullptr;
    bool m_Dragging = false;
    glm::vec2 m_DragStart = {0.0f, 0.0f};
    glm::vec2 m_ScrollStart = {0.0f, 0.0f};
};

} // namespace Leir
