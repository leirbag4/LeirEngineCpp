#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leir {

namespace RHI { class RenderBackend; }
class Texture2D;

class LEIR_API Font {
public:
    Font(RHI::RenderBackend* device, const std::string& ttfPath, int fontSize = 16);
    ~Font();

    struct GlyphInfo {
        Vector2 uv0, uv1;
        Vector2 size;
        Vector2 bearing;
        float advance;
    };

    const GlyphInfo& GetGlyphInfo(uint32_t codepoint);
    Texture2D* GetAtlasTexture() const { return m_AtlasTexture.get(); }
    float GetLineHeight() const { return m_LineHeight; }
    float GetAscender() const { return m_Ascender; }
    float GetSpaceWidth() const { return m_SpaceWidth; }

    Vector2 MeasureText(const std::string& text, float maxWidth = 0.0f) const;
    std::vector<Vector4> LayoutText(const std::string& text, float maxWidth = 0.0f) const;

private:
    void BuildAtlas();

    RHI::RenderBackend* m_Device = nullptr;    std::vector<uint8_t> m_TTFData;
    int m_FontSize;
    float m_Scale;
    float m_LineHeight;
    float m_Ascender;
    float m_SpaceWidth;

    struct PackedGlyph {
        uint32_t codepoint;
        int x, y, w, h;
        float advance;
        float bearingX, bearingY;
    };
    std::vector<PackedGlyph> m_PackedGlyphs;
    std::unordered_map<uint32_t, GlyphInfo> m_GlyphCache;
    std::unique_ptr<Texture2D> m_AtlasTexture;
    int m_AtlasWidth = 0;
    int m_AtlasHeight = 0;
};

} // namespace Leir
