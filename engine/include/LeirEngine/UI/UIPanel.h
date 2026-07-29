#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include <glm/glm.hpp>

namespace Leir {

class LEIR_API UIPanel : public UIElement {
public:
    UIPanel();
    ~UIPanel() override;
};

} // namespace Leir
