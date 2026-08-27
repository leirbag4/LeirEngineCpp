#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/Dock/DockNode.h"
#include "LeirEngine/Math/Vector2.h"
#include <vector>

namespace Leir {

class DockSplitter;

enum class LEIR_API DockOrientation {
    Horizontal,   // children side by side (vertical splitter bar, ResizeEW)
    Vertical,     // children stacked (horizontal splitter bar, ResizeNS)
};

// A split container: positions its child nodes by ratios with 6px splitters
// between them. Requires UIElement::ComputeLayout to be virtual (ratios differ
// from the standard "Fill" split, which shares space equally).
class LEIR_API DockSplitNode : public DockNode {
public:
    explicit DockSplitNode(DockOrientation orientation);
    ~DockSplitNode() override;

    DockOrientation GetOrientation() const { return m_Orientation; }

    size_t GetNodeCount() const { return m_NodeChildren.size(); }
    DockNode* GetNode(size_t i) const { return m_NodeChildren[i]; }
    const std::vector<float>& GetRatios() const { return m_Ratios; }
    float GetRatioForChild(DockNode* child) const;

    void AddNode(DockNode* child, float ratio);
    void RemoveNode(DockNode* child);
    void ReplaceChild(DockNode* oldNode, DockNode* newNode, float ratio);

    // Adjust the two ratios around splitter `index` by pixelDelta (px). The
    // delta is measured from the ratio snapshot taken by BeginSplitterDrag,
    // so the splitter tracks the mouse exactly (no cumulative drift).
    void BeginSplitterDrag(size_t index);
    void DragSplitter(size_t index, float pixelDelta);

    float GetSplitterWidth() const { return 6.0f; }

    Vector2 GetMinSize() const override;
    void ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset = Vector2(0.0f, 0.0f)) override;

private:
    void RebuildSplitters();
    void NormalizeRatios();

    DockOrientation m_Orientation = DockOrientation::Horizontal;
    std::vector<DockNode*> m_NodeChildren;
    std::vector<float> m_Ratios;
    std::vector<DockSplitter*> m_Splitters;
    float m_DragStartA = 0.0f;
    float m_DragStartB = 0.0f;
};

} // namespace Leir
