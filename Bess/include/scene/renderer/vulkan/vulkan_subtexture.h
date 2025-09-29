#pragma once

#include <memory>
#include "glm.hpp"

namespace Bess::Renderer2D::Vulkan {

    class VulkanTexture;

    class VulkanSubTexture {
    public:
        VulkanSubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2& min, const glm::vec2& max);
        VulkanSubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize);

        // Delete copy constructor and assignment operator
        VulkanSubTexture(const VulkanSubTexture&) = delete;
        VulkanSubTexture& operator=(const VulkanSubTexture&) = delete;

        // Move constructor and assignment operator
        VulkanSubTexture(VulkanSubTexture&& other) noexcept;
        VulkanSubTexture& operator=(VulkanSubTexture&& other) noexcept;

        std::shared_ptr<VulkanTexture> getTexture() const { return m_texture; }
        const glm::vec2* getTexCoords() const { return m_texCoords; }
        glm::vec2 getScale() const;
        const glm::vec2& getMin() const { return m_texCoords[0]; }
        const glm::vec2& getMax() const { return m_texCoords[2]; }

    private:
        std::shared_ptr<VulkanTexture> m_texture;
        glm::vec2 m_texCoords[4];

        void calculateTexCoords(const glm::vec2& min, const glm::vec2& max);
    };

} // namespace Bess::Renderer2D::Vulkan
