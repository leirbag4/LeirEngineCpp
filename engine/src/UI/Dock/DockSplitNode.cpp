#include "LeirEngine/UI/Dock/DockSplitNode.h"
#include "LeirEngine/UI/Dock/DockSplitter.h"
#include <algorithm>

namespace Leir {

DockSplitNode::DockSplitNode(DockOrientation orientation)
    : DockNode(DockNodeType::Split)
    , m_Orientation(orientation)
{
    SetName("DockSplit");
    SetColor({0.0f, 0.0f, 0.0f, 0.0f});
}

DockSplitNode::~DockSplitNode()
{
    // Delete all UI children (child nodes + splitters). The base dtor only
    // nulls parent pointers, so we remove+delete explicitly to avoid dangling
    // pointers when the base destructor iterates m_Children.
    auto children = GetChildren();
    for (auto* c : children) {
        RemoveChild(c);
        delete c;
    }
}

float DockSplitNode::GetRatioForChild(DockNode* child) const
{
    for (size_t i = 0; i < m_NodeChildren.size(); ++i)
        if (m_NodeChildren[i] == child)
            return m_Ratios[i];
    return 0.5f;
}

void DockSplitNode::AddNode(DockNode* child, float ratio)
{
    m_NodeChildren.push_back(child);
    m_Ratios.push_back(ratio);
    AddChild(child);
    RebuildSplitters();
}

void DockSplitNode::RemoveNode(DockNode* child)
{
    auto it = std::find(m_NodeChildren.begin(), m_NodeChildren.end(), child);
    if (it == m_NodeChildren.end())
        return;
    size_t idx = it - m_NodeChildren.begin();
    m_NodeChildren.erase(it);
    m_Ratios.erase(m_Ratios.begin() + idx);
    RemoveChild(child);
    RebuildSplitters();
    NormalizeRatios();
}

void DockSplitNode::ReplaceChild(DockNode* oldNode, DockNode* newNode, float ratio)
{
    auto it = std::find(m_NodeChildren.begin(), m_NodeChildren.end(), oldNode);
    if (it == m_NodeChildren.end())
        return;
    size_t idx = it - m_NodeChildren.begin();
    *it = newNode;
    m_Ratios[idx] = ratio;
    RemoveChild(oldNode);
    AddChild(newNode);
    RebuildSplitters();
    NormalizeRatios();
}

void DockSplitNode::DragSplitter(size_t index, float pixelDelta)
{
    if (index >= m_Splitters.size() || index + 1 >= m_NodeChildren.size())
        return;

    const bool horiz = (m_Orientation == DockOrientation::Horizontal);
    const float innerLen = horiz
        ? m_ComputedRect.z - m_Padding[0] - m_Padding[2]
        : m_ComputedRect.w - m_Padding[1] - m_Padding[3];
    const float splitterW = GetSplitterWidth();
    const float usable = std::max(1.0f, innerLen - (float)(m_NodeChildren.size() - 1) * splitterW);

    const float delta = pixelDelta / usable;

    const size_t n = m_NodeChildren.size();
    float a = m_Ratios[index];
    float b = m_Ratios[index + 1];
    float others = 0.0f;
    for (size_t j = 0; j < n; ++j)
        if (j != index && j != index + 1)
            others += m_Ratios[j];

    const float minFrac = 0.05f;
    float newA = a + delta;
    newA = std::clamp(newA, minFrac, std::max(minFrac, 1.0f - others - minFrac));

    m_Ratios[index] = newA;
    m_Ratios[index + 1] = std::max(minFrac, (a + b) - newA);
    NormalizeRatios();
}

Vector2 DockSplitNode::GetMinSize() const
{
    const bool horiz = (m_Orientation == DockOrientation::Horizontal);
    float main = 0.0f;
    float cross = 0.0f;
    for (size_t i = 0; i < m_NodeChildren.size(); ++i) {
        Vector2 m = m_NodeChildren[i]->GetMinSize();
        if (horiz) {
            main += m.x;
            cross = std::max(cross, m.y);
        } else {
            main += m.y;
            cross = std::max(cross, m.x);
        }
    }
    if (m_NodeChildren.size() > 1)
        main += (float)(m_NodeChildren.size() - 1) * GetSplitterWidth();
    return horiz ? Vector2{main, cross} : Vector2{cross, main};
}

void DockSplitNode::ComputeLayout(const Vector2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);

    const size_t n = m_NodeChildren.size();
    if (n == 0)
        return;

    const float innerX = m_ComputedRect.x + m_Padding[0];
    const float innerY = m_ComputedRect.y + m_Padding[1];
    const float innerW = m_ComputedRect.z - m_Padding[0] - m_Padding[2];
    const float innerH = m_ComputedRect.w - m_Padding[1] - m_Padding[3];

    const bool horiz = (m_Orientation == DockOrientation::Horizontal);
    const float splitterW = GetSplitterWidth();
    const float contentLen = horiz ? innerW : innerH;
    const float usable = std::max(0.0f, contentLen - (float)(n - 1) * splitterW);

    float cursor = horiz ? innerX : innerY;
    for (size_t i = 0; i < n; ++i) {
        const float len = m_Ratios[i] * usable;
        DockNode* child = m_NodeChildren[i];

        if (horiz) {
            child->GetRect().anchor = AnchorSet::TopLeft();
            child->GetRect().offset = {cursor, innerY, cursor + len, innerY + innerH};
            child->ComputeLayout({len, innerH});
        } else {
            child->GetRect().anchor = AnchorSet::TopLeft();
            child->GetRect().offset = {innerX, cursor, innerX + innerW, cursor + len};
            child->ComputeLayout({innerW, len});
        }
        cursor += len;

        if (i + 1 < n) {
            DockSplitter* sp = m_Splitters[i];
            if (horiz) {
                sp->GetRect().anchor = AnchorSet::TopLeft();
                sp->GetRect().offset = {cursor, innerY, cursor + splitterW, innerY + innerH};
                sp->ComputeLayout({splitterW, innerH});
            } else {
                sp->GetRect().anchor = AnchorSet::TopLeft();
                sp->GetRect().offset = {innerX, cursor, innerX + innerW, cursor + splitterW};
                sp->ComputeLayout({innerW, splitterW});
            }
            cursor += splitterW;
        }
    }
}

void DockSplitNode::RebuildSplitters()
{
    std::vector<UIElement*> oldSplitters;
    for (auto* c : GetChildren())
        if (auto* sp = dynamic_cast<DockSplitter*>(c))
            oldSplitters.push_back(sp);
    for (auto* sp : oldSplitters) {
        RemoveChild(sp);
        delete sp;
    }
    m_Splitters.clear();

    const size_t n = m_NodeChildren.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        auto* sp = new DockSplitter();
        sp->Configure(this, i);
        AddChild(sp);
        m_Splitters.push_back(sp);
    }
}

void DockSplitNode::NormalizeRatios()
{
    if (m_Ratios.empty())
        return;
    float sum = 0.0f;
    for (float r : m_Ratios)
        sum += r;
    if (sum <= 0.0f) {
        const float equal = 1.0f / (float)m_Ratios.size();
        for (float& r : m_Ratios)
            r = equal;
        return;
    }
    for (float& r : m_Ratios)
        r /= sum;
}

} // namespace Leir
