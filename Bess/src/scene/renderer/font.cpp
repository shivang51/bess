#include "scene/renderer/font.h"
#include "common/log.h"

namespace Bess::Renderer2D {
    Font::Font(const std::string &path) {
        if (FT_Init_FreeType(&m_ft)) {
            BESS_ERROR("ERROR::FREETYPE: Could not init FreeType Library");
            assert(false);
        }

        if (FT_New_Face(m_ft, path.c_str(), 0, &m_face)) {
            BESS_ERROR("ERROR::FREETYPE: Failed to load font");
            assert(false);
        }

        FT_Set_Pixel_Sizes(m_face, 0, m_defaultSize);

        if (FT_Load_Char(m_face, 'X', FT_LOAD_RENDER)) {
            BESS_ERROR("ERROR::FREETYTPE: Failed to load Glyph");
            assert(false);
        }

        loadCharacters();
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_ft);
    }

    Font::~Font() {
        Characters.clear();
    }

    void Font::loadCharacters() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        FT_UInt glyphIdx;
        FT_ULong charCode = FT_Get_First_Char(m_face, &glyphIdx);

        if (glyphIdx == 0) {
            BESS_ERROR("ERROR::FREETYPE: No characters found in font");
            return;
        }

        while (glyphIdx != 0) {
            if (FT_Load_Char(m_face, charCode, FT_LOAD_RENDER)) {
                BESS_ERROR("ERROR::FREETYTPE: Failed to load Glyph");
                continue;
            }

            auto texture = std::make_shared<Gl::Texture>(
                GL_RGB,
                GL_RED,
                m_face->glyph->bitmap.width,
                m_face->glyph->bitmap.rows,
                m_face->glyph->bitmap.buffer);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

            Character character = {
                texture,
                glm::ivec2(m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows),
                glm::ivec2(m_face->glyph->bitmap_left, m_face->glyph->bitmap_top),
                (int)m_face->glyph->advance.x};
            Characters.insert(std::pair<char, Character>(charCode, character));
            charCode = FT_Get_Next_Char(m_face, charCode, &glyphIdx);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    const Font::Character &Font::getCharacter(char ch) {
        return Characters[ch];
    }

    float Font::getScale(float size) {
        return size / (float)m_defaultSize;
    }
} // namespace Bess::Renderer2D
