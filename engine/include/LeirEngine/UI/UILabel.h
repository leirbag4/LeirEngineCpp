#pragma once
#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>

namespace Leir {

class Font;

struct LEIR_API TextGlyphQuad {
    Vector4 rect;
    Vector4 uv;
    Vector4 color;
};

class LEIR_API UILabel : public UIElement {
public:
    UILabel();
    ~UILabel() override;

    void SetText(const std::string& text);
    const std::string& GetText() const { return m_Text; }

    void SetFont(Font* font) { m_Font = font; MarkDirty(); }
    Font* GetFont() const { return m_Font; }

    void SetFontSize(int size) { m_FontSize = size; MarkDirty(); }
    int GetFontSize() const { return m_FontSize; }

    enum class Alignment { Left, Center, Right };
    void SetAlignment(Alignment align) { m_Alignment = align; MarkDirty(); }
    Alignment GetAlignment() const { return m_Alignment; }

    void SetWordWrap(bool wrap) { m_WordWrap = wrap; MarkDirty(); }
    bool GetWordWrap() const { return m_WordWrap; }

    void SetMaxWidth(float w) { m_MaxWidth = w; MarkDirty(); }
    float GetMaxWidth() const { return m_MaxWidth; }

    Vector2 GetMinSize() const override;
    const std::vector<TextGlyphQuad>& GetGlyphQuads() const { return m_GlyphQuads; }
    void Invalidate() { MarkDirty(); }

protected:
    void OnLayoutComputed() override;

private:
    // m_Dirty drives the glyph rebuild; m_SizeValid drives the cached
    // natural size so layout (GetMinSize, called every frame) stays O(1)
    // instead of re-measuring all glyphs on every UpdateLayout pass.
    void MarkDirty() { m_Dirty = true; m_SizeValid = false; }
    void Rebuild();

    std::string m_Text;
    Font* m_Font = nullptr;
    int m_FontSize = 16;
    Alignment m_Alignment = Alignment::Left;
    bool m_WordWrap = false;
    float m_MaxWidth = 0.0f;
    std::vector<TextGlyphQuad> m_GlyphQuads;
    bool m_Dirty = true;
    mutable Vector2 m_CachedSize = {0.0f, 0.0f};
    mutable bool m_SizeValid = false;
};

} // namespace Leir
