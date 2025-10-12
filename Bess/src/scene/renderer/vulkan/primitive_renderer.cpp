#include "scene/renderer/vulkan/primitive_renderer.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "camera.h"
#include "common/log.h"
#include "primitive_vertex.h"
#include "scene/renderer/font.h"
#include "scene/renderer/vulkan/text_renderer.h"
#include <memory>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D::Vulkan {

    PrimitiveRenderer::PrimitiveRenderer(const std::shared_ptr<VulkanDevice> &device,
                                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                         VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {

        m_circlePipeline = std::make_unique<Pipelines::CirclePipeline>(device, renderPass, extent);
        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(device, renderPass, extent);
        m_quadPipeline = std::make_unique<Pipelines::QuadPipeline>(device, renderPass, extent);
        // m_textPipeline = std::make_unique<Pipelines::TextPipeline>(device, renderPass, extent);

        m_textRenderer = std::make_unique<Renderer::TextRenderer>(device, renderPass, extent);
    }

    PrimitiveRenderer::~PrimitiveRenderer() = default;

    PrimitiveRenderer::PrimitiveRenderer(PrimitiveRenderer &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_renderPass(std::move(other.m_renderPass)),
          m_extent(other.m_extent),
          m_circlePipeline(std::move(other.m_circlePipeline)),
          m_gridPipeline(std::move(other.m_gridPipeline)),
          m_quadPipeline(std::move(other.m_quadPipeline)),
          m_textPipeline(std::move(other.m_textPipeline)),
          m_currentCommandBuffer(other.m_currentCommandBuffer) {
        other.m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    PrimitiveRenderer &PrimitiveRenderer::operator=(PrimitiveRenderer &&other) noexcept {
        if (this != &other) {
            m_device = std::move(other.m_device);
            m_renderPass = std::move(other.m_renderPass);
            m_extent = other.m_extent;
            m_circlePipeline = std::move(other.m_circlePipeline);
            m_gridPipeline = std::move(other.m_gridPipeline);
            m_quadPipeline = std::move(other.m_quadPipeline);
            m_textPipeline = std::move(other.m_textPipeline);
            m_currentCommandBuffer = other.m_currentCommandBuffer;

            other.m_currentCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void PrimitiveRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        m_textRenderer->beginFrame(commandBuffer);
        const auto msdfFont = Assets::AssetManager::instance().get(Assets::Fonts::robotoMsdf);
        updateTextUniforms({.pxRange = msdfFont->getPxRange()});
    }

    void PrimitiveRenderer::endFrame() {
        m_textRenderer->endFrame();
        if (m_circlePipeline) {
            m_circlePipeline->beginPipeline(m_currentCommandBuffer);
            m_circlePipeline->setCirclesData(m_opaqueCircleInstances, m_translucentCircleInstances);
            m_circlePipeline->endPipeline();
            m_opaqueCircleInstances.clear();
            m_translucentCircleInstances.clear();
        }

        if (m_textPipeline) {
            m_textPipeline->beginPipeline(m_currentCommandBuffer);
            m_textPipeline->setTextData(m_textInstances);
            m_textPipeline->endPipeline();
            m_textInstances.clear();
        }

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
        m_textRenderer->setCurrentFrameIndex(frameIndex);
        if (m_circlePipeline) {
            m_circlePipeline->setCurrentFrameIndex(frameIndex);
        }
        if (m_textPipeline) {
            m_textPipeline->setCurrentFrameIndex(frameIndex);
        }
        if (m_gridPipeline) {
            m_gridPipeline->setCurrentFrameIndex(frameIndex);
        }
        if (m_quadPipeline) {
            m_quadPipeline->setCurrentFrameIndex(frameIndex);
        }
    }

    void PrimitiveRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id,
                                     const GridColors &gridColors, const std::shared_ptr<Camera> &camera) {
        if (!m_gridPipeline) {
            BESS_WARN("[PrimitiveRenderer] Grid pipeline not available");
            return;
        }

        GridUniforms uniforms;
        uniforms.gridMajorColor = gridColors.majorColor;
        uniforms.gridMinorColor = gridColors.minorColor;
        uniforms.axisXColor = gridColors.axisXColor;
        uniforms.axisYColor = gridColors.axisYColor;

        const auto &camPos = camera->getPos();
        uniforms.cameraOffset = {-camPos.x, camPos.y};
        uniforms.resolution = camera->getSize();
        uniforms.zoom = camera->getZoom();

        m_gridPipeline->updateGridUniforms(uniforms);
        m_gridVertex = {pos, size, id};
    }

    void PrimitiveRenderer::drawQuad(const glm::vec3 &pos,
                                     const glm::vec2 &size,
                                     const glm::vec4 &color,
                                     int id,
                                     QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[PrimitiveRenderer] Quad pipeline not available");
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = (int)props.isMica;
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
                                             const std::shared_ptr<VulkanTexture> &texture,
                                             QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[PrimitiveRenderer] Quad pipeline not available");
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = (int)props.isMica;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);

        m_texturedQuadInstances[texture].emplace_back(instance);
    }

    void PrimitiveRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                             const glm::vec2 &size,
                                             const glm::vec4 &tint,
                                             int id,
                                             const std::shared_ptr<SubTexture> &subTexture,
                                             QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[PrimitiveRenderer] Quad pipeline not available");
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = id;
        instance.isMica = (int)props.isMica;
        instance.texData = subTexture->getStartWH();

        m_texturedQuadInstances[subTexture->getTexture()].emplace_back(instance);
    }

    void PrimitiveRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius) {
        if (!m_circlePipeline) {
            BESS_WARN("[PrimitiveRenderer] Circle pipeline not available");
            return;
        }

        CircleInstance instance{};
        instance.position = center;
        instance.color = color;
        instance.radius = radius;
        instance.innerRadius = innerRadius;
        instance.id = id;

        if (color.a == 1.f) {
            m_opaqueCircleInstances.emplace_back(instance);
        } else {
            m_translucentCircleInstances.emplace_back(instance);
        }
    }

    void PrimitiveRenderer::drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id) {
    }

    void PrimitiveRenderer::drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                     const glm::vec4 &color, const int id, float angle) {
        m_textRenderer->drawText(text, pos, size, color, id);
    }

    void PrimitiveRenderer::updateUBO(const UniformBufferObject &ubo) {
        m_ubo = ubo;
        if (m_circlePipeline) {
            m_circlePipeline->updateUniformBuffer(ubo);
        }
        if (m_textPipeline) {
            m_textPipeline->updateUniformBuffer(ubo);
        }
        if (m_gridPipeline) {
            m_gridPipeline->updateUniformBuffer(ubo);
        }
        if (m_quadPipeline) {
            m_quadPipeline->updateUniformBuffer(ubo);
        }

        m_textRenderer->updateUBO(ubo);
    }

    void PrimitiveRenderer::updateTextUniforms(const TextUniforms &textUniforms) {
        if (m_textPipeline) {
            m_textPipeline->updateTextUniforms(textUniforms);
        }
    }

    void PrimitiveRenderer::resize(VkExtent2D extent) {
        if (m_circlePipeline)
            m_circlePipeline->resize(extent);
        if (m_textPipeline)
            m_textPipeline->resize(extent);
        if (m_gridPipeline)
            m_gridPipeline->resize(extent);
        if (m_quadPipeline)
            m_quadPipeline->resize(extent);
        m_textRenderer->resize(extent);
    }

    glm::vec2 PrimitiveRenderer::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        return m_textRenderer->getRenderSize(str, (size_t)renderSize);
    }
} // namespace Bess::Renderer2D::Vulkan
