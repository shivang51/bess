#pragma once

#include "scene/renderer/gl/subtexture.h"
#include "scene/renderer/gl/texture.h"

#include "json/json.h"
#include <memory>
#include <string>

namespace Bess::Renderer2D {

    struct MsdfCharacter {
        char character;
        glm::vec2 offset;
        glm::vec2 size;
        float advance;
        std::shared_ptr<Gl::SubTexture> subTexture;
    };

    class MsdfFont {
      public:
        MsdfFont() = default;

        // Constructor that loads the font from texture atlas
        // path is the path of the json file with character data
        MsdfFont(const std::string &path, const std::string &jsonFileName);

        ~MsdfFont();

        // Loads the font from texture atlas
        // path is the path of the json file with character data
        void loadFont(const std::string &path, const std::string &jsonFileName);

        float getScale(float size) const;

        float getLineHeight() const;

        const MsdfCharacter &getCharacterData(char c) const;

        std::shared_ptr<Gl::Texture> getTextureAtlas() const;

      private:
        bool isValidJson(const Json::Value &json);

        struct Bounds {
            float left;
            float bottom;
            float right;
            float top;
        };

        Bounds getBounds(const Json::Value &val);

        std::shared_ptr<Gl::Texture> m_fontTextureAtlas;
        std::vector<MsdfCharacter> m_charTable;
        float m_fontSize = 0.f, m_lineHeight = 0.f;
    };
} // namespace Bess::Renderer2D
