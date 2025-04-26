#pragma once

#include "ft2build.h"
#include "gl/texture.h"
#include "glm.hpp"
#include <map>
#include <string>

#include FT_FREETYPE_H

namespace Bess::Renderer2D {
    class Font {
      public:
        struct Character {
            Gl::Texture *Texture; // ID handle of the glyph texture
            glm::ivec2 Size;      // Size of glyph
            glm::ivec2 Bearing;   // Offset from baseline to left/top of glyph
            int Advance;          // Offset to advance to next glyph
        };

        Font() = default;

        ~Font();

        Font(const std::string &path);

        const Character &getCharacter(char ch);

        static float getScale(float size);

      private:
        FT_Library m_ft;
        FT_Face m_face;
        std::map<char, Character> Characters;
        void loadCharacters();

        static const int m_defaultSize = 48;
    };
} // namespace Bess::Renderer2D
