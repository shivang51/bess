#pragma once
#include "glm.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <string>
#include <vector>

namespace Bess::Renderer::Font {
    using Vec2 = glm::vec2;

    struct MoveTo {
        Vec2 p;
    };

    struct LineTo {
        Vec2 p;
    };

    struct QuadTo {
        Vec2 c, p;
    };

    struct CubicTo {
        Vec2 c1, c2, p;
    };

    struct PathCommand {
        enum class Kind : uint8_t { Move,
                                    Line,
                                    Quad,
                                    Cubic } kind;
        union {
            MoveTo move;
            LineTo line;
            QuadTo quad;
            CubicTo cubic;
        };
    };

    struct CharacterPath {
        std::vector<PathCommand> cmds;
        float advanceX = 0.0f;
        float advanceY = 0.0f;
    };

    struct Glyph {
        char32_t charCode{};
        CharacterPath path = {};
    };

    class GlyphExtractor {
      public:
        GlyphExtractor() = default;
        GlyphExtractor(const std::string &fontPath);
        ~GlyphExtractor();

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
        FT_Library m_ft = nullptr;
        FT_Face m_face = nullptr;
    };
} // namespace Bess::Renderer::Font
