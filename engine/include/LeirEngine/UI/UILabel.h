#pragma once

/**
 * @file UILabel.h
 * @brief Text label widget with font, alignment and word-wrap.
 * @ingroup UI
 */

#include "LeirEngine/Core/Export.h"
#include "LeirEngine/UI/UIElement.h"
#include "LeirEngine/Math/Vector2.h"
#include "LeirEngine/Math/Vector4.h"
#include <string>
#include <vector>

namespace Leir {

class Font;

/**
 * @brief One glyph quad produced by label layout: rectangle, UV and color.
 * @ingroup UI
 */
struct LEIR_API TextGlyphQuad {
    Vector4 rect;  ///< Glyph rectangle (x,y,w,h) in logical pixels.
    Vector4 uv;    ///< Glyph UV rectangle (u,v,w,h) in atlas space.
    Vector4 color; ///< Glyph color (tint).
};

/**
 * @brief Text label: lays out text into glyph quads using a Font.
 * @ingroup UI
 * @details Supports alignment, word-wrap and a max width. Glyphs are rebuilt
 *  lazily (MarkDirty) and the natural size is cached for O(1) layout.
 */
class LEIR_API UILabel : public UIElement {
public:
    /**
     * @brief Constructs an empty label.
     */
    UILabel();

    /**
     * @brief Destroys the label.
     */
    ~UILabel() override;

    /**
     * @brief Sets the text to display.
     * @param[in] text UTF-8 text.
     */
    void SetText(const std::string& text);

    /**
     * @brief Returns the current text.
     * @return Text string.
     */
    const std::string& GetText() const { return m_Text; }

    /**
     * @brief Sets the font.
     * @param[in] font Font pointer (must outlive the label).
     */
    void SetFont(Font* font) { m_Font = font; MarkDirty(); }

    /**
     * @brief Returns the font.
     * @return Font pointer or nullptr.
     */
    Font* GetFont() const { return m_Font; }

    /**
     * @brief Sets the font size.
     * @param[in] size Font size in logical pixels.
     */
    void SetFontSize(int size) { m_FontSize = size; MarkDirty(); }

    /**
     * @brief Returns the font size.
     * @return Font size.
     */
    int GetFontSize() const { return m_FontSize; }

    /**
     * @brief Horizontal alignment for multi-line text.
     * @ingroup UI
     */
    enum class Alignment { Left, ///< Left aligned.
                           Center, ///< Centered.
                           Right ///< Right aligned.
    };

    /**
     * @brief Sets text alignment.
     * @param[in] align Alignment.
     */
    void SetAlignment(Alignment align) { m_Alignment = align; MarkDirty(); }

    /**
     * @brief Returns the alignment.
     * @return Current alignment.
     */
    Alignment GetAlignment() const { return m_Alignment; }

    /**
     * @brief Enables word-wrap.
     * @param[in] wrap True to wrap.
     */
    void SetWordWrap(bool wrap) { m_WordWrap = wrap; MarkDirty(); }

    /**
     * @brief Returns whether word-wrap is enabled.
     * @return True if wrapping.
     */
    bool GetWordWrap() const { return m_WordWrap; }

    /**
     * @brief Sets the max width for word-wrap (0 = unlimited).
     * @param[in] w Max width in logical pixels.
     */
    void SetMaxWidth(float w) { m_MaxWidth = w; MarkDirty(); }

    /**
     * @brief Returns the max width.
     * @return Max width.
     */
    float GetMaxWidth() const { return m_MaxWidth; }

    /**
     * @brief Returns the minimum size (natural text extent) for layout.
     * @return Minimum size.
     */
    Vector2 GetMinSize() const override;

    /**
     * @brief Returns the built glyph quads for rendering.
     * @return Vector of glyph quads.
     */
    const std::vector<TextGlyphQuad>& GetGlyphQuads() const { return m_GlyphQuads; }

    /**
     * @brief Marks the label dirty (forces rebuild on next layout).
     */
    void Invalidate() { MarkDirty(); }

protected:
    /**
     * @brief Called after ComputeLayout to rebuild glyphs if dirty.
     */
    void OnLayoutComputed() override;

private:
    void MarkDirty() { m_Dirty = true; m_SizeValid = false; }
    void Rebuild();

    std::string m_Text;                               ///< Text content.
    Font* m_Font = nullptr;                           ///< Font (not owned).
    int m_FontSize = 16;                              ///< Font size.
    Alignment m_Alignment = Alignment::Left;          ///< Alignment.
    bool m_WordWrap = false;                          ///< Word-wrap flag.
    float m_MaxWidth = 0.0f;                          ///< Max width for wrap.
    std::vector<TextGlyphQuad> m_GlyphQuads;          ///< Built glyph quads.
    bool m_Dirty = true;                              ///< Dirty flag.
    mutable Vector2 m_CachedSize = {0.0f, 0.0f};       ///< Cached natural size.
    mutable bool m_SizeValid = false;                 ///< Cache valid flag.
    uint32_t m_FontGen = 0;                           ///< Font atlas generation at build time.
};

} // namespace Leir
