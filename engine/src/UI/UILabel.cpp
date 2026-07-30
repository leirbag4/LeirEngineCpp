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

glm::vec2 UILabel::GetMinSize() const
{
    if (!m_Font) return {0.0f, (float)m_FontSize};
    auto size = const_cast<UILabel*>(this)->m_Font->MeasureText(m_Text, m_WordWrap ? m_MaxWidth : 0.0f);
    return size;
}

void UILabel::OnLayoutComputed()
{
    Rebuild();
}

void UILabel::Rebuild()
{
    if (!m_Dirty) return;
    m_Dirty = false;
    m_GlyphQuads.clear();

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
