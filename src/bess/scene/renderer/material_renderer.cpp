#include "scene/renderer/material_renderer.h"
#include "application/asset_manager/asset_manager.h"
#include "application/assets.h"
#include "renderer/font.h"
#include "json/json.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace Bess::Renderer {
    namespace {
        struct TextMeasureCacheKey {
            const Font::FontFile *font = nullptr;
            int32_t renderSizeMilli = 0;
            std::string text;

            bool operator==(const TextMeasureCacheKey &other) const noexcept =
                default;
        };

        struct TextMeasureCacheKeyHash {
            size_t operator()(const TextMeasureCacheKey &key) const noexcept {
                size_t seed = std::hash<const void *>{}(key.font);
                seed ^= std::hash<int32_t>{}(key.renderSizeMilli) + 0x9e3779b9 +
                        (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::string>{}(key.text) + 0x9e3779b9 +
                        (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct MsdfTextMeasureCacheKey {
            int32_t renderSizeMilli = 0;
            std::string text;

            bool operator==(
                const MsdfTextMeasureCacheKey &other) const noexcept = default;
        };

        struct MsdfTextMeasureCacheKeyHash {
            size_t
            operator()(const MsdfTextMeasureCacheKey &key) const noexcept {
                size_t seed =
                    std::hash<int32_t>{}(key.renderSizeMilli);
                seed ^= std::hash<std::string>{}(key.text) + 0x9e3779b9 +
                        (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct MsdfMeasureGlyph {
            float advance = 0.f;
            float left = 0.f;
            float right = 0.f;
            float bottom = 0.f;
            float top = 0.f;
            bool drawable = false;
        };

        constexpr const char *kDefaultMsdfMetricsPath =
            "assets/fonts/Roboto/msdf-Roboto-Regular-32/Roboto-Regular.json";
        constexpr uint32_t kReplacementCodepoint = 0xFFFD;

        uint32_t decodeUtf8(std::string_view text, size_t &offset) {
            if (offset >= text.size()) {
                return 0;
            }

            const size_t start = offset;
            const auto first = static_cast<unsigned char>(text[start]);
            if (first <= 0x7F) {
                offset = start + 1;
                return first;
            }

            uint32_t codepoint = 0;
            size_t length = 0;
            uint32_t minCodepoint = 0;
            if ((first & 0xE0) == 0xC0) {
                codepoint = first & 0x1F;
                length = 2;
                minCodepoint = 0x80;
            } else if ((first & 0xF0) == 0xE0) {
                codepoint = first & 0x0F;
                length = 3;
                minCodepoint = 0x800;
            } else if ((first & 0xF8) == 0xF0) {
                codepoint = first & 0x07;
                length = 4;
                minCodepoint = 0x10000;
            } else {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            if (start + length > text.size()) {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            for (size_t i = 1; i < length; ++i) {
                const auto byte = static_cast<unsigned char>(text[start + i]);
                if ((byte & 0xC0) != 0x80) {
                    offset = start + 1;
                    return kReplacementCodepoint;
                }
                codepoint = (codepoint << 6) | (byte & 0x3F);
            }

            if (codepoint < minCodepoint || codepoint > 0x10FFFF ||
                (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
                offset = start + 1;
                return kReplacementCodepoint;
            }

            offset = start + length;
            return codepoint;
        }

        class MsdfMeasureFont {
          public:
            bool load(const std::string &jsonPath) {
                std::ifstream input(jsonPath);
                if (!input.is_open()) {
                    return false;
                }

                Json::Value root;
                Json::CharReaderBuilder builder;
                std::string errors;
                if (!Json::parseFromStream(builder, input, &root, &errors) ||
                    !root.isMember("glyphs")) {
                    return false;
                }

                if (root.isMember("metrics")) {
                    m_lineHeight =
                        root["metrics"].get("lineHeight", 1.f).asFloat();
                } else {
                    m_lineHeight = 1.f;
                }

                m_glyphs.clear();
                for (const auto &glyphJson : root["glyphs"]) {
                    if (!glyphJson.isObject() ||
                        !glyphJson.isMember("unicode")) {
                        continue;
                    }

                    MsdfMeasureGlyph glyph;
                    glyph.advance = glyphJson.get("advance", 0.f).asFloat();
                    if (glyphJson.isMember("planeBounds")) {
                        const Json::Value &bounds = glyphJson["planeBounds"];
                        glyph.left = bounds.get("left", 0.f).asFloat();
                        glyph.right = bounds.get("right", 0.f).asFloat();
                        glyph.bottom = bounds.get("bottom", 0.f).asFloat();
                        glyph.top = bounds.get("top", 0.f).asFloat();
                        glyph.drawable = true;
                    }

                    m_glyphs[static_cast<uint32_t>(
                        glyphJson.get("unicode", 0).asUInt64())] = glyph;
                }

                return !m_glyphs.empty();
            }

            [[nodiscard]] bool valid() const noexcept {
                return !m_glyphs.empty();
            }

            [[nodiscard]] const MsdfMeasureGlyph *
            findGlyph(uint32_t codepoint) const noexcept {
                auto it = m_glyphs.find(codepoint);
                if (it != m_glyphs.end()) {
                    return &it->second;
                }

                it = m_glyphs.find('?');
                if (it != m_glyphs.end()) {
                    return &it->second;
                }

                it = m_glyphs.find(' ');
                return it != m_glyphs.end() ? &it->second : nullptr;
            }

            [[nodiscard]] float spaceAdvance() const noexcept {
                if (const auto *space = findGlyph(' ');
                    space != nullptr && space->advance > 0.f) {
                    return space->advance;
                }
                return 0.25f;
            }

            [[nodiscard]] float lineHeight() const noexcept {
                return m_lineHeight;
            }

          private:
            std::unordered_map<uint32_t, MsdfMeasureGlyph> m_glyphs;
            float m_lineHeight = 1.f;
        };

        const MsdfMeasureFont *getDefaultMsdfMeasureFont() {
            static const MsdfMeasureFont font = [] {
                MsdfMeasureFont loadedFont;
                loadedFont.load(kDefaultMsdfMetricsPath);
                return loadedFont;
            }();

            return font.valid() ? &font : nullptr;
        }

        std::optional<float>
        measureMsdfTextBaselineOffsetForVerticalCenter(std::string_view text,
                                                       float renderSize) {
            const MsdfMeasureFont *font = getDefaultMsdfMeasureFont();
            if (font == nullptr) {
                return std::nullopt;
            }

            const float safeRenderSize = std::max(renderSize, 1.f);
            const auto renderSizeMilli =
                static_cast<int32_t>(std::lround(safeRenderSize * 1000.0f));

            static constexpr size_t kMaxTextMeasureCacheEntries = 4096;
            static std::unordered_map<MsdfTextMeasureCacheKey, float,
                                      MsdfTextMeasureCacheKeyHash>
                s_msdfTextBaselineOffsetCache;
            static std::deque<MsdfTextMeasureCacheKey>
                s_msdfTextBaselineOffsetCacheOrder;

            MsdfTextMeasureCacheKey cacheKey{
                .renderSizeMilli = renderSizeMilli,
                .text = std::string(text),
            };

            if (const auto it = s_msdfTextBaselineOffsetCache.find(cacheKey);
                it != s_msdfTextBaselineOffsetCache.end()) {
                return it->second;
            }

            const float lineHeight =
                std::max(font->lineHeight() * safeRenderSize, safeRenderSize);
            float baselineY = 0.f;
            float inkTop = std::numeric_limits<float>::max();
            float inkBottom = std::numeric_limits<float>::lowest();
            bool hasInk = false;

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\n') {
                    baselineY += lineHeight;
                    continue;
                }

                if (codepoint == '\t') {
                    continue;
                }

                const MsdfMeasureGlyph *glyph = font->findGlyph(codepoint);
                if (glyph == nullptr || !glyph->drawable) {
                    continue;
                }

                const float top = baselineY - (glyph->top * safeRenderSize);
                const float bottom =
                    baselineY - (glyph->bottom * safeRenderSize);
                inkTop = std::min(inkTop, top);
                inkBottom = std::max(inkBottom, bottom);
                hasInk = true;
            }

            const float offsetY =
                hasInk ? -((inkTop + inkBottom) * 0.5f)
                       : safeRenderSize * 0.35f;

            s_msdfTextBaselineOffsetCache.emplace(cacheKey, offsetY);
            s_msdfTextBaselineOffsetCacheOrder.push_back(cacheKey);
            if (s_msdfTextBaselineOffsetCacheOrder.size() >
                kMaxTextMeasureCacheEntries) {
                s_msdfTextBaselineOffsetCache.erase(
                    s_msdfTextBaselineOffsetCacheOrder.front());
                s_msdfTextBaselineOffsetCacheOrder.pop_front();
            }

            return offsetY;
        }

        glm::vec2 measureMsdfTextRenderSize(std::string_view text,
                                            float renderSize) {
            const MsdfMeasureFont *font = getDefaultMsdfMeasureFont();
            if (font == nullptr) {
                return {-1.f, -1.f};
            }

            const float safeRenderSize = std::max(renderSize, 1.f);
            const auto renderSizeMilli =
                static_cast<int32_t>(std::lround(safeRenderSize * 1000.0f));

            static constexpr size_t kMaxTextMeasureCacheEntries = 4096;
            static std::unordered_map<MsdfTextMeasureCacheKey, glm::vec2,
                                      MsdfTextMeasureCacheKeyHash>
                s_msdfTextMeasureCache;
            static std::deque<MsdfTextMeasureCacheKey>
                s_msdfTextMeasureCacheOrder;

            MsdfTextMeasureCacheKey cacheKey{
                .renderSizeMilli = renderSizeMilli,
                .text = std::string(text),
            };

            if (const auto it = s_msdfTextMeasureCache.find(cacheKey);
                it != s_msdfTextMeasureCache.end()) {
                return it->second;
            }

            const float spaceAdvance =
                font->spaceAdvance() * safeRenderSize;
            float lineAdvance = 0.f;
            float lineInkMin = 0.f;
            float lineInkMax = 0.f;
            bool hasLineInk = false;
            float maxLineWidth = 0.f;
            float totalHeight = safeRenderSize;

            auto finishLine = [&]() {
                float lineWidth = lineAdvance;
                if (hasLineInk) {
                    const float inkMin = std::min(0.f, lineInkMin);
                    const float inkMax = std::max(lineAdvance, lineInkMax);
                    lineWidth = std::max(lineWidth, inkMax - inkMin);
                }
                maxLineWidth = std::max(maxLineWidth, lineWidth);

                lineAdvance = 0.f;
                lineInkMin = 0.f;
                lineInkMax = 0.f;
                hasLineInk = false;
            };

            size_t offset = 0;
            while (offset < text.size()) {
                const uint32_t codepoint = decodeUtf8(text, offset);
                if (codepoint == 0) {
                    break;
                }

                if (codepoint == '\r') {
                    if (offset < text.size() && text[offset] == '\n') {
                        ++offset;
                    }
                    finishLine();
                    totalHeight += safeRenderSize;
                    continue;
                }

                if (codepoint == '\n') {
                    finishLine();
                    totalHeight += safeRenderSize;
                    continue;
                }

                if (codepoint == '\t') {
                    lineAdvance += spaceAdvance * 4.f;
                    continue;
                }

                const MsdfMeasureGlyph *glyph = font->findGlyph(codepoint);
                if (glyph == nullptr) {
                    continue;
                }

                if (glyph->drawable) {
                    const float glyphLeft =
                        lineAdvance + glyph->left * safeRenderSize;
                    const float glyphRight =
                        lineAdvance + glyph->right * safeRenderSize;
                    if (hasLineInk) {
                        lineInkMin = std::min(lineInkMin, glyphLeft);
                        lineInkMax = std::max(lineInkMax, glyphRight);
                    } else {
                        lineInkMin = glyphLeft;
                        lineInkMax = glyphRight;
                        hasLineInk = true;
                    }
                }

                lineAdvance +=
                    (glyph->advance > 0.f ? glyph->advance : 0.5f) *
                    safeRenderSize;
            }

            finishLine();
            glm::vec2 calcSize = {maxLineWidth, totalHeight};

            s_msdfTextMeasureCache.emplace(cacheKey, calcSize);
            s_msdfTextMeasureCacheOrder.push_back(cacheKey);
            if (s_msdfTextMeasureCacheOrder.size() >
                kMaxTextMeasureCacheEntries) {
                s_msdfTextMeasureCache.erase(
                    s_msdfTextMeasureCacheOrder.front());
                s_msdfTextMeasureCacheOrder.pop_front();
            }

            return calcSize;
        }
    } // namespace

    static Material2D makeGrid(const glm::vec3 &pos, const glm::vec2 &size,
                               uint64_t id) {
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

    MaterialRenderer::MaterialRenderer(
        const std::shared_ptr<VulkanDevice> &device,
        const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
        VkExtent2D extent) {

        m_gridPipeline = std::make_unique<Pipelines::GridPipeline>(
            device, renderPass, extent);
        m_primitivePipeline = std::make_unique<Pipelines::PrimitivePipeline>(
            device, renderPass, extent);
        m_textRenderer = std::make_unique<Renderer::TextRenderer>(
            device, renderPass, extent);

        auto fontFilePtr = getFontFile();
        *fontFilePtr = m_textRenderer->getFontFile();

        m_translucentMaterials = {};

        m_gridMaterial = makeGrid({0.f, 0.f, 0.f}, {1.f, 1.f}, -2);

        auto &appCtx = Bess::GAppContext::getInstance();
        m_shadowTexture = appCtx.getSubSystem<Assets::AssetManager>()->get(
            Assets::Textures::shadowTexture);
    }

    MaterialRenderer::~MaterialRenderer() = default;

    void MaterialRenderer::drawMaterial(const Material2D &material) {
        if (material.alpha != 1.f) {
            m_translucentMaterials.push(material);
            return;
        }
    }

    void MaterialRenderer::drawGrid(const glm::vec3 &pos, const glm::vec2 &size,
                                    uint64_t id, const GridColors &gridColors,
                                    const std::shared_ptr<Camera> &camera) {
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

    void MaterialRenderer::drawQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                    const glm::vec4 &color, uint64_t id,
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

            drawTexturedQuad(
                {pos.x + offset.x, pos.y + offset.y, pos.z - 0.0001},
                {(size.x * scaleShadow.x) - props.borderRadius.x,
                 (size.y * scaleShadow.y) - props.borderRadius.y},
                props.shadow.color,
                props.shadow.useInvalidId ? PickingId::invalid() : id,
                m_shadowTexture, shadowProps);
        }
    }

    void MaterialRenderer::drawTexturedQuad(
        const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &tint,
        uint64_t id, const std::shared_ptr<VulkanTexture> &texture,
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

    void MaterialRenderer::drawTexturedQuad(
        const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &tint,
        uint64_t id, const std::shared_ptr<SubTexture> &subTexture,
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

    void MaterialRenderer::drawCircle(const glm::vec3 &center, float radius,
                                      const glm::vec4 &color, uint64_t id,
                                      float innerRadius) {
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

    void MaterialRenderer::drawLine(const glm::vec3 &start,
                                    const glm::vec3 &end, const float thickness,
                                    const glm::vec4 &color, const uint64_t id) {
        if (!m_primitivePipeline) {
            return;
        }

        const glm::vec2 delta = glm::vec2(end) - glm::vec2(start);
        const float length = glm::length(delta);
        if (length <= 0.0001f) {
            drawCircle(start, thickness * 0.5f, color, id);
            return;
        }

        PrimitiveInstance instance{};
        instance.primitiveType = static_cast<int32_t>(PrimitiveType::Line);
        instance.position =
            glm::vec3((glm::vec2(start) + glm::vec2(end)) * 0.5f,
                      (start.z + end.z) * 0.5f);
        instance.color = color;
        instance.size = glm::vec2(length + thickness, thickness);
        instance.primitiveData = glm::vec4(thickness, 0.0f, 0.0f, 0.0f);
        instance.id = encodeId(id);
        instance.angle = std::atan2(delta.y, delta.x);

        if (color.a == 1.f) {
            m_primitiveInstances.emplace_back(instance);
        } else {
            auto m = makePrimitive(instance);
            m_translucentMaterials.push(m);
        }
    }

    void MaterialRenderer::drawText(const std::string &text,
                                    const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const uint64_t &id,
                                    float angle) {

        if (m_textRenderer) {
            m_textRenderer->drawText(text, pos, size, color, id);
        }
    }

    void MaterialRenderer::drawIcon(const std::string &text,
                                    const glm::vec3 &pos, const size_t size,
                                    const glm::vec4 &color, const uint64_t &id,
                                    float angle) {

        if (m_textRenderer) {
            m_textRenderer->drawIcon(text, pos, size, color, id);
        }
    }

    glm::vec2 MaterialRenderer::drawTextWrapped(
        const std::string &text, const glm::vec3 &pos, const size_t size,
        const glm::vec4 &color, const uint64_t &id, float wrapWidthPx,
        float angle) {
        if (m_textRenderer) {
            return m_textRenderer->drawTextWrapped(text, pos, size, color, id,
                                                   wrapWidthPx, angle);
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

        float prevZ =
            m_translucentMaterials.empty() ? 0 : m_translucentMaterials.top().z;

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
                    m_texturedPrimitiveInstances[m.primitive.texture]
                        .emplace_back(m.primitive.instance);
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
        if (m_primitivePipeline && (!m_primitiveInstances.empty() ||
                                    !m_texturedPrimitiveInstances.empty())) {
            m_primitivePipeline->drawPrimitiveInstances(
                m_cmdBuffer, isTranslucent, m_primitiveInstances,
                m_texturedPrimitiveInstances);
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
        const glm::vec2 msdfSize = measureMsdfTextRenderSize(str, renderSize);
        if (msdfSize.x >= 0.f) {
            return msdfSize;
        }

        auto font = *getFontFile();

        // for tests when font is not loaded
        if (!font) {
            const float safeRenderSize = std::max(renderSize, 1.f);
            float currentLineWidth = 0.f;
            float maxLineWidth = 0.f;
            float totalHeight = safeRenderSize;
            for (const char ch : str) {
                if (ch == '\n') {
                    maxLineWidth = std::max(maxLineWidth, currentLineWidth);
                    currentLineWidth = 0.f;
                    totalHeight += safeRenderSize;
                    continue;
                }
                currentLineWidth += safeRenderSize * 0.6f;
            }

            maxLineWidth = std::max(maxLineWidth, currentLineWidth);
            return {maxLineWidth, totalHeight};
        }

        const auto renderSizeMilli =
            static_cast<int32_t>(std::lround(renderSize * 1000.0f));

        static constexpr size_t kMaxTextMeasureCacheEntries = 4096;
        static std::unordered_map<TextMeasureCacheKey, glm::vec2,
                                  TextMeasureCacheKeyHash>
            s_textMeasureCache;
        static std::deque<TextMeasureCacheKey> s_textMeasureCacheOrder;

        TextMeasureCacheKey cacheKey{
            .font = font,
            .renderSizeMilli = renderSizeMilli,
            .text = str,
        };

        if (const auto it = s_textMeasureCache.find(cacheKey);
            it != s_textMeasureCache.end()) {
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

    float MaterialRenderer::getTextBaselineOffsetForVerticalCenter(
        const std::string &str, float renderSize) {
        if (const auto msdfOffset =
                measureMsdfTextBaselineOffsetForVerticalCenter(str,
                                                               renderSize)) {
            return *msdfOffset;
        }

        const float safeRenderSize = std::max(renderSize, 1.f);
        auto font = *getFontFile();
        if (!font) {
            return safeRenderSize * 0.35f;
        }

        const float fontSize = font->getSize();
        if (fontSize <= 0.f) {
            return safeRenderSize * 0.35f;
        }

        const float scale = safeRenderSize / fontSize;
        float baselineY = 0.f;
        float inkTop = std::numeric_limits<float>::max();
        float inkBottom = std::numeric_limits<float>::lowest();
        bool hasInk = false;

        auto includeY = [&](float y) {
            inkTop = std::min(inkTop, y);
            inkBottom = std::max(inkBottom, y);
            hasInk = true;
        };

        for (const char ch : str) {
            if (ch == '\n') {
                baselineY += safeRenderSize;
                continue;
            }

            const auto &glyph = font->getGlyph(ch);
            for (const auto &cmd : glyph.path.getCmds()) {
                switch (cmd.kind) {
                case Path::PathCommand::Kind::Move:
                    includeY(baselineY + (cmd.move.p.y * scale));
                    break;
                case Path::PathCommand::Kind::Line:
                    includeY(baselineY + (cmd.line.p.y * scale));
                    break;
                case Path::PathCommand::Kind::Quad:
                    includeY(baselineY + (cmd.quad.c.y * scale));
                    includeY(baselineY + (cmd.quad.p.y * scale));
                    break;
                case Path::PathCommand::Kind::Cubic:
                    includeY(baselineY + (cmd.cubic.c1.y * scale));
                    includeY(baselineY + (cmd.cubic.c2.y * scale));
                    includeY(baselineY + (cmd.cubic.p.y * scale));
                    break;
                }
            }
        }

        return hasInk ? -((inkTop + inkBottom) * 0.5f)
                      : safeRenderSize * 0.35f;
    }

    Font::FontFile **MaterialRenderer::getFontFile() {
        static Font::FontFile *file = nullptr;
        return &file;
    }
} // namespace Bess::Renderer
