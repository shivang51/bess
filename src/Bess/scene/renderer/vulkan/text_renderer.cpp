#include "scene/renderer/vulkan/text_renderer.h"
#include "asset_manager/asset_manager.h"
#include "application/assets.h"
#include "scene/renderer/font.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include <cstdint>

using namespace Bess::Vulkan;
namespace Bess::Renderer {

    TextRenderer::TextRenderer(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        m_pathRenderer = std::make_unique<Renderer2D::Vulkan::PathRenderer>(device, renderPass, extent);

        constexpr auto robotoPath = Assets::Fonts::Paths::roboto.paths[0].data();
        m_font = std::move(Font::FontFile(robotoPath));
        m_font.init(48);
    }

    void TextRenderer::drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                const glm::vec4 &color, const int id, float angle) {
        float scale = (float)size / m_font.getSize();
        float posX = pos.x, posY = pos.y;
        for (const char ch : text) {
            if (ch == '\n') {
                posY += m_font.getSize(), posX = 0.f;
                continue;
            }

            auto &glyph = m_font.getGlyph(ch);
            m_pathRenderer->drawPath(glyph.path, {.genStroke = false,
                                                  .genFill = true,
                                                  .translate = {posX, posY, pos.z},
                                                  .scale = glm::vec2(scale),
                                                  .fillColor = color,
                                                  .glyphId = id});
            posX += glyph.advanceX * scale;
        }
    }

    glm::vec2 TextRenderer::drawTextWrapped(const std::string &text, const glm::vec3 &pos, size_t size,
                                            const glm::vec4 &color, int id, float wrapWidthPx, float angle) {
        float scale = (float)size / m_font.getSize();
        float posX = pos.x, posY = pos.y;
        float widthUsed = 0.f, heightUsed = 0;
        float maxWidth = 0.f;
        for (const char ch : text) {
            if (ch == '\n' || (widthUsed >= wrapWidthPx && ch == ' ')) {
                posY += m_font.getSize() * scale, posX = pos.x;
                maxWidth = std::max(maxWidth, widthUsed);
                heightUsed += m_font.getSize() * scale;
                widthUsed = 0.f;
                continue;
            }

            auto &glyph = m_font.getGlyph(ch);
            m_pathRenderer->drawPath(glyph.path, {.genStroke = false,
                                                  .genFill = true,
                                                  .translate = {posX, posY, pos.z},
                                                  .scale = glm::vec2(scale),
                                                  .fillColor = color,
                                                  .glyphId = id});
            posX += glyph.advanceX * scale;
            widthUsed += glyph.advanceX * scale;
        }
        widthUsed = std::max(maxWidth, widthUsed);
        heightUsed += m_font.getSize() * scale;
        return {widthUsed, heightUsed};
    }

    glm::vec2 TextRenderer::getRenderSize(const std::string &text, size_t size) {
        float scale = (float)size / m_font.getSize();

        glm::vec2 renderSize = {0.f, 0.f};
        for (const char ch : text) {
            const auto &glyph = m_font.getGlyph(ch);
            renderSize.x += glyph.advanceX;
        }

        renderSize.x *= scale;
        renderSize.y = (float)size;
        return renderSize;
    }

    void TextRenderer::resize(VkExtent2D size) {
        m_pathRenderer->resize(size);
        m_extent = size;
    }

    void TextRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_pathRenderer->beginFrame(commandBuffer);
    }

    void TextRenderer::endFrame() {
        m_pathRenderer->endFrame();
    }

    void TextRenderer::setCurrentFrameIndex(uint32_t frameIndex) {
        m_pathRenderer->setCurrentFrameIndex(frameIndex);
    }

    void TextRenderer::updateUBO(const UniformBufferObject &ubo) {
        m_pathRenderer->updateUniformBuffer(ubo);
    }
} // namespace Bess::Renderer
