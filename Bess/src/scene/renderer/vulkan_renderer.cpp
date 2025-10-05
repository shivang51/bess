#include "scene/renderer/vulkan_renderer.h"
#include "common/log.h"

namespace Bess::Renderer2D {
    std::shared_ptr<Camera> VulkanRenderer::m_camera = nullptr;

    void VulkanRenderer::init() {
    }

    void VulkanRenderer::shutdown() {
        BESS_INFO("[VulkanRenderer] Shutting down Vulkan renderer");
        m_camera = nullptr;
    }

    void VulkanRenderer::beginScene(std::shared_ptr<Camera> camera) {
        m_camera = camera;
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getTransform();
        ubo.ortho = m_camera->getOrtho();
        primitiveRenderer->updateUBO(ubo);
    }

    void VulkanRenderer::end() {
    }

    void VulkanRenderer::clearColor() {
    }

    void VulkanRenderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        const auto &camPos = m_camera->getPos();

        Vulkan::GridUniforms gridUniforms{};
        gridUniforms.zoom = m_camera->getZoom();
        gridUniforms.cameraOffset = glm::vec2({-camPos.x, camPos.y});
        gridUniforms.gridMinorColor = colors.minorColor;
        gridUniforms.gridMajorColor = colors.majorColor;
        gridUniforms.axisXColor = colors.axisXColor;
        gridUniforms.axisYColor = colors.axisYColor;
        gridUniforms.resolution = glm::vec2(m_camera->getSize());

        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->updateUniformBuffer(gridUniforms);
        primitiveRenderer->drawGrid(pos, size, id, gridUniforms);
    }

    void VulkanRenderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                              const glm::vec4 &color, int id, QuadRenderProperties properties) {
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->drawQuad(
            pos,
            size,
            color,
            id,
            properties.borderRadius,
            properties.borderSize,
            properties.borderColor,
            properties.isMica ? 1 : 0);
    }

    void VulkanRenderer::texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                      const std::shared_ptr<Vulkan::VulkanTexture> &texture,
                                      const glm::vec4 &tintColor, int id, QuadRenderProperties properties) {
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->drawTexturedQuad(
            pos,
            size,
            tintColor,
            id,
            properties.borderRadius,
            properties.borderSize,
            properties.borderColor,
            properties.isMica ? 1 : 0,
            texture);
    }

    void VulkanRenderer::texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                      const std::shared_ptr<Vulkan::SubTexture> &texture,
                                      const glm::vec4 &tintColor, int id, QuadRenderProperties properties) {
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->drawTexturedQuad(
            pos,
            size,
            tintColor,
            id,
            properties.borderRadius,
            properties.borderSize,
            properties.borderColor,
            properties.isMica ? 1 : 0,
            texture);
    }
} // namespace Bess::Renderer2D
