#include "LeirEngine/UI/UIElement.h"
#include <algorithm>

namespace Leir {

UIElement::UIElement() = default;
UIElement::~UIElement()
{
    for (auto* child : m_Children)
        child->m_Parent = nullptr;
}

void UIElement::SetPadding(float left, float top, float right, float bottom)
{
    m_Padding[0] = left;
    m_Padding[1] = top;
    m_Padding[2] = right;
    m_Padding[3] = bottom;
}

void UIElement::SetParent(UIElement* parent)
{
    if (m_Parent)
        m_Parent->RemoveChild(this);
    m_Parent = parent;
}

void UIElement::AddChild(UIElement* child)
{
    if (child->m_Parent)
        child->m_Parent->RemoveChild(child);
    m_Children.push_back(child);
    child->m_Parent = this;
}

void UIElement::InsertChildAt(UIElement* child, size_t index)
{
    if (child->m_Parent)
        child->m_Parent->RemoveChild(child);
    index = std::min(index, m_Children.size());
    m_Children.insert(m_Children.begin() + index, child);
    child->m_Parent = this;
}

void UIElement::RemoveChild(UIElement* child)
{
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        m_Children.erase(it);
        child->m_Parent = nullptr;
    }
}

Vector2 UIElement::GetMinSize() const
{
    if (m_MinSizeOverride.has_value())
        return *m_MinSizeOverride;
    return {0.0f, 0.0f};
}

Vector2 UIElement::GetNaturalSize() const
{
    switch (m_SizePolicy) {
        case SizePolicy::Content:
            return GetContentSize();
        case SizePolicy::Fixed:
        case SizePolicy::Fill:
        case SizePolicy::Grow:
        default:
            return GetMinSize();
    }
}

Vector2 UIElement::GetContentSize() const
{
    if (m_LayoutMode == LayoutMode::Free)
        return GetMinSize();

    bool isRow = (m_LayoutMode == LayoutMode::Row);
    float mainTotal = 0.0f;
    float crossMax = 0.0f;
    int childCount = 0;

    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;
        Vector2 natural = child->GetNaturalSize();
        if (isRow) {
            mainTotal += natural.x;
            crossMax = std::max(crossMax, natural.y);
        } else {
            mainTotal += natural.y;
            crossMax = std::max(crossMax, natural.x);
        }
        childCount++;
    }

    if (childCount > 1)
        mainTotal += m_Spacing * (childCount - 1);

    if (isRow)
        return {m_Padding[0] + mainTotal + m_Padding[2],
                m_Padding[1] + crossMax + m_Padding[3]};
    return {m_Padding[0] + crossMax + m_Padding[2],
            m_Padding[1] + mainTotal + m_Padding[3]};
}

void UIElement::ComputeLayout(const Vector2& availableSize)
{
    if (!m_Active)
        return;

    switch (m_LayoutMode) {
        case LayoutMode::Free:
            ComputeFreeLayout(availableSize);
            break;
        case LayoutMode::Row:
            ComputeRowLayout(availableSize);
            break;
        case LayoutMode::Column:
            ComputeColumnLayout(availableSize);
            break;
    }

    OnLayoutComputed();
}

void UIElement::ComputeFreeLayout(const Vector2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);

    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;
        Vector2 childSize = {
            m_ComputedRect.z - m_Padding[0] - m_Padding[2],
            m_ComputedRect.w - m_Padding[1] - m_Padding[3]
        };
        child->m_Rect.offset.left += m_ComputedRect.x;
        child->m_Rect.offset.top += m_ComputedRect.y;
        child->m_Rect.offset.right += m_ComputedRect.x;
        child->m_Rect.offset.bottom += m_ComputedRect.y;
        child->ComputeLayout(childSize);
    }
}

void UIElement::ComputeRowLayout(const Vector2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);

    float innerX = m_Padding[0];
    float innerY = m_Padding[1];
    float innerW = m_ComputedRect.z - m_Padding[0] - m_Padding[2];
    float innerH = m_ComputedRect.w - m_Padding[1] - m_Padding[3];

    int fillCount = 0;
    int growCount = 0;
    float fixedTotal = 0.0f;
    float fillTotal = 0.0f;

    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;
        switch (child->m_SizePolicy) {
            case SizePolicy::Fixed:
                fixedTotal += child->GetMinSize().x;
                break;
            case SizePolicy::Content:
                fixedTotal += child->GetContentSize().x;
                break;
            case SizePolicy::Fill:
                fillCount++;
                break;
            case SizePolicy::Grow:
                growCount++;
                break;
        }
        fixedTotal += m_Spacing;
    }
    fixedTotal -= m_Spacing;
    if (fixedTotal < 0.0f) fixedTotal = 0.0f;

    float leftover = innerW - fixedTotal;
    if (leftover < 0.0f) leftover = 0.0f;

    if (fillCount > 0) {
        fillTotal = leftover / fillCount;
        leftover = 0.0f;
    }

    float growShare = (growCount > 0) ? leftover / growCount : 0.0f;

    float cursorX = innerX;
    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;

        float childW = 0.0f;
        float childH = innerH;

        switch (child->m_SizePolicy) {
            case SizePolicy::Fixed:
                childW = child->GetMinSize().x;
                break;
            case SizePolicy::Content:
                childW = std::max(child->GetContentSize().x, child->GetMinSize().x);
                break;
            case SizePolicy::Fill:
                childW = fillTotal;
                break;
            case SizePolicy::Grow:
                childW = child->GetMinSize().x + growShare;
                break;
        }

        child->m_Rect.offset.left = cursorX + m_ComputedRect.x;
        child->m_Rect.offset.top = innerY + m_ComputedRect.y;
        child->m_Rect.offset.right = cursorX + childW + m_ComputedRect.x;
        child->m_Rect.offset.bottom = innerY + childH + m_ComputedRect.y;
        child->m_Rect.anchor = AnchorSet::TopLeft();

        child->ComputeLayout({childW, childH});
        cursorX += childW + m_Spacing;
    }
}

void UIElement::ComputeColumnLayout(const Vector2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);

    float innerX = m_Padding[0];
    float innerY = m_Padding[1];
    float innerW = m_ComputedRect.z - m_Padding[0] - m_Padding[2];
    float innerH = m_ComputedRect.w - m_Padding[1] - m_Padding[3];

    int fillCount = 0;
    int growCount = 0;
    float fixedTotal = 0.0f;
    float fillTotal = 0.0f;

    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;
        switch (child->m_SizePolicy) {
            case SizePolicy::Fixed:
                fixedTotal += child->GetMinSize().y;
                break;
            case SizePolicy::Content:
                fixedTotal += child->GetContentSize().y;
                break;
            case SizePolicy::Fill:
                fillCount++;
                break;
            case SizePolicy::Grow:
                growCount++;
                break;
        }
        fixedTotal += m_Spacing;
    }
    fixedTotal -= m_Spacing;
    if (fixedTotal < 0.0f) fixedTotal = 0.0f;

    float leftover = innerH - fixedTotal;
    if (leftover < 0.0f) leftover = 0.0f;

    if (fillCount > 0) {
        fillTotal = leftover / fillCount;
        leftover = 0.0f;
    }

    float growShare = (growCount > 0) ? leftover / growCount : 0.0f;

    float cursorY = innerY;
    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;

        float childW = innerW;
        float childH = 0.0f;

        switch (child->m_SizePolicy) {
            case SizePolicy::Fixed:
                childH = child->GetMinSize().y;
                childW = std::max(child->GetMinSize().x, innerW);
                break;
            case SizePolicy::Content:
                childH = std::max(child->GetContentSize().y, child->GetMinSize().y);
                childW = std::max(std::max(child->GetContentSize().x, child->GetMinSize().x), innerW);
                break;
            case SizePolicy::Fill:
                childH = std::max(fillTotal, child->GetMinSize().y);
                break;
            case SizePolicy::Grow:
                childH = child->GetMinSize().y + growShare;
                break;
        }

        child->m_Rect.offset.left = innerX + m_ComputedRect.x;
        child->m_Rect.offset.top = cursorY + m_ComputedRect.y;
        child->m_Rect.offset.right = innerX + childW + m_ComputedRect.x;
        child->m_Rect.offset.bottom = cursorY + childH + m_ComputedRect.y;
        child->m_Rect.anchor = AnchorSet::TopLeft();

        child->ComputeLayout({childW, childH});
        cursorY += childH + m_Spacing;
    }
}

UIElement* UIElement::FindChildByName(const std::string& name)
{
    if (m_Name == name)
        return this;
    for (auto* child : m_Children) {
        auto* found = child->FindChildByName(name);
        if (found)
            return found;
    }
    return nullptr;
}

} // namespace Leir
