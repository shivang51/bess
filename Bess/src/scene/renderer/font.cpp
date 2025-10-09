#include "scene/renderer/font.h"
#include "common/log.h"
#include "vulkan_texture.h"

namespace Bess::Renderer2D {
    Font::Font(const std::string &path) {
        // This constructor is deprecated - use the one with device parameter
        BESS_WARN("Font constructor without device is deprecated. Use Font(path, device) instead.");
    }

    Font::Font(const std::string &path, Vulkan::VulkanDevice& device) : m_device(&device) {
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
        if (!m_device) {
            BESS_ERROR("ERROR::FONT: Vulkan device not available for texture creation");
            return;
        }

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

            // Create Vulkan texture from bitmap data
            // For now, we'll create a placeholder texture since we need to implement
            // texture creation from raw bitmap data in VulkanTexture class
            // const auto texture = std::make_shared<Vulkan::VulkanTexture>(*m_device, "");

            // Character character = {
            //     texture,
            //     glm::ivec2(m_face->glyph->bitmap.width, m_face->glyph->bitmap.rows),
            //     glm::ivec2(m_face->glyph->bitmap_left, m_face->glyph->bitmap_top),
            //     (int)m_face->glyph->advance.x};
            // Characters.insert(std::pair<char, Character>(charCode, character));
            // charCode = FT_Get_Next_Char(m_face, charCode, &glyphIdx);
        }
    }

    const Font::Character &Font::getCharacter(char ch) {
        return Characters[ch];
    }

    float Font::getScale(float size) {
        return size / (float)m_defaultSize;
    }
} // namespace Bess::Renderer2D
