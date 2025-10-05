#pragma once

#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "scene/renderer/vulkan/vulkan_texture.h"

#include "glm.hpp"
#include "json/json.h"
#include <memory>
#include <string>

namespace Bess::Renderer2D::Vulkan {
    class VulkanDevice;
}

namespace Bess::Renderer2D {

    struct MsdfCharacter {
        char character;
        glm::vec2 offset;
        glm::vec2 size;
        float advance;
        std::shared_ptr<Vulkan::SubTexture> subTexture;
    };

    class MsdfFont {
      public:
        MsdfFont() = default;

        // Constructor that loads the font from texture atlas
        // path is the path of the json file with character data
        MsdfFont(const std::string &path, const std::string &jsonFileName, std::shared_ptr<Vulkan::VulkanDevice> device);

        ~MsdfFont();

        // Loads the font from texture atlas
        // path is the path of the json file with character data
        void loadFont(const std::string &path, const std::string &jsonFileName, std::shared_ptr<Vulkan::VulkanDevice> device);

        float getScale(float size) const;

        float getLineHeight() const;

        const MsdfCharacter &getCharacterData(char c) const;

        std::shared_ptr<Vulkan::VulkanTexture> getTextureAtlas() const;

      private:
        bool isValidJson(const Json::Value &json) const;

        struct Bounds {
            float left;
            float bottom;
            float right;
            float top;
        };

        Bounds getBounds(const Json::Value &val) const;

        std::shared_ptr<Vulkan::VulkanTexture> m_fontTextureAtlas;
        std::vector<MsdfCharacter> m_charTable;
        float m_fontSize = 0.f, m_lineHeight = 0.f;
        std::shared_ptr<Vulkan::VulkanDevice> m_device = nullptr;
    };
} // namespace Bess::Renderer2D
