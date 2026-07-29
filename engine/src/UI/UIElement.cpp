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

void UIElement::RemoveChild(UIElement* child)
{
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        m_Children.erase(it);
        child->m_Parent = nullptr;
    }
}

glm::vec2 UIElement::GetMinSize() const
{
    return {0.0f, 0.0f};
}

void UIElement::ComputeLayout(const glm::vec2& availableSize)
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

void UIElement::ComputeFreeLayout(const glm::vec2& availableSize)
{
    m_ComputedRect = m_Rect.GetRect(availableSize);

    for (auto* child : m_Children) {
        if (!child->IsActive())
            continue;
        glm::vec2 childSize = {
            m_ComputedRect.z - m_Padding[0] - m_Padding[2],
            m_ComputedRect.w - m_Padding[1] - m_Padding[3]
        };
        child->ComputeLayout(childSize);
        child->m_ComputedRect.x += m_ComputedRect.x;
        child->m_ComputedRect.y += m_ComputedRect.y;
    }
}

void UIElement::ComputeRowLayout(const glm::vec2& availableSize)
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
            case SizePolicy::Fill:
                childW = fillTotal;
                break;
            case SizePolicy::Grow:
                childW = child->GetMinSize().x + growShare;
                break;
        }

        child->m_Rect.offset.left = cursorX;
        child->m_Rect.offset.top = innerY;
        child->m_Rect.offset.right = cursorX + childW;
        child->m_Rect.offset.bottom = innerY + childH;
        child->m_Rect.anchor = AnchorSet::TopLeft();

        child->ComputeLayout({childW, childH});
        child->m_ComputedRect.x += m_ComputedRect.x;
        child->m_ComputedRect.y += m_ComputedRect.y;
        cursorX += childW + m_Spacing;
    }
}

void UIElement::ComputeColumnLayout(const glm::vec2& availableSize)
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
                break;
            case SizePolicy::Fill:
                childH = fillTotal;
                break;
            case SizePolicy::Grow:
                childH = child->GetMinSize().y + growShare;
                break;
        }

        child->m_Rect.offset.left = innerX;
        child->m_Rect.offset.top = cursorY;
        child->m_Rect.offset.right = innerX + childW;
        child->m_Rect.offset.bottom = cursorY + childH;
        child->m_Rect.anchor = AnchorSet::TopLeft();

        child->ComputeLayout({childW, childH});
        child->m_ComputedRect.x += m_ComputedRect.x;
        child->m_ComputedRect.y += m_ComputedRect.y;
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
