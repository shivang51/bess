#include "scene/renderer/material_renderer.h"
#include "common/log.h"
#include "primitive_vertex.h"
#include "scene/renderer/material.h"
#include <queue>
#include <unordered_map>

namespace Bess::Renderer {
    MaterialRenderer::MaterialRenderer(const std::shared_ptr<VulkanDevice> &device,
                                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                       VkExtent2D extent) {

        m_circlePipeline = std::make_unique<Pipelines::CirclePipeline>(device, renderPass, extent);
        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(device, renderPass, extent);
        m_quadPipeline = std::make_unique<Pipelines::QuadPipeline>(device, renderPass, extent);
        m_textRenderer = std::make_unique<Renderer::TextRenderer>(device, renderPass, extent);

        m_translucentMaterials = {};
    }

    MaterialRenderer::~MaterialRenderer() = default;

    void MaterialRenderer::drawMaterial(const Material2D &material) {
        if (material.alpha != 1.f) {
            m_translucentMaterials.push(material);
            return;
        }
    }

    void MaterialRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id,
                                    const GridColors &gridColors, const std::shared_ptr<Camera> &camera) {
        if (!m_gridPipeline) {
            BESS_WARN("[MaterialRenderer] Grid pipeline not available");
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

        Material2D m{
            Material2D::MaterialType::quad,
            pos.z,
            0.f};
        m.grid.uniforms = uniforms;
        m.grid.vertex = {pos, size, id};
    }

    static Material2D makeQuad(const Vulkan::QuadInstance &instance) {
        Material2D m;
        m.type = Material2D::MaterialType::quad;
        new (&m.quad) QuadMaterial();
        m.quad.instance = instance;
        m.z = instance.position.z;
        m.alpha = instance.color.a;
        return m;
    }

    static Material2D makeCircle(const Vulkan::CircleInstance &instance) {
        Material2D m;
        m.type = Material2D::MaterialType::circle;
        new (&m.circle) CircleMaterial();
        m.circle.instance = instance;
        m.z = instance.position.z;
        m.alpha = instance.color.a;
        return m;
    }
    void MaterialRenderer::drawQuad(const glm::vec3 &pos,
                                    const glm::vec2 &size,
                                    const glm::vec4 &color,
                                    int id,
                                    QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[MaterialRenderer] Quad pipeline not available");
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
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);

        if (color.a == 1.f) {
            m_quadInstances.emplace_back(instance);
        } else {
            auto m = makeQuad(instance);
            m_translucentMaterials.push(m);
            auto m_ = m_translucentMaterials.top();
        }
    }
    void MaterialRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                            const glm::vec2 &size,
                                            const glm::vec4 &tint,
                                            int id,
                                            const std::shared_ptr<VulkanTexture> &texture,
                                            QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[MaterialRenderer] Quad pipeline not available");
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

        Material2D m{
            Material2D::MaterialType::quad,
            instance.position.z,
            instance.color.a};
        m.quad.instance = instance;
        m.quad.texture = texture;
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                            const glm::vec2 &size,
                                            const glm::vec4 &tint,
                                            int id,
                                            const std::shared_ptr<SubTexture> &subTexture,
                                            QuadRenderProperties props) {
        if (!m_quadPipeline) {
            BESS_WARN("[MaterialRenderer] Quad pipeline not available");
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

        Material2D m{
            Material2D::MaterialType::quad,
            instance.position.z,
            instance.color.a};
        m.quad.instance = instance;
        m.quad.texture = subTexture->getTexture();
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius) {
        if (!m_circlePipeline) {
            BESS_WARN("[MaterialRenderer] Circle pipeline not available");
            return;
        }

        CircleInstance instance{};
        instance.position = center;
        instance.color = color;
        instance.radius = radius;
        instance.innerRadius = innerRadius;
        instance.id = id;

        if (color.a == 1.f) {
            m_circleInstances.emplace_back(instance);
        } else {
            auto m = makeCircle(instance);
            m_translucentMaterials.push(m);
        }
    }

    void MaterialRenderer::drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const int id, float angle) {
        m_textRenderer->drawText(text, pos, size, color, id);
    }

    void MaterialRenderer::resize(VkExtent2D extent) {
        if (m_quadPipeline)
            m_quadPipeline->resize(extent);

        if (m_circlePipeline)
            m_circlePipeline->resize(extent);

        if (m_gridPipeline)
            m_gridPipeline->resize(extent);

        if (m_textRenderer)
            m_textRenderer->resize(extent);
    }

    void MaterialRenderer::updateUBO(const UniformBufferObject &ubo) {
        if (m_quadPipeline)
            m_quadPipeline->updateUniformBuffer(ubo);

        if (m_circlePipeline)
            m_circlePipeline->updateUniformBuffer(ubo);

        if (m_gridPipeline)
            m_gridPipeline->updateUniformBuffer(ubo);

        if (m_textRenderer)
            m_textRenderer->updateUBO(ubo);
    }

    void MaterialRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_cmdBuffer = commandBuffer;
        if (m_textRenderer) {
            m_textRenderer->beginFrame(m_cmdBuffer);
        }

        m_circleInstances.clear();
        m_quadInstances.clear();
        m_quadPipeline->cleanPrevStateCounter();
        m_circlePipeline->cleanPrevStateCounter();
    }

    void MaterialRenderer::endFrame() {
        if (m_textRenderer) {
            m_textRenderer->endFrame();
        }

        flushVertices(false);


        float prevZ = m_translucentMaterials.top().z;

        while (!m_translucentMaterials.empty()) {
            auto m = m_translucentMaterials.top();
            m_translucentMaterials.pop();
            if (m.z != prevZ) {
                flushVertices(true);
                prevZ = m.z;
            }

            switch (m.type) {
            case Material2D::MaterialType::quad:
                m_quadInstances.emplace_back(m.quad.instance);
                break;
            case Material2D::MaterialType::circle:
                m_circleInstances.emplace_back(m.circle.instance);
                break;
            case Material2D::MaterialType::path:
            case Material2D::MaterialType::grid:
                break;
            }
        }
        flushVertices(true);
    }

    void MaterialRenderer::flushVertices(bool isTranslucent) {
        if (m_quadPipeline && !m_quadInstances.empty()) {
            m_quadPipeline->beginPipeline(m_cmdBuffer, isTranslucent);
            m_quadPipeline->setQuadsData(m_quadInstances);
            m_quadPipeline->endPipeline();
            m_quadInstances.clear();
        }

        if (m_circlePipeline && !m_circleInstances.empty()) {
            m_circlePipeline->beginPipeline(m_cmdBuffer, isTranslucent);
            m_circlePipeline->setCirclesData(m_circleInstances);
            m_circlePipeline->endPipeline();
            m_circleInstances.clear();
        }

    }

    void MaterialRenderer::setCurrentFrameIndex(uint32_t frameIndex) {
        if (m_quadPipeline)
            m_quadPipeline->setCurrentFrameIndex(frameIndex);

        if (m_circlePipeline)
            m_circlePipeline->setCurrentFrameIndex(frameIndex);

        if (m_gridPipeline)
            m_gridPipeline->setCurrentFrameIndex(frameIndex);

        if (m_textRenderer)
            m_textRenderer->setCurrentFrameIndex(frameIndex);
    }
}
