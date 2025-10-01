#include "scene/renderer/vulkan/primitive_renderer.h"
#include "common/log.h"

namespace Bess::Renderer2D::Vulkan {

    PrimitiveRenderer::PrimitiveRenderer(const std::shared_ptr<VulkanDevice> &device,
                                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                         VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        
        // Create pipeline instances
        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(device, renderPass, extent);
        m_quadPipeline = std::make_unique<Pipelines::QuadPipeline>(device, renderPass, extent);
    }

    PrimitiveRenderer::~PrimitiveRenderer() {
        // Pipeline destructors will handle cleanup
    }

    PrimitiveRenderer::PrimitiveRenderer(PrimitiveRenderer &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_renderPass(std::move(other.m_renderPass)),
          m_extent(other.m_extent),
          m_gridPipeline(std::move(other.m_gridPipeline)),
          m_quadPipeline(std::move(other.m_quadPipeline)),
          m_currentCommandBuffer(other.m_currentCommandBuffer) {
        other.m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    PrimitiveRenderer &PrimitiveRenderer::operator=(PrimitiveRenderer &&other) noexcept {
        if (this != &other) {
            m_device = std::move(other.m_device);
            m_renderPass = std::move(other.m_renderPass);
            m_extent = other.m_extent;
            m_gridPipeline = std::move(other.m_gridPipeline);
            m_quadPipeline = std::move(other.m_quadPipeline);
            m_currentCommandBuffer = other.m_currentCommandBuffer;

            other.m_currentCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void PrimitiveRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        
        // Begin both pipelines
        if (m_gridPipeline) {
            m_gridPipeline->beginPipeline(commandBuffer);
        }
        if (m_quadPipeline) {
            m_quadPipeline->beginPipeline(commandBuffer);
        }
    }

    void PrimitiveRenderer::endFrame() {
        // End both pipelines
        if (m_gridPipeline) {
            m_gridPipeline->endPipeline();
        }
        if (m_quadPipeline) {
            m_quadPipeline->endPipeline();
        }
        
        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void PrimitiveRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms) {
        if (!m_gridPipeline) {
            BESS_WARN("[PrimitiveRenderer] Grid pipeline not available");
            return;
        }
        
        m_gridPipeline->updateGridUniforms(gridUniforms);
        m_gridPipeline->drawGrid(pos, size, id, gridUniforms);
    }

    void PrimitiveRenderer::drawQuad(const glm::vec3 &pos,
                                     const glm::vec2 &size,
                                     const glm::vec4 &color,
                                     int id,
                                     const glm::vec4 &borderRadius,
                                     const glm::vec4 &borderSize,
                                     const glm::vec4 &borderColor,
                                     int isMica) {
        if (!m_quadPipeline) {
            BESS_WARN("[PrimitiveRenderer] Quad pipeline not available");
            return;
        }
        
        m_quadPipeline->drawQuad(pos, size, color, id, borderRadius, borderSize, borderColor, isMica);
    }

    void PrimitiveRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius) {
        // Implementation for circle rendering
        // This is a placeholder - you can implement actual circle rendering here
    }

    void PrimitiveRenderer::drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id) {
        // Implementation for line rendering
        // This is a placeholder - you can implement actual line rendering here
    }

    void PrimitiveRenderer::updateUniformBuffer(const UniformBufferObject &ubo, const GridUniforms &gridUniforms) {
        if (m_gridPipeline) {
            m_gridPipeline->updateUniformBuffer(ubo);
            m_gridPipeline->updateGridUniforms(gridUniforms);
        }
    }

    void PrimitiveRenderer::updateMvp(const UniformBufferObject &ubo) {
        if (m_quadPipeline) {
            m_quadPipeline->updateUniformBuffer(ubo);
        }
    }

} // namespace Bess::Renderer2D::Vulkan