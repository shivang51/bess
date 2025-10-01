#include "scene/renderer/vulkan_renderer.h"
#include "common/log.h"

namespace Bess::Renderer2D {
    std::shared_ptr<Vulkan::PrimitiveRenderer> VulkanRenderer::m_primitiveRenderer = nullptr;
    std::shared_ptr<Camera> VulkanRenderer::m_camera = nullptr;

    void VulkanRenderer::init() {
    }

    void VulkanRenderer::shutdown() {
        BESS_INFO("[VulkanRenderer] Shutting down Vulkan renderer");
        m_primitiveRenderer = nullptr;
        m_camera = nullptr;
    }

    void VulkanRenderer::beginScene(std::shared_ptr<Camera> camera) {
        m_camera = camera;
        m_primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer();
    }

    void VulkanRenderer::end() {
        // The actual frame end is handled by VulkanCore
        // This method is a placeholder
    }

    void VulkanRenderer::clearColor() {
    }

    void VulkanRenderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        if (!m_primitiveRenderer || !m_camera) {
            BESS_WARN("[VulkanRenderer] Cannot render grid - primitive renderer or camera not available");
            return;
        }

        const auto &camPos = m_camera->getPos();

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getTransform();
        ubo.ortho = m_camera->getOrtho();
        m_primitiveRenderer->updateMvp(ubo);

        Vulkan::GridUniforms gridUniforms{};
        gridUniforms.zoom = m_camera->getZoom();
        gridUniforms.cameraOffset = glm::vec2({-camPos.x, camPos.y});
        gridUniforms.gridMinorColor = colors.minorColor;
        gridUniforms.gridMajorColor = colors.majorColor;
        gridUniforms.axisXColor = colors.axisXColor;
        gridUniforms.axisYColor = colors.axisYColor;
        gridUniforms.resolution = glm::vec2(m_camera->getSize());

        m_primitiveRenderer->updateUniformBuffer(ubo, gridUniforms);

        m_primitiveRenderer->drawGrid(pos, size, id, gridUniforms);
    }

    void VulkanRenderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                              const glm::vec4 &color, int id, QuadRenderProperties properties) {
        if (!m_primitiveRenderer || !m_camera) {
            BESS_WARN("[VulkanRenderer] Cannot render quad - primitive renderer or camera not available");
            return;
        }

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getTransform();
        ubo.ortho = m_camera->getOrtho();
        m_primitiveRenderer->updateMvp(ubo);

        m_primitiveRenderer->drawQuad(
            pos,
            size,
            color,
            id,
            properties.borderRadius,
            properties.borderSize,
            properties.borderColor,
            properties.isMica ? 1 : 0);
    }
} // namespace Bess::Renderer2D
