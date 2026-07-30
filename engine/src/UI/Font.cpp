#include "LeirEngine/UI/Font.h"
#include "LeirEngine/Rendering/Texture2D.h"
#include "LeirEngine/Rendering/VulkanDevice.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <cstdio>
#include <spdlog/spdlog.h>

namespace Leir {

Font::Font(VulkanDevice* device, const std::string& ttfPath, int fontSize)
    : m_Device(device), m_FontSize(fontSize)
{
    FILE* f = fopen(ttfPath.c_str(), "rb");
    if (!f) {
        spdlog::error("Font: failed to open {}", ttfPath);
        m_LineHeight = (float)fontSize;
        m_Ascender = (float)fontSize;
        m_SpaceWidth = (float)fontSize * 0.5f;
        BuildAtlas();
        return;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    m_TTFData.resize(fileSize);
    fread(m_TTFData.data(), 1, fileSize, f);
    fclose(f);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, m_TTFData.data(), 0)) {
        spdlog::error("Font: failed to init {}", ttfPath);
        m_LineHeight = (float)fontSize;
        m_Ascender = (float)fontSize;
        m_SpaceWidth = (float)fontSize * 0.5f;
        BuildAtlas();
        return;
    }

    m_Scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    m_LineHeight = (float)(ascent - descent + lineGap) * m_Scale;
    m_Ascender = (float)ascent * m_Scale;

    int advance;
    stbtt_GetCodepointHMetrics(&info, 32, &advance, nullptr);
    m_SpaceWidth = (float)advance * m_Scale;

    m_PackedGlyphs.clear();
    for (uint32_t cp = 32; cp <= 126; ++cp) {
        int w, h, xOff, yOff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, m_Scale, m_Scale, cp, &w, &h, &xOff, &yOff);
        if (!bitmap) continue;

        stbtt_GetCodepointHMetrics(&info, cp, &advance, nullptr);
        PackedGlyph pg;
        pg.codepoint = cp;
        pg.w = w;
        pg.h = h;
        pg.advance = (float)advance * m_Scale;
        pg.bearingX = (float)xOff;
        pg.bearingY = (float)yOff;

        int found = -1;
        for (size_t i = 0; i < m_PackedGlyphs.size(); ++i) {
            if (m_PackedGlyphs[i].codepoint == cp) { found = (int)i; break; }
        }
        if (found >= 0) {
            stbtt_FreeBitmap(bitmap, nullptr);
            continue;
        }

        m_PackedGlyphs.push_back(pg);
        m_GlyphCache[cp] = {}; // placeholder
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    BuildAtlas();

    spdlog::info("Font loaded: {} ({}px, {} glyphs)", ttfPath, fontSize, m_PackedGlyphs.size());
}

Font::~Font() = default;

void Font::BuildAtlas()
{
    if (m_PackedGlyphs.empty()) {
        m_AtlasWidth = 1;
        m_AtlasHeight = 1;
        std::vector<unsigned char> dummy(4, 255);
        Image dummyImg(1, 1, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        m_AtlasTexture = std::make_unique<Texture2D>(m_Device, dummyImg);
        return;
    }

    // Simple row-by-row packer
    int atlasW = 128;
    int atlasH = 128;
    int maxGlyphW = 0, maxGlyphH = 0;
    for (auto& pg : m_PackedGlyphs) {
        if (pg.w > maxGlyphW) maxGlyphW = pg.w;
        if (pg.h > maxGlyphH) maxGlyphH = pg.h;
    }

    // Compute reasonable atlas size
    int totalArea = 0;
    for (auto& pg : m_PackedGlyphs) totalArea += (pg.w + 2) * (pg.h + 2);
    totalArea = (int)(totalArea * 1.2f);

    atlasW = 128;
    atlasH = 128;
    while (atlasW * atlasH < totalArea && atlasW < 4096) {
        atlasW *= 2;
        if (atlasW * atlasH < totalArea)
            atlasH *= 2;
    }

    m_AtlasWidth = atlasW;
    m_AtlasHeight = atlasH;
    std::vector<unsigned char> atlasData(atlasW * atlasH, 0);

    int cursorX = 1, cursorY = 1;
    int rowH = 0;

    stbtt_fontinfo info;
    stbtt_InitFont(&info, m_TTFData.data(), 0);

    for (auto& pg : m_PackedGlyphs) {
        if (cursorX + pg.w + 1 > atlasW) {
            cursorX = 1;
            cursorY += rowH + 1;
            rowH = 0;
        }

        // Skip glyph if it doesn't fit vertically
        if (cursorY + pg.h + 1 > atlasH) {
            spdlog::warn("Font: glyph {} (codepoint {}) exceeds atlas height, skipping", pg.codepoint, pg.codepoint);
            continue;
        }

        int w, h, xOff, yOff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, m_Scale, m_Scale, pg.codepoint, &w, &h, &xOff, &yOff);
        if (!bitmap) continue;

        pg.x = cursorX;
        pg.y = cursorY;
        pg.w = w;
        pg.h = h;

        for (int by = 0; by < h; ++by) {
            for (int bx = 0; bx < w; ++bx) {
                int srcIdx = by * w + bx;
                int dstIdx = (cursorY + by) * atlasW + (cursorX + bx);
                atlasData[dstIdx] = bitmap[srcIdx];
            }
        }

        GlyphInfo gi;
        gi.uv0 = {(float)(cursorX) / atlasW, (float)(cursorY) / atlasH};
        gi.uv1 = {(float)(cursorX + w) / atlasW, (float)(cursorY + h) / atlasH};
        gi.size = {(float)w, (float)h};
        gi.bearing = {pg.bearingX, pg.bearingY};
        gi.advance = pg.advance;
        m_GlyphCache[pg.codepoint] = gi;

        cursorX += w + 2;
        if (h + 2 > rowH) rowH = h + 2;

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    // Convert alpha-only to RGBA
    std::vector<unsigned char> rgbaData(atlasW * atlasH * 4);
    for (int i = 0; i < atlasW * atlasH; ++i) {
        rgbaData[i * 4 + 0] = 255;
        rgbaData[i * 4 + 1] = 255;
        rgbaData[i * 4 + 2] = 255;
        rgbaData[i * 4 + 3] = atlasData[i];
    }

    Image atlasImg(atlasW, atlasH, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    memcpy(atlasImg.GetData(), rgbaData.data(), rgbaData.size());
    m_AtlasTexture = std::make_unique<Texture2D>(m_Device, atlasImg);
}

const Font::GlyphInfo& Font::GetGlyphInfo(uint32_t codepoint)
{
    auto it = m_GlyphCache.find(codepoint);
    if (it != m_GlyphCache.end())
        return it->second;

    // Fallback to '?' or first available glyph
    static GlyphInfo fallback = {};
    it = m_GlyphCache.find(63); // '?'
    if (it != m_GlyphCache.end())
        return it->second;
    if (!m_GlyphCache.empty())
        return m_GlyphCache.begin()->second;
    return fallback;
}

Vector2 Font::MeasureText(const std::string& text, float maxWidth) const
{
    if (text.empty()) return {0.0f, m_LineHeight};

    float x = 0, y = 0;
    float w = 0, h = m_LineHeight;
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = (unsigned char)text[i];
        if (cp < 0x80) { ++i; }
        else if ((cp & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((cp & 0x1F) << 6) | (text[i+1] & 0x3F); i += 2; }
        else if ((cp & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((cp & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F); i += 3; }
        else { ++i; continue; }

        if (cp == '\n') {
            if (x > w) w = x;
            x = 0;
            y += m_LineHeight;
            h += m_LineHeight;
            continue;
        }

        auto it = m_GlyphCache.find(cp);
        if (it == m_GlyphCache.end()) it = m_GlyphCache.find(63);
        if (it == m_GlyphCache.end()) continue;

        float nextX = x + it->second.advance;
        if (maxWidth > 0 && nextX > maxWidth && x > 0) {
            if (x > w) w = x;
            x = 0;
            y += m_LineHeight;
            h += m_LineHeight;
            nextX = it->second.advance;
        }
        x = nextX;
    }
    if (x > w) w = x;
    return {w, h};
}

std::vector<Vector4> Font::LayoutText(const std::string& text, float maxWidth) const
{
    std::vector<Vector4> quads;
    if (text.empty()) return quads;

    float x = 0, y = 0;
    for (size_t i = 0; i < text.size();) {
        uint32_t cp = (unsigned char)text[i];
        if (cp < 0x80) { ++i; }
        else if ((cp & 0xE0) == 0xC0 && i + 1 < text.size()) { cp = ((cp & 0x1F) << 6) | (text[i+1] & 0x3F); i += 2; }
        else if ((cp & 0xF0) == 0xE0 && i + 2 < text.size()) { cp = ((cp & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F); i += 3; }
        else { ++i; continue; }

        if (cp == '\n') {
            x = 0;
            y += m_LineHeight;
            continue;
        }
        if (cp == ' ') {
            x += m_SpaceWidth;
            continue;
        }

        auto it = m_GlyphCache.find(cp);
        if (it == m_GlyphCache.end()) it = m_GlyphCache.find(63);
        if (it == m_GlyphCache.end()) continue;

        const auto& g = it->second;
        float nextX = x + g.advance;
        if (maxWidth > 0 && nextX > maxWidth && x > 0) {
            x = 0;
            y += m_LineHeight;
            nextX = g.advance;
        }

        float gx = x + g.bearing.x;
        float gy = y + g.bearing.y;
        quads.push_back({gx, gy, g.size.x, g.size.y});
        quads.push_back({g.uv0.x, g.uv0.y, g.uv1.x - g.uv0.x, g.uv1.y - g.uv0.y});
        x = nextX;
    }
    return quads;
}

} // namespace Leir
