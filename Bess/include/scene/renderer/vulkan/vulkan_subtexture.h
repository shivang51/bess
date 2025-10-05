#pragma once

#include "glm.hpp"
#include <memory>
#include <vector>

namespace Bess::Renderer2D::Vulkan {

    class VulkanTexture;

    class SubTexture {
      public:
        SubTexture() = default;
        SubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize);

        SubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize,
                   float margin, const glm::vec2 &cellSize);

        const glm::vec4 &getStartWH() const;

        glm::vec2 getScale() const;

        const std::vector<glm::vec2> &getTexCoords() const;

        std::shared_ptr<VulkanTexture> getTexture();

        void calcCoordsFrom(std::shared_ptr<VulkanTexture> tex, const glm::vec2 &pos, const glm::vec2 &size);

      private:
        void calculateCoords();

      private:
        std::shared_ptr<VulkanTexture> m_texture;
        glm::vec2 m_coord;
        glm::vec2 m_spriteSize;
        float m_margin;
        glm::vec2 m_cellSize;
        std::vector<glm::vec2> m_texCoords;
        glm::vec4 m_startWH;
    };

} // namespace Bess::Renderer2D::Vulkan
