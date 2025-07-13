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
        for (auto &ch : Characters) {
            delete ch.second.Texture;
        }
    }

    void Font::loadCharacters() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(m_face, c, FT_LOAD_RENDER)) {
                BESS_ERROR("ERROR::FREETYTPE: Failed to load Glyph");
                continue;
            }

            auto texture = new Gl::Texture(
                GL_RED,
                GL_RED,
                m_face->glyph->bitmap.width,
                m_face->glyph->bitmap.rows,
                m_face->glyph->bitmap.buffer);

            Character character = {
                texture,
                glm::ivec2(m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows),
                glm::ivec2(m_face->glyph->bitmap_left, m_face->glyph->bitmap_top),
                (int)m_face->glyph->advance.x};
            Characters.insert(std::pair<char, Character>(c, character));
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
