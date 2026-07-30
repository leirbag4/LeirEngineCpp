#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class LEIR_API ScrollView : public UIElement {
public:
    ScrollView();
    ~ScrollView() override;

    void SetScrollOffset(const Vector2& offset) { m_ScrollOffset = offset; }
    const Vector2& GetScrollOffset() const { return m_ScrollOffset; }

    void SetContent(UIElement* content);
    UIElement* GetContent() const { return m_Content; }

    bool OnPointerDown(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;

protected:
    void OnLayoutComputed() override;

private:
    Vector2 m_ScrollOffset = {0.0f, 0.0f};
    UIElement* m_Content = nullptr;
    bool m_Dragging = false;
    Vector2 m_DragStart = {0.0f, 0.0f};
    Vector2 m_ScrollStart = {0.0f, 0.0f};
};

} // namespace Leir
