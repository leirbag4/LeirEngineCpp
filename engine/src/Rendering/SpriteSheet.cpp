#include "LeirEngine/Rendering/SpriteSheet.h"
#include "LeirEngine/Rendering/Texture2D.h"

namespace Leir {

SpriteSheet::SpriteSheet(Texture2D* texture, int frameWidth, int frameHeight)
    : m_Texture(texture)
    , m_FrameWidth(frameWidth)
    , m_FrameHeight(frameHeight)
{
    m_Columns = texture->GetWidth() / frameWidth;
    m_Rows = texture->GetHeight() / frameHeight;
}

int SpriteSheet::GetFrameCount() const
{
    return m_Columns * m_Rows;
}

Vector4 SpriteSheet::GetUVRect(int frameIndex) const
{
    if (frameIndex < 0 || frameIndex >= GetFrameCount())
        frameIndex = 0;

    int col = frameIndex % m_Columns;
    int row = frameIndex / m_Columns;

    float texW = (float)m_Texture->GetWidth();
    float texH = (float)m_Texture->GetHeight();

    float u = (col * m_FrameWidth) / texW;
    float v = (row * m_FrameHeight) / texH;
    float w = m_FrameWidth / texW;
    float h = m_FrameHeight / texH;

    return {u, v, w, h};
}

} // namespace Leir
