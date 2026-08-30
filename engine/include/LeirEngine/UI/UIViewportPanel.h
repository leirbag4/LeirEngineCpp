#pragma once

/**
 * @file UIViewportPanel.h
 * @brief UI element that displays a RenderTexture (viewport) and maps screen to viewport coordinates.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class RenderTexture;

/**
 * @brief Viewport panel: UI element that hosts a RenderTexture and maps coordinates.
 * @ingroup UI
 * @details Detected by UIRenderer via dynamic_cast to create a ViewportDraw entry.
 */
class LEIR_API UIViewportPanel : public UIElement {
public:
    /**
     * @brief Constructs an empty viewport panel.
     */
    UIViewportPanel();

    /**
     * @brief Destroys the viewport panel.
     */
    ~UIViewportPanel() override;

    /**
     * @brief Sets the RenderTexture to display.
     * @param[in] rt RenderTexture pointer (not owned).
     */
    void SetRenderTexture(RenderTexture* rt) { m_RenderTexture = rt; }

    /**
     * @brief Returns the RenderTexture.
     * @return RenderTexture pointer or nullptr.
     */
    RenderTexture* GetRenderTexture() const { return m_RenderTexture; }

    /**
     * @brief Converts screen coordinates to viewport-local coordinates.
     * @param[in] screenX Screen X in logical pixels.
     * @param[in] screenY Screen Y in logical pixels.
     * @return Viewport-local coordinates.
     */
    Vector2 ScreenToViewport(float screenX, float screenY) const;

    /**
     * @brief Returns whether a screen position is inside the viewport.
     * @param[in] screenX Screen X.
     * @param[in] screenY Screen Y.
     * @return True if inside.
     */
    bool IsInsideViewport(float screenX, float screenY) const;

private:
    RenderTexture* m_RenderTexture = nullptr;       ///< RenderTexture (not owned).
};

} // namespace Leir
