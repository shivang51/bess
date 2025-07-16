#pragma once

#include "scene/renderer/gl/texture.h"
#include "scene/renderer/gl/subtexture.h"

#include <string>
#include <memory>

namespace Bess::Renderer2D {

    struct MsdfCharacter{
        char character;
        glm::vec2 offset;
        glm::vec2 size;
        glm::vec2 texPos;
        float advance;
        std::shared_ptr<Gl::SubTexture> subTexture;
    };

    class MsdfFont {
        public:
            MsdfFont() = default;

            // Constructor that loads the font from texture atlas
            // path is the path of the json file with character data
            MsdfFont(const std::string &path, const std::string& jsonFileName, float fontSize = 32.f);

            ~MsdfFont();

            // Loads the font from texture atlas
            // path is the path of the json file with character data
            void loadFont(const std::string &path, const std::string& jsonFileName);

            float getScale(float size) const;

            MsdfCharacter getCharacterData(char c) const;

            std::shared_ptr<Gl::Texture> getTextureAtlas() const;

        private:
            std::shared_ptr<Gl::Texture> m_fontTextureAtlas;
            std::unordered_map<char, MsdfCharacter> m_charData = {};
            float m_fontSize;
    };
}