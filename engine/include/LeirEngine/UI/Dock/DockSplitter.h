#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class DockSplitNode;

// Ratio-based splitter bar between two children of a DockSplitNode. Dragging
// adjusts the parent's ratios[] (instead of an absolute pixel width).
class LEIR_API DockSplitter : public UIPanel {
public:
    DockSplitter();
    ~DockSplitter() override;

    void Configure(DockSplitNode* node, size_t index);

    bool OnPointerDown(const Vector2& pos) override;
    void OnPointerMove(const Vector2& pos) override;
    bool OnPointerUp(const Vector2& pos) override;
    void OnPointerEnter(const Vector2& pos) override;
    void OnPointerExit() override;

    Vector2 GetMinSize() const override;

private:
    void ApplyCursor();

    DockSplitNode* m_Node = nullptr;
    size_t m_Index = 0;
    bool m_Dragging = false;
    Vector2 m_DragStart = {0.0f, 0.0f};
};

} // namespace Leir
