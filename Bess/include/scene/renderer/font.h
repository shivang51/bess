#pragma once

#include "device.h"
#include "ft2build.h"
#include "glm.hpp"
#include "vulkan_texture.h"
#include <map>
#include <memory>
#include <string>

#include FT_FREETYPE_H

using namespace Bess::Vulkan;
namespace Bess::Renderer2D {
    class Font {
      public:
        struct Character {
            std::shared_ptr<VulkanTexture> Texture;
            glm::ivec2 Size;
            glm::ivec2 Bearing;
            int Advance;
        };

        Font() = default;

        ~Font();

        Font(const std::string &path);
        Font(const std::string &path, VulkanDevice &device);

        const Character &getCharacter(char ch);

        static float getScale(float size);

      private:
        FT_Library m_ft;
        FT_Face m_face;
        std::map<char, Character> Characters;
        VulkanDevice *m_device = nullptr;
        void loadCharacters();

        static const int m_defaultSize = 48;
    };
} // namespace Bess::Renderer2D
