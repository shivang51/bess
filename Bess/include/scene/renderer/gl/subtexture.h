#pragma once

#include "texture.h"
#include "glm.hpp"
#include <memory>

namespace Bess::Gl {
    class SubTexture {
      public:

        SubTexture() = default;
        SubTexture(std::shared_ptr<Texture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize);

        SubTexture(std::shared_ptr<Texture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize,
            float margin, glm::vec2 cellSize);

        const std::vector<glm::vec2> &getTexCoords() const;

        std::shared_ptr<Texture> getTexture();

        void calcCoordsFrom(const glm::vec2 &pos, const glm::vec2& size);

        private:
            void calculateCoords();

        private:
            std::shared_ptr<Texture> m_texture;
            glm::vec2 m_coord;
            glm::vec2 m_spriteSize;
            float m_margin;
            glm::vec2 m_cellSize;
            std::vector<glm::vec2> m_texCoords;
    };
}