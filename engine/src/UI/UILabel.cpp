#include "LeirEngine/UI/UILabel.h"
#include "LeirEngine/UI/Font.h"

namespace Leir {

UILabel::UILabel() = default;
UILabel::~UILabel() = default;

void UILabel::SetText(const std::string& text)
{
    if (m_Text != text) {
        m_Text = text;
        MarkDirty();
    }
}

Vector2 UILabel::GetMinSize() const
{
    if (!m_Font) return {0.0f, (float)m_FontSize};
    if (!m_SizeValid) {
        m_CachedSize = m_Font->MeasureText(m_Text, m_WordWrap ? m_MaxWidth : 0.0f);
        m_SizeValid = true;
    }
    return m_CachedSize;
}

void UILabel::OnLayoutComputed()
{
    Rebuild();
}

void UILabel::Rebuild()
{
    // FIX (2026-08-24, Bug 1): rebuild when the label is dirty OR the font atlas
    // was re-rasterized (DPI change bumps Font::GetAtlasGen). Cached glyphs from
    // an older gen have stale UVs that sample the wrong atlas region -> glitched /
    // flattened text on static labels. The extra check is one 32-bit compare per
    // label per frame (this already runs for every label every layout); no work
    // happens in the steady state.
    if (!m_Dirty && (!m_Font || m_Font->GetAtlasGen() == m_FontGen)) return;
    m_Dirty = false;
    m_GlyphQuads.clear();
    // Record the atlas generation these glyphs were built with (drives the check
    // above; also consumed on the empty-text path so a rescale is not misdetected).
    m_FontGen = m_Font ? m_Font->GetAtlasGen() : 0;

    if (!m_Font || m_Text.empty()) return;

    const auto& cr = GetComputedRect();
    float contentW = cr.z;

    auto rawQuads = m_Font->LayoutText(m_Text, m_WordWrap ? contentW : 0.0f);

    // Calculate text block size for alignment
    float blockW = 0.0f, blockH = m_Font->GetLineHeight();
    for (size_t i = 0; i < rawQuads.size(); i += 2) {
        float gw = rawQuads[i].z;
        float gh = rawQuads[i].w;
        float gx = rawQuads[i].x;
        float gy = rawQuads[i].y;
        if (gx + gw > blockW) blockW = gx + gw;
        if (gy + gh > blockH) blockH = gy + gh;
    }
    if (blockH < m_Font->GetLineHeight()) blockH = m_Font->GetLineHeight();

    // Offset based on alignment
    float alignX = 0.0f;
    if (m_Alignment == Alignment::Center)
        alignX = (contentW - blockW) * 0.5f;
    else if (m_Alignment == Alignment::Right)
        alignX = contentW - blockW;

    float offsetX = alignX;
    float offsetY = (cr.w - blockH) * 0.5f + (m_Font ? m_Font->GetAscender() : 0.0f);

    for (size_t i = 0; i < rawQuads.size(); i += 2) {
        const auto& rect = rawQuads[i];
        const auto& uv = rawQuads[i + 1];

        TextGlyphQuad q;
        q.rect = {offsetX + rect.x, offsetY + rect.y, rect.z, rect.w};
        q.uv = uv;
        q.color = m_Color;
        m_GlyphQuads.push_back(q);
    }
}

} // namespace Leir
