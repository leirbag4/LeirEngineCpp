#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Core/Component.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"

namespace Leir {

class Texture2D;
class SpriteSheet;
class Material;

class LEIR_API SpriteRenderer : public Component {
public:
    SpriteRenderer();
    ~SpriteRenderer() override;

    void SetTexture(Texture2D* texture) { m_Texture = texture; }
    Texture2D* GetTexture() const { return m_Texture; }

    void SetSpriteSheet(SpriteSheet* sheet) { m_SpriteSheet = sheet; }
    SpriteSheet* GetSpriteSheet() const { return m_SpriteSheet; }

    void SetFrameIndex(int index) { m_FrameIndex = index; }
    int GetFrameIndex() const { return m_FrameIndex; }

    void SetColor(const Vector4& color) { m_Color = color; }
    const Vector4& GetColor() const { return m_Color; }

    void SetTiling(const Vector2& tiling) { m_Tiling = tiling; }
    const Vector2& GetTiling() const { return m_Tiling; }

    void SetFlipX(bool flip) { m_FlipX = flip; }
    bool GetFlipX() const { return m_FlipX; }
    void SetFlipY(bool flip) { m_FlipY = flip; }
    bool GetFlipY() const { return m_FlipY; }

private:
    Texture2D* m_Texture = nullptr;
    SpriteSheet* m_SpriteSheet = nullptr;
    int m_FrameIndex = 0;
    Vector4 m_Color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector2 m_Tiling{1.0f, 1.0f};
    bool m_FlipX = false;
    bool m_FlipY = false;
};

} // namespace Leir
