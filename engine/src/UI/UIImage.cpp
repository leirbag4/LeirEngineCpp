#include "LeirEngine/UI/UIImage.h"

namespace Leir {

UIImage::UIImage() = default;
UIImage::~UIImage() = default;

void UIImage::SetSliceBorders(float left, float top, float right, float bottom)
{
    m_SliceBorders[0] = left;
    m_SliceBorders[1] = top;
    m_SliceBorders[2] = right;
    m_SliceBorders[3] = bottom;
}

glm::vec2 UIImage::GetMinSize() const
{
    if (m_SliceEnabled) {
        return {
            m_SliceBorders[0] + m_SliceBorders[2],
            m_SliceBorders[1] + m_SliceBorders[3]
        };
    }
    return {0.0f, 0.0f};
}

} // namespace Leir
