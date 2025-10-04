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
    }

    void PrimitiveRenderer::endFrame() {
        if (m_gridPipeline) {
            m_gridPipeline->beginPipeline(m_currentCommandBuffer);
            m_gridPipeline->drawGrid(m_gridVertex.position,
                                     m_gridVertex.texCoord, // storing size in this for now
                                     m_gridVertex.fragId);
            m_gridPipeline->endPipeline();
        }

        if (m_quadPipeline) {
            m_quadPipeline->beginPipeline(m_currentCommandBuffer);
            m_quadPipeline->setQuadsData(m_opaqueQuadInstances, m_translucentQuadInstances, m_texturedQuadInstances);
            m_quadPipeline->endPipeline();
            m_opaqueQuadInstances.clear();
            m_translucentQuadInstances.clear();
            m_texturedQuadInstances.clear();
        }

        m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    void PrimitiveRenderer::setCurrentFrameIndex(uint32_t frameIndex) {
        if (m_gridPipeline) {
            m_gridPipeline->setCurrentFrameIndex(frameIndex);
        }
        if (m_quadPipeline) {
            m_quadPipeline->setCurrentFrameIndex(frameIndex);
        }
    }

    void PrimitiveRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms) {
        if (!m_gridPipeline) {
            BESS_WARN("[PrimitiveRenderer] Grid pipeline not available");
            return;
        }

        m_gridPipeline->updateGridUniforms(gridUniforms);
        m_gridVertex = {pos, size, id};
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

        QuadInstance instance{};
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = borderRadius;
        instance.borderColor = borderColor;
        instance.borderSize = borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = isMica;
        instance.texSlotIdx = 0;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f); // Full local UV [0,1] by default

        if (color.a == 1.f) {
            m_opaqueQuadInstances.emplace_back(instance);
        } else {
            m_translucentQuadInstances.emplace_back(instance);
        }
    }

    void PrimitiveRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                             const glm::vec2 &size,
                                             const glm::vec4 &tint,
                                             int id,
                                             const glm::vec4 &borderRadius,
                                             const glm::vec4 &borderSize,
                                             const glm::vec4 &borderColor,
                                             int isMica,
                                             const std::shared_ptr<VulkanTexture> &texture) {
        if (!m_quadPipeline) {
            BESS_WARN("[PrimitiveRenderer] Quad pipeline not available");
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = borderRadius;
        instance.borderColor = borderColor;
        instance.borderSize = borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = isMica;
        instance.texSlotIdx = 1; // indicate textured
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);

        m_texturedQuadInstances[texture].emplace_back(instance);
    }

    void PrimitiveRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius) {
    }

    void PrimitiveRenderer::drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id) {
    }

    void PrimitiveRenderer::updateUniformBuffer(const GridUniforms &gridUniforms) {
        if (m_gridPipeline) {
            m_gridPipeline->updateGridUniforms(gridUniforms);
        }
    }

    void PrimitiveRenderer::updateUBO(const UniformBufferObject &ubo) {
        if (m_gridPipeline) {
            m_gridPipeline->updateUniformBuffer(ubo);
        }
        if (m_quadPipeline) {
            m_quadPipeline->updateUniformBuffer(ubo);
        }
    }

} // namespace Bess::Renderer2D::Vulkan
