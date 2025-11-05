#pragma once
#include "path.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <string>
#include <vector>

namespace Bess::Renderer::Font {
    struct Glyph {
        char32_t charCode{};
        Path path;
        float advanceX = 0.0f;
        float advanceY = 0.0f;
    };

    class GlyphExtractor {
      public:
        GlyphExtractor() = default;
        GlyphExtractor(const std::string &fontPath);
        ~GlyphExtractor();

        GlyphExtractor(GlyphExtractor &&other) noexcept;

        GlyphExtractor &operator=(GlyphExtractor &&other) noexcept;

        GlyphExtractor(const GlyphExtractor &) = delete;
        GlyphExtractor &operator=(const GlyphExtractor &) = delete;

        size_t getGlyphsCount();

        bool isValid() const { return m_face != nullptr; }

        bool setPixelSize(int pixelHeight);

        bool extractGlyph(char32_t codepoint, Glyph &out, bool yDown = true);
        bool extractGlyph(const char *codepoint, Glyph &out, bool yDown = true);

        float ascent() const;
        float descent() const;
        float lineHeight() const;

        struct OutlineCollector;

        static char32_t decodeSingleUTF8(std::string_view utf8);

      private:
        FT_Face m_face = nullptr;
        static FT_Library s_ftLibrary;
        static int s_instCount;
    };
} // namespace Bess::Renderer::Font
