#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"

namespace Leir {

class Texture2D;

class LEIR_API UIImage : public UIElement {
public:
    UIImage();
    ~UIImage() override;

    void SetTexture(Texture2D* texture) { m_Texture = texture; }
    Texture2D* GetTexture() const { return m_Texture; }

    void SetSliceEnabled(bool enabled) { m_SliceEnabled = enabled; }
    bool IsSliceEnabled() const { return m_SliceEnabled; }

    void SetSliceBorders(float left, float top, float right, float bottom);
    const float* GetSliceBorders() const { return m_SliceBorders; }

    Vector2 GetMinSize() const override;

private:
    Texture2D* m_Texture = nullptr;
    bool m_SliceEnabled = false;
    float m_SliceBorders[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Leir
