#pragma once

#include "glyph_extractor.h"
#include <cstddef>
#include <string>

namespace Bess::Renderer::Font {
    class FontFile {
      public:
        FontFile() = default;
        FontFile(const std::string &path);

        FontFile(FontFile &&other) noexcept;
        FontFile &operator=(FontFile &&other) noexcept;

        FontFile(const FontFile &) = delete;
        FontFile &operator=(const FontFile &) = delete;

        void init(float fontSize, size_t glyphMin = 0, size_t glyphMax = 128);

        Glyph &getGlyph(char32_t ch);
        Glyph &getGlyph(const char *data);

        float getSize() const;

      private:
        Glyph &indexChar(char32_t ch);

        std::vector<Glyph> m_glyphsTable;
        float m_size = 0;
        size_t m_min = 0, m_max = 0;
        GlyphExtractor m_glyphExtractor;
        size_t m_glyphCount = 0;
    };
} // namespace Bess::Renderer::Font
