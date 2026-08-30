#pragma once

/**
 * @file UIPanel.h
 * @brief Simple colored panel widget (background quad).
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

/**
 * @brief Colored panel element: draws its computed rectangle as a solid quad.
 * @ingroup UI
 * @details Inherits all layout, clipping and hit-testing from UIElement. The
 *  background color is the element's tint (SetColor/GetColor).
 */
class LEIR_API UIPanel : public UIElement {
public:
    /**
     * @brief Constructs a panel with default (white) tint.
     */
    UIPanel();

    /**
     * @brief Destroys the panel.
     */
    ~UIPanel() override;
};

} // namespace Leir
