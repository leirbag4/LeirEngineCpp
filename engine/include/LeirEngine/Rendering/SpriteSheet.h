#pragma once
#include "LeirEngine/Core/Export.h"
#include <glm/glm.hpp>

namespace Leir {

class Texture2D;

class LEIR_API SpriteSheet {
public:
    SpriteSheet(Texture2D* texture, int frameWidth, int frameHeight);
    ~SpriteSheet() = default;

    int GetFrameCount() const;
    glm::vec4 GetUVRect(int frameIndex) const;

    Texture2D* GetTexture() const { return m_Texture; }
    int GetFrameWidth() const { return m_FrameWidth; }
    int GetFrameHeight() const { return m_FrameHeight; }
    int GetColumns() const { return m_Columns; }
    int GetRows() const { return m_Rows; }

private:
    Texture2D* m_Texture;
    int m_FrameWidth;
    int m_FrameHeight;
    int m_Columns;
    int m_Rows;
};

} // namespace Leir
