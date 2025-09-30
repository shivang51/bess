#include "scene/renderer/vulkan_renderer.h"
#include "common/log.h"

namespace Bess::Renderer2D {

    std::shared_ptr<Vulkan::PrimitiveRenderer> VulkanRenderer::m_primitiveRenderer = nullptr;
    std::shared_ptr<Camera> VulkanRenderer::m_camera = nullptr;

    void VulkanRenderer::init() {
        BESS_INFO("[VulkanRenderer] Initializing Vulkan renderer");
        // The primitive renderer is already initialized in VulkanCore
        // We just need to get a reference to it
        m_primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer();
        if (!m_primitiveRenderer) {
            BESS_ERROR("[VulkanRenderer] Failed to get primitive renderer from VulkanCore");
        }
    }

    void VulkanRenderer::shutdown() {
        BESS_INFO("[VulkanRenderer] Shutting down Vulkan renderer");
        m_primitiveRenderer = nullptr;
        m_camera = nullptr;
    }

    void VulkanRenderer::begin(std::shared_ptr<Camera> camera) {
        m_camera = camera;
        // Refresh primitive renderer in case it was recreated (e.g., on resize)
        m_primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer();
        // The actual frame begin is handled by VulkanCore
        // This method just stores the camera for later use
    }

    void VulkanRenderer::end() {
        // The actual frame end is handled by VulkanCore
        // This method is a placeholder
    }

    void VulkanRenderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        if (!m_primitiveRenderer || !m_camera) {
            BESS_WARN("[VulkanRenderer] Cannot render grid - primitive renderer or camera not available");
            return;
        }

        const auto &camPos = m_camera->getPos();

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getOrtho();

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

} // namespace Bess::Renderer2D
