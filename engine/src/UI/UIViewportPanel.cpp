#include "LeirEngine/UI/UIViewportPanel.h"

namespace Leir {

UIViewportPanel::UIViewportPanel()
{
}

UIViewportPanel::~UIViewportPanel()
{
}

glm::vec2 UIViewportPanel::ScreenToViewport(float screenX, float screenY) const
{
    const auto& cr = m_ComputedRect;
    float vx = screenX - cr.x;
    float vy = screenY - cr.y;
    return { vx, vy };
}

bool UIViewportPanel::IsInsideViewport(float screenX, float screenY) const
{
    const auto& cr = m_ComputedRect;
    return screenX >= cr.x && screenX < cr.x + cr.z &&
           screenY >= cr.y && screenY < cr.y + cr.w;
}

} // namespace Leir
