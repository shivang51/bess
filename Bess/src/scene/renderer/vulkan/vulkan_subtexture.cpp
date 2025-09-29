#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "scene/renderer/vulkan/vulkan_texture.h"

namespace Bess::Renderer2D::Vulkan {

    VulkanSubTexture::VulkanSubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2& min, const glm::vec2& max)
        : m_texture(texture) {
        calculateTexCoords(min, max);
    }

    VulkanSubTexture::VulkanSubTexture(std::shared_ptr<VulkanTexture> texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize)
        : m_texture(texture) {
        glm::vec2 min = {coords.x * cellSize.x, coords.y * cellSize.y};
        glm::vec2 max = {(coords.x + spriteSize.x) * cellSize.x, (coords.y + spriteSize.y) * cellSize.y};
        calculateTexCoords(min, max);
    }

    VulkanSubTexture::VulkanSubTexture(VulkanSubTexture&& other) noexcept
        : m_texture(std::move(other.m_texture)) {
        for (int i = 0; i < 4; i++) {
            m_texCoords[i] = other.m_texCoords[i];
        }
    }

    VulkanSubTexture& VulkanSubTexture::operator=(VulkanSubTexture&& other) noexcept {
        if (this != &other) {
            m_texture = std::move(other.m_texture);
            for (int i = 0; i < 4; i++) {
                m_texCoords[i] = other.m_texCoords[i];
            }
        }
        return *this;
    }

    void VulkanSubTexture::calculateTexCoords(const glm::vec2& min, const glm::vec2& max) {
        m_texCoords[0] = {min.x, min.y};
        m_texCoords[1] = {max.x, min.y};
        m_texCoords[2] = {max.x, max.y};
        m_texCoords[3] = {min.x, max.y};
    }

    glm::vec2 VulkanSubTexture::getScale() const {
        const float width = static_cast<float>(m_texture->getWidth());
        const float height = static_cast<float>(m_texture->getHeight());
        if (width == 0.0f || height == 0.0f) {
            return {1.0f, 1.0f};
        }
        return {width, height};
    }

} // namespace Bess::Renderer2D::Vulkan
