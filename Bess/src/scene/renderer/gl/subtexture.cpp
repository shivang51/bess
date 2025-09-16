#include "scene/renderer/gl/subtexture.h"

namespace Bess::Gl {

    SubTexture::SubTexture(std::shared_ptr<Texture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize)
        : m_texture(std::move(texture)), m_coord(coord), m_spriteSize(spriteSize), m_margin(0.f), m_cellSize({1.f, 1.f}) {
        calculateCoords();
    }

    SubTexture::SubTexture(std::shared_ptr<Texture> texture, const glm::vec2 &coord, const glm::vec2 &spriteSize,
                           float margin, const glm::vec2 &cellSize)
        : m_texture(std::move(texture)), m_coord(coord), m_spriteSize(spriteSize), m_margin(margin), m_cellSize(cellSize) {
        calculateCoords();
    }

    const std::vector<glm::vec2> &SubTexture::getTexCoords() const {
        return m_texCoords;
    }

    const glm::vec4 &SubTexture::getStartWH() const {
        return m_startWH;
    }

    glm::vec2 SubTexture::getScale() const {
        return m_spriteSize;
    }

    std::shared_ptr<Texture> SubTexture::getTexture() {
        return m_texture;
    }

    void SubTexture::calculateCoords() {
        float startX = m_coord.x * (m_spriteSize.x + m_margin);
        float startY = m_coord.y * (m_spriteSize.y + m_margin);
        float texSizeX = m_spriteSize.x * m_cellSize.x;
        float texSizeY = m_spriteSize.y * m_cellSize.y;
        glm::vec2 texOffset = {startX / m_texture->getWidth(), startY / m_texture->getHeight()};
        glm::vec2 texSize = {texSizeX / m_texture->getWidth(), texSizeY / m_texture->getHeight()};

        m_startWH = {texOffset.x, texOffset.y, texSize.x, texSize.y};

        m_texCoords = {
            {texOffset.x, texOffset.y + texSize.y},
            {texOffset.x, texOffset.y},
            {texOffset.x + texSize.x, texOffset.y},
            {texOffset.x + texSize.x, texOffset.y + texSize.y}};
    }

    void SubTexture::calcCoordsFrom(std::shared_ptr<Gl::Texture> tex, const glm::vec2 &pos, const glm::vec2 &size) {
        float startX = pos.x;
        float startY = pos.y;
        float texSizeX = size.x;
        float texSizeY = size.y;
        m_texture = tex;
        glm::vec2 texOffset = {startX / m_texture->getWidth(), startY / m_texture->getHeight()};
        glm::vec2 texSize = {texSizeX / m_texture->getWidth(), texSizeY / m_texture->getHeight()};
        m_startWH = {texOffset.x, texOffset.y, texSize.x, texSize.y};
        m_texCoords = {
            {texOffset.x, texOffset.y + texSize.y},
            {texOffset.x, texOffset.y},
            {texOffset.x + texSize.x, texOffset.y},
            {texOffset.x + texSize.x, texOffset.y + texSize.y}};
    }
} // namespace Bess::Gl
