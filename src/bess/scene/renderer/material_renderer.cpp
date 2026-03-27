#include "scene/renderer/material_renderer.h"
#include "application/asset_manager/asset_manager.h"
#include "application/assets.h"
#include "renderer/font.h"
#include "scene/scene_state/components/scene_component_types.h"
#include <cmath>
#include <cstdint>
#include <deque>

namespace Bess::Renderer {
    namespace {
        struct TextMeasureCacheKey {
            const Font::FontFile *font = nullptr;
            int32_t renderSizeMilli = 0;
            std::string text;

            bool operator==(const TextMeasureCacheKey &other) const noexcept = default;
        };

        struct TextMeasureCacheKeyHash {
            size_t operator()(const TextMeasureCacheKey &key) const noexcept {
                size_t seed = std::hash<const void *>{}(key.font);
                seed ^= std::hash<int32_t>{}(key.renderSizeMilli) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::string>{}(key.text) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                return seed;
            }
        };
    } // namespace

    static Material2D makeGrid(const glm::vec3 &pos, const glm::vec2 &size, uint64_t id) {
        Material2D m;
        m.type = Material2D::MaterialType::grid;
        new (&m.grid) GridMaterial();
        m.grid.position = pos;
        m.grid.size = size;
        m.grid.id = id;
        m.alpha = 0.9f;
        m.z = pos.z;
        return m;
    }

    static Material2D makePrimitive(const Vulkan::PrimitiveInstance &instance) {
        Material2D m;
        m.type = Material2D::MaterialType::primitive;
        new (&m.primitive) PrimitiveMaterial();
        m.primitive.instance = instance;
        m.z = instance.position.z;
        m.alpha = instance.color.a;
        return m;
    }

    MaterialRenderer::MaterialRenderer(const std::shared_ptr<VulkanDevice> &device,
                                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                                       VkExtent2D extent) {

        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(device, renderPass, extent);
        m_primitivePipeline = std::make_unique<Pipelines::PrimitivePipeline>(device, renderPass, extent);
        m_textRenderer = std::make_unique<Renderer::TextRenderer>(device, renderPass, extent);

        auto fontFilePtr = getFontFile();
        *fontFilePtr = m_textRenderer->getFontFile();

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
        if (!m_primitivePipeline) {
            return;
        }

        PrimitiveInstance instance{};
        instance.primitiveType = static_cast<int32_t>(PrimitiveType::Quad);
        instance.position = pos;
        instance.color = color;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = static_cast<int>(props.isMica);
        instance.texSlotIdx = 0;
        instance.angle = props.angle;

        if (color.a == 1.f) {
            m_primitiveInstances.emplace_back(instance);
        } else {
            auto m = makePrimitive(instance);
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
                             props.shadow.useInvalidId
                                 ? Canvas::PickingId::invalid()
                                 : id,
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
        if (!m_primitivePipeline) {
            return;
        }

        PrimitiveInstance instance{};
        instance.primitiveType = static_cast<int32_t>(PrimitiveType::Quad);
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.texData = glm::vec4(0.0f, 0.0f, 1.f, 1.f);
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = static_cast<int>(props.isMica);
        instance.angle = props.angle;

        auto m = makePrimitive(instance);
        m.primitive.instance = instance;
        m.primitive.texture = texture;
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawTexturedQuad(const glm::vec3 &pos,
                                            const glm::vec2 &size,
                                            const glm::vec4 &tint,
                                            uint64_t id,
                                            const std::shared_ptr<SubTexture> &subTexture,
                                            QuadRenderProperties props) {
        if (!m_primitivePipeline) {
            return;
        }

        PrimitiveInstance instance{};
        instance.primitiveType = static_cast<int32_t>(PrimitiveType::Quad);
        instance.position = pos;
        instance.color = tint;
        instance.borderRadius = props.borderRadius;
        instance.borderColor = props.borderColor;
        instance.borderSize = props.borderSize;
        instance.texData = subTexture->getStartWH();
        instance.size = size;
        instance.id = encodeId(id);
        instance.isMica = static_cast<int>(props.isMica);
        instance.angle = props.angle;

        auto m = makePrimitive(instance);
        m.primitive.instance = instance;
        m.primitive.texture = subTexture->getTexture();
        m_translucentMaterials.push(m);
    }

    void MaterialRenderer::drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, uint64_t id, float innerRadius) {
        if (!m_primitivePipeline) {
            return;
        }

        PrimitiveInstance instance{};
        instance.primitiveType = static_cast<int32_t>(PrimitiveType::Circle);
        instance.position = center;
        instance.color = color;
        instance.size = glm::vec2(radius * 2.0f);
        instance.primitiveData = glm::vec4(radius, innerRadius, 0.0f, 0.0f);
        instance.id = encodeId(id);

        if (color.a == 1.f) {
            m_primitiveInstances.emplace_back(instance);
        } else {
            auto m = makePrimitive(instance);
            m_translucentMaterials.push(m);
        }
    }

    void MaterialRenderer::drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const uint64_t &id, float angle) {

        if (m_textRenderer) {
            m_textRenderer->drawText(text, pos, size, color, id);
        }
    }

    void MaterialRenderer::drawIcon(const std::string &text, const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const uint64_t &id, float angle) {

        if (m_textRenderer) {
            m_textRenderer->drawIcon(text, pos, size, color, id);
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
        if (m_primitivePipeline)
            m_primitivePipeline->resize(extent);

        if (m_gridPipeline)
            m_gridPipeline->resize(extent);

        if (m_textRenderer)
            m_textRenderer->resize(extent);
    }

    void MaterialRenderer::updateUBO(const std::shared_ptr<Camera> &camera) {
        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = camera->getTransform();
        ubo.ortho = camera->getOrtho();
        if (m_primitivePipeline)
            m_primitivePipeline->updateUniformBuffer(ubo);

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

        if (m_primitivePipeline) {
            m_primitivePipeline->cleanPrevStateCounter();
            m_primitiveInstances.clear();
            m_texturedPrimitiveInstances.clear();
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
            case Material2D::MaterialType::primitive: {
                if (m.primitive.texture) {
                    m_texturedPrimitiveInstances[m.primitive.texture].emplace_back(m.primitive.instance);
                } else {
                    m_primitiveInstances.emplace_back(m.primitive.instance);
                }
            } break;
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
        if (m_primitivePipeline && (!m_primitiveInstances.empty() || !m_texturedPrimitiveInstances.empty())) {
            m_primitivePipeline->drawPrimitiveInstances(m_cmdBuffer, isTranslucent, m_primitiveInstances, m_texturedPrimitiveInstances);
            m_primitiveInstances.clear();
            m_texturedPrimitiveInstances.clear();
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
        if (m_primitivePipeline)
            m_primitivePipeline->setCurrentFrameIndex(frameIndex);

        if (m_gridPipeline)
            m_gridPipeline->setCurrentFrameIndex(frameIndex);

        if (m_textRenderer)
            m_textRenderer->setCurrentFrameIndex(frameIndex);
    }

    glm::vec2 MaterialRenderer::getTextRenderSize(const std::string &str,
                                                  float renderSize) {
        auto font = *getFontFile();
        BESS_ASSERT(font, "Font file not set in MaterialRenderer");
        const auto renderSizeMilli = static_cast<int32_t>(std::lround(renderSize * 1000.0f));

        static constexpr size_t kMaxTextMeasureCacheEntries = 4096;
        static std::unordered_map<TextMeasureCacheKey, glm::vec2, TextMeasureCacheKeyHash> s_textMeasureCache;
        static std::deque<TextMeasureCacheKey> s_textMeasureCacheOrder;

        TextMeasureCacheKey cacheKey{
            .font = font,
            .renderSizeMilli = renderSizeMilli,
            .text = str,
        };

        if (const auto it = s_textMeasureCache.find(cacheKey); it != s_textMeasureCache.end()) {
            return it->second;
        }

        const float scale = renderSize / font->getSize();
        float currentLineWidth = 0.f;
        float maxLineWidth = 0.f;
        float totalHeight = renderSize;
        for (const char ch : str) {
            if (ch == '\n') {
                maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                currentLineWidth = 0.f;
                totalHeight += renderSize;
                continue;
            }
            const auto &glyph = font->getGlyph(ch);
            currentLineWidth += glyph.advanceX;
        }

        maxLineWidth = std::max(maxLineWidth, currentLineWidth);
        glm::vec2 calcSize = {maxLineWidth * scale, totalHeight};

        s_textMeasureCache.emplace(cacheKey, calcSize);
        s_textMeasureCacheOrder.push_back(cacheKey);
        if (s_textMeasureCacheOrder.size() > kMaxTextMeasureCacheEntries) {
            s_textMeasureCache.erase(s_textMeasureCacheOrder.front());
            s_textMeasureCacheOrder.pop_front();
        }

        return calcSize;
    }

    Font::FontFile **MaterialRenderer::getFontFile() {
        static Font::FontFile *file = nullptr;
        return &file;
    }
} // namespace Bess::Renderer
