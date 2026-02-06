#include "scene/renderer/material_renderer.h"
#include "application/assets.h"
#include "asset_manager/asset_manager.h"
#include <cstdint>

namespace Bess::Renderer {
    static Material2D makeGrid(const glm::vec3 &pos, const glm::vec2 &size, uint64_t id) {
        Material2D m;
        m.type = Material2D::MaterialType::grid;
        new (&m.grid) QuadMaterial();
        m.grid.position = pos;
        m.grid.size = size;
        m.grid.id = id;
        m.alpha = 0.9f;
        m.z = pos.z;
        return m;
    }

    MaterialRenderer::MaterialRenderer(const std::shared_ptr<VulkanDevice> &device,
                                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                       VkExtent2D extent) {

        m_circlePipeline = std::make_unique<Pipelines::CirclePipeline>(device, renderPass, extent);
        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(device, renderPass, extent);
        m_quadPipeline = std::make_unique<Pipelines::QuadPipeline>(device, renderPass, extent);
        m_textRenderer = std::make_unique<Renderer::TextRenderer>(device, renderPass, extent);

        m_translucentMaterials = {};

        m_gridMaterial = makeGrid({0.f, 0.f, 0.f}, {1.f, 1.f}, -2);

        m_shadowTexture = Assets::AssetManager::instance().get(Assets::Textures::shadowTexture);
    }

    MaterialRenderer::~MaterialRenderer() = default;

    void MaterialRenderer::drawMaterial(const Material2D &material) {
        if (material.alpha != 1.f) {
            m_translucentMaterials.push(material);
            return;
        }
    }

    void MaterialRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size, uint64_t id,
                                    const GridColors &gridColors, const std::shared_ptr<Camera> &camera) {
        if (!m_gridPipeline) {
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

        auto m = makeGrid(pos, size, id);
        m.grid.uniforms = uniforms;

        m_translucentMaterials.push(m);
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

    glm::uvec2 encodeId(uint64_t id) {
        glm::uvec2 encodedId;
        encodedId.x = static_cast<uint32_t>(id & 0xFFFFFFFF);
        encodedId.y = static_cast<uint32_t>((id >> 32));
        return encodedId;
    }

    void MaterialRenderer::drawQuad(const glm::vec3 &pos,
                                    const glm::vec2 &size,
                                    const glm::vec4 &color,
                                    uint64_t id,
                                    QuadRenderProperties props) {
        if (!m_quadPipeline) {
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = (int)props.isMica;
        instance.texSlotIdx = 0;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);

        if (color.a == 1.f) {
            m_quadInstances.emplace_back(instance);
        } else {
            auto m = makeQuad(instance);
            m_translucentMaterials.push(m);
        }

        if (props.shadow.enabled) {
            QuadRenderProperties shadowProps;
            const auto &offset = props.shadow.offset;
            const auto &scaleShadow = props.shadow.scale;
            shadowProps.borderRadius = props.borderRadius;
            drawTexturedQuad({pos.x + offset.x, pos.y + offset.y, pos.z - 0.0001},
                             {(size.x * scaleShadow.x) - props.borderRadius.x,
                              (size.y * scaleShadow.y) - props.borderRadius.y},
                             props.shadow.color,
                             id,
                             m_shadowTexture,
                             shadowProps);
        }
    }

    void MaterialRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                            const glm::vec2 &size,
                                            const glm::vec4 &tint,
                                            uint64_t id,
                                            const std::shared_ptr<VulkanTexture> &texture,
                                            QuadRenderProperties props) {
        if (!m_quadPipeline) {
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = (int)props.isMica;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);

        auto m = makeQuad(instance);
        m.quad.instance = instance;
        m.quad.texture = texture;
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                            const glm::vec2 &size,
                                            const glm::vec4 &tint,
                                            uint64_t id,
                                            const std::shared_ptr<SubTexture> &subTexture,
                                            QuadRenderProperties props) {
        if (!m_quadPipeline) {
            return;
        }

        QuadInstance instance{};
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = (int)props.isMica;
        instance.texData = subTexture->getStartWH();

        auto m = makeQuad(instance);
        m.quad.instance = instance;
        m.quad.texture = subTexture->getTexture();
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, uint64_t id, float innerRadius) {
        if (!m_circlePipeline) {
            return;
        }

        CircleInstance instance{};
        instance.position = center;
        instance.color = color;
        instance.radius = radius;
        instance.innerRadius = innerRadius;
        instance.id = encodeId(id);

        if (color.a == 1.f) {
            m_circleInstances.emplace_back(instance);
        } else {
            auto m = makeCircle(instance);
            m_translucentMaterials.push(m);
        }
    }

    void MaterialRenderer::drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const uint64_t &id, float angle) {

        if (m_textRenderer) {
            m_textRenderer->drawText(text, pos, size, color, id);
        }
    }

    glm::vec2 MaterialRenderer::drawTextWrapped(const std::string &text, const glm::vec3 &pos, const size_t size,
                                                const glm::vec4 &color, const uint64_t &id, float wrapWidthPx, float angle) {
        if (m_textRenderer) {
            return m_textRenderer->drawTextWrapped(text, pos, size, color, id, wrapWidthPx, angle);
        }

        return {0.f, 0.f};
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

    void MaterialRenderer::updateUBO(const std::shared_ptr<Camera> &camera) {
        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = camera->getTransform();
        ubo.ortho = camera->getOrtho();
        if (m_quadPipeline)
            m_quadPipeline->updateUniformBuffer(ubo);

        if (m_circlePipeline)
            m_circlePipeline->updateUniformBuffer(ubo);

        if (m_gridPipeline) {
            m_gridPipeline->updateUniformBuffer(ubo);
        }

        if (m_textRenderer)
            m_textRenderer->updateUBO(ubo);
    }

    void MaterialRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_cmdBuffer = commandBuffer;
        if (m_textRenderer) {
            m_textRenderer->beginFrame(m_cmdBuffer);
        }

        if (m_circlePipeline) {
            m_circlePipeline->cleanPrevStateCounter();
            m_circleInstances.clear();
        }

        if (m_quadPipeline) {
            m_quadPipeline->cleanPrevStateCounter();
            m_quadInstances.clear();
            m_texturedQuadInstances.clear();
        }
    }

    void MaterialRenderer::endFrame() {
        /// opaque entities
        if (m_textRenderer) {
            m_textRenderer->endFrame();
        }

        flushVertices(false);

        /// translucent entities

        float prevZ = m_translucentMaterials.empty() ? 0 : m_translucentMaterials.top().z;

        while (!m_translucentMaterials.empty()) {
            auto m = m_translucentMaterials.top();
            m_translucentMaterials.pop();
            if (m.z != prevZ) {
                flushVertices(true);
                prevZ = m.z;
            }

            switch (m.type) {
            case Material2D::MaterialType::quad: {
                if (m.quad.texture) {
                    m_texturedQuadInstances[m.quad.texture].emplace_back(m.quad.instance);
                } else {
                    m_quadInstances.emplace_back(m.quad.instance);
                }
            } break;
            case Material2D::MaterialType::circle:
                m_circleInstances.emplace_back(m.circle.instance);
                break;
            case Material2D::MaterialType::grid:
                m_gridMaterial = m;
                break;
            case Material2D::MaterialType::path:
                break;
            }
        }
        flushVertices(true);
    }

    void MaterialRenderer::flushVertices(bool isTranslucent) {
        if (m_quadPipeline && (!m_quadInstances.empty() || !m_texturedQuadInstances.empty())) {
            m_quadPipeline->drawQuadInstances(m_cmdBuffer, isTranslucent, m_quadInstances, m_texturedQuadInstances);
            m_quadInstances.clear();
            m_texturedQuadInstances.clear();
        }

        if (m_circlePipeline && !m_circleInstances.empty()) {
            m_circlePipeline->beginPipeline(m_cmdBuffer, isTranslucent);
            m_circlePipeline->setCirclesData(m_circleInstances);
            m_circlePipeline->endPipeline();
            m_circleInstances.clear();
        }

        if (m_gridPipeline && m_gridMaterial.grid.id != -2) {
            const auto &grid = m_gridMaterial.grid;
            m_gridPipeline->updateGridUniforms(m_gridMaterial.grid.uniforms);
            m_gridPipeline->beginPipeline(m_cmdBuffer, isTranslucent);
            m_gridPipeline->drawGrid(grid.position, grid.size, grid.id);
            m_gridPipeline->endPipeline();
            m_gridMaterial.grid.id = -2;
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

    glm::vec2 MaterialRenderer::getTextRenderSize(const std::string &str, float renderSize) {
        if (!m_textRenderer) {
            return {0.f, 0.f};
        }
        return m_textRenderer->getRenderSize(str, (size_t)renderSize);
    }
} // namespace Bess::Renderer
