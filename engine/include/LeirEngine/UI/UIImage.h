#pragma once

/**
 * @file UIImage.h
 * @brief Image widget: textured quad with optional 9-slice.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class Texture2D;

/**
 * @brief Image element: draws a Texture2D in its computed rectangle.
 * @ingroup UI
 * @details Supports 9-slice scaling via slice borders.
 */
class LEIR_API UIImage : public UIElement {
public:
    /**
     * @brief Constructs an empty image.
     */
    UIImage();

    /**
     * @brief Destroys the image.
     */
    ~UIImage() override;

    /**
     * @brief Sets the texture to display.
     * @param[in] texture Texture pointer (not owned).
     */
    void SetTexture(Texture2D* texture) { m_Texture = texture; }

    /**
     * @brief Returns the texture.
     * @return Texture pointer or nullptr.
     */
    Texture2D* GetTexture() const { return m_Texture; }

    /**
     * @brief Enables 9-slice scaling.
     * @param[in] enabled True to enable 9-slice.
     */
    void SetSliceEnabled(bool enabled) { m_SliceEnabled = enabled; }

    /**
     * @brief Returns whether 9-slice is enabled.
     * @return True if 9-slice.
     */
    bool IsSliceEnabled() const { return m_SliceEnabled; }

    /**
     * @brief Sets 9-slice borders (left, top, right, bottom) in logical pixels.
     * @param[in] left Left border.
     * @param[in] top Top border.
     * @param[in] right Right border.
     * @param[in] bottom Bottom border.
     */
    void SetSliceBorders(float left, float top, float right, float bottom);

    /**
     * @brief Returns the 9-slice borders.
     * @return Pointer to 4 floats (left, top, right, bottom).
     */
    const float* GetSliceBorders() const { return m_SliceBorders; }

    /**
     * @brief Returns minimum size (texture size or 0).
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

private:
    Texture2D* m_Texture = nullptr;                 ///< Texture (not owned).
    bool m_SliceEnabled = false;                    ///< 9-slice flag.
    float m_SliceBorders[4] = {0.0f, 0.0f, 0.0f, 0.0f}; ///< Slice borders.
};

} // namespace Leir
