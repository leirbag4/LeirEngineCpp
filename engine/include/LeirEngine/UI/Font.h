#pragma once

/**
 * @file Font.h
 * @brief Font atlas: rasterization, glyph metrics and text layout.
 * @ingroup UI
 */

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

/**
 * @brief Font atlas: rasterizes a TTF at a DPI scale, provides glyph metrics and layout.
 * @ingroup UI
 * @details Metrics are kept in logical pixels (atlas pixels / contentScale) so each
 *  texel maps 1:1 to a physical pixel. Supports re-rasterization on DPI change.
 */
class LEIR_API Font {
public:
    /**
     * @brief Constructs a font and builds its atlas.
     * @param[in] device Render backend for atlas texture.
     * @param[in] ttfPath Path to TTF file.
     * @param[in] fontSize Font size in logical pixels.
     * @param[in] contentScale DPI scale (physical/logical).
     */
    Font(RHI::RenderBackend* device, const std::string& ttfPath, int fontSize = 16, float contentScale = 1.0f);

    /**
     * @brief Destroys the font and its atlas.
     */
    ~Font();

    /**
     * @brief Re-rasterizes the atlas for a new DPI scale in place.
     * @details Metrics stay logical, so all Font* holders remain valid.
     * @param[in] contentScale New DPI scale.
     */
    void SetContentScale(float contentScale);

    /**
     * @brief Glyph metrics for one codepoint.
     * @ingroup UI
     */
    struct GlyphInfo {
        Vector2 uv0;     ///< UV top-left in atlas.
        Vector2 uv1;     ///< UV bottom-right in atlas.
        Vector2 size;    ///< Glyph size in logical pixels.
        Vector2 bearing; ///< Bearing (offset from baseline).
        float advance;   ///< Advance width.
    };

    /**
     * @brief Returns glyph info for a codepoint.
     * @param[in] codepoint Unicode codepoint.
     * @return GlyphInfo reference (creates fallback if missing).
     */
    const GlyphInfo& GetGlyphInfo(uint32_t codepoint);

    /**
     * @brief Returns atlas texture.
     * @return Texture pointer.
     */
    Texture2D* GetAtlasTexture() const { return m_AtlasTexture.get(); }

    /**
     * @brief Returns line height.
     * @return Line height in logical pixels.
     */
    float GetLineHeight() const { return m_LineHeight; }

    /**
     * @brief Returns ascender (baseline to top).
     * @return Ascender in logical pixels.
     */
    float GetAscender() const { return m_Ascender; }

    /**
     * @brief Returns space width (advance for ' ').
     * @return Space width in logical pixels.
     */
    float GetSpaceWidth() const { return m_SpaceWidth; }

    /**
     * @brief Returns atlas generation (bumped on re-rasterization).
     * @details Labels cache glyph UVs; when this changes, stale UVs glitch.
     * @return Generation counter.
     */
    uint32_t GetAtlasGen() const { return m_AtlasGen; }

    /**
     * @brief Measures text extent.
     * @param[in] text Text to measure.
     * @param[in] maxWidth Max width for word-wrap (0 = unlimited).
     * @return Text size (width, height) in logical pixels.
     */
    Vector2 MeasureText(const std::string& text, float maxWidth = 0.0f) const;

    /**
     * @brief Lays out text into glyph quads.
     * @param[in] text Text to lay out.
     * @param[in] maxWidth Max width for word-wrap (0 = unlimited).
     * @return Vector of glyph quads (rect + uv).
     */
    std::vector<Vector4> LayoutText(const std::string& text, float maxWidth = 0.0f) const;

private:
    void BuildAtlas();
    void Rebuild(float contentScale);

    RHI::RenderBackend* m_Device = nullptr;    ///< Backend for atlas texture.
    std::vector<uint8_t> m_TTFData;           ///< TTF file data.
    int m_FontSize;                           ///< Font size (logical).
    float m_ContentScale = 1.0f;              ///< DPI scale.
    float m_Scale;                            ///< Font scale factor.
    float m_LineHeight;                       ///< Line height.
    float m_Ascender;                         ///< Ascender.
    float m_SpaceWidth;                       ///< Space width.

    struct PackedGlyph {
        uint32_t codepoint; ///< Codepoint.
        int x, y, w, h;     ///< Atlas rect.
        float advance;      ///< Advance.
        float bearingX, bearingY; ///< Bearing.
    };
    std::vector<PackedGlyph> m_PackedGlyphs;                  ///< Packed glyphs.
    std::unordered_map<uint32_t, GlyphInfo> m_GlyphCache;     ///< Glyph cache.
    std::unique_ptr<Texture2D> m_AtlasTexture;                ///< Atlas texture.
    int m_AtlasWidth = 0;                                     ///< Atlas width.
    int m_AtlasHeight = 0;                                    ///< Atlas height.
    uint32_t m_AtlasGen = 0;                                  ///< Atlas generation.
};

} // namespace Leir
