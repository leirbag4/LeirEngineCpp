#pragma once

/**
 * @file DockDropOverlay.h
 * @brief Visual feedback for dock drag&drop: ghost tab and highlighted zone.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIPanel.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

/**
 * @brief Visual feedback for dock drag&drop: ghost tab + highlighted zone.
 * @ingroup UI
 * @details Rendered in the overlay layer (above viewports) via SetOverlayLayer.
 *  Inactive unless a drag is in progress.
 */
class LEIR_API DockDropOverlay : public UIPanel {
public:
    /**
     * @brief Constructs an overlay (initially invisible).
     */
    DockDropOverlay();

    /**
     * @brief Destroys the overlay.
     */
    ~DockDropOverlay() override;

    /**
     * @brief Sets visibility.
     * @param[in] visible True to show.
     */
    void SetVisible(bool visible);

    /**
     * @brief Sets ghost tab rectangle (absolute screen coordinates).
     * @param[in] rect Rectangle (x,y,w,h).
     */
    void SetGhostRect(const Vector4& rect);

    /**
     * @brief Sets highlighted zone rectangle and color.
     * @param[in] rect Rectangle (x,y,w,h).
     * @param[in] color Highlight color.
     */
    void SetZoneRect(const Vector4& rect, const Vector4& color);

    /**
     * @brief Hides the highlighted zone.
     */
    void HideZone();

    /**
     * @brief Computes layout without adding position to ghost/zone children.
     * @details Ghost/zone are positioned in absolute screen coordinates; base
     *  Free layout would accumulate the overlay's origin each frame and drift.
     * @param[in] availableSize Available size.
     * @param[in] parentOffset Parent absolute offset.
     */
    void ComputeLayout(const Vector2& availableSize, const Vector2& parentOffset = Vector2(0.0f, 0.0f)) override;

private:
    UIPanel* m_Ghost = nullptr;                     ///< Ghost panel (owned).
    UIPanel* m_Zone = nullptr;                      ///< Zone highlight panel (owned).
};

} // namespace Leir
