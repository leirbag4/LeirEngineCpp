#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <glm/glm.hpp>

namespace Leir {

class RenderTexture;

class LEIR_API UIViewportPanel : public UIElement {
public:
    UIViewportPanel();
    ~UIViewportPanel() override;

    void SetRenderTexture(RenderTexture* rt) { m_RenderTexture = rt; }
    RenderTexture* GetRenderTexture() const { return m_RenderTexture; }

    glm::vec2 ScreenToViewport(float screenX, float screenY) const;
    bool IsInsideViewport(float screenX, float screenY) const;

private:
    RenderTexture* m_RenderTexture = nullptr;
};

} // namespace Leir
