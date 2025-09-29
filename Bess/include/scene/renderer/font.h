#pragma once

#include "ft2build.h"
#include "glm.hpp"
#include "scene/renderer/vulkan/vulkan_texture.h"
#include <map>
#include <string>
#include <memory>

#include FT_FREETYPE_H

namespace Bess::Renderer2D::Vulkan {
    class VulkanDevice;
}

namespace Bess::Renderer2D {
    class Font {
      public:
        struct Character {
            std::shared_ptr<Vulkan::VulkanTexture> Texture; 
            glm::ivec2 Size;     
            glm::ivec2 Bearing;  
            int Advance;         
        };

        Font() = default;

        ~Font();

        Font(const std::string &path);
        Font(const std::string &path, Vulkan::VulkanDevice& device);

        const Character &getCharacter(char ch);

        static float getScale(float size);

      private:
        FT_Library m_ft;
        FT_Face m_face;
        std::map<char, Character> Characters;
        Vulkan::VulkanDevice* m_device = nullptr;
        void loadCharacters();

        static const int m_defaultSize = 48;
    };
} // namespace Bess::Renderer2D
