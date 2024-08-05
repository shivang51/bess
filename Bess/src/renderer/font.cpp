#include "renderer/font.h"
#include <iostream>

namespace Bess::Renderer2D {
    Font::Font(const std::string& path) {
        if (FT_Init_FreeType(&m_ft))
        {
            std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            assert(false);
        }

        if (FT_New_Face(m_ft, path.c_str(), 0, &m_face))
        {
            std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
            assert(false);
        }

        FT_Set_Pixel_Sizes(m_face, 0, m_defaultSize);

        if (FT_Load_Char(m_face, 'X', FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
            assert(false);
        }

        loadCharacters();
        FT_Done_Face(m_face);
        FT_Done_FreeType(m_ft);
    }

    Font::~Font() {
        for (auto& ch : Characters) {
            delete ch.second.Texture;
        }
    }

    void Font::loadCharacters() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++)
        {
            if (FT_Load_Char(m_face, c, FT_LOAD_RENDER))
            {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
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
                m_face->glyph->advance.x
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }
    }

    const Font::Character& Font::getCharacter(char ch) {
        return Characters[ch];
    }

    float Font::getScale(float size)
    {
        return size / (float)m_defaultSize;
    }
}