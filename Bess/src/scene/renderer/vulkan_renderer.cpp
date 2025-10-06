#include "scene/renderer/vulkan_renderer.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "common/log.h"

namespace Bess::Renderer2D {
    std::shared_ptr<Camera> VulkanRenderer::m_camera = nullptr;

    void VulkanRenderer::init() {
    }

    void VulkanRenderer::shutdown() {
        BESS_INFO("[VulkanRenderer] Shutting down Vulkan renderer");
        m_camera = nullptr;
    }

    void VulkanRenderer::beginScene(const std::shared_ptr<Camera> &camera) {
        m_camera = camera;
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getTransform();
        ubo.ortho = m_camera->getOrtho();
        primitiveRenderer->updateUBO(ubo);

        if (pathRenderer) {
            pathRenderer->updateUniformBuffer(ubo);
        }
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

    void VulkanRenderer::circle(const glm::vec3 &center, const float radius,
                                const glm::vec4 &color, const int id, float innerRadius) {
        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->drawCircle(center, radius, color, id, innerRadius);
    }

    void VulkanRenderer::msdfText(const std::string &text, const glm::vec3 &pos, const size_t size,
                                  const glm::vec4 &color, const int id, float angle) {
        // Command to use to generate MSDF font texture atlas
        // https://github.com/Chlumsky/msdf-atlas-gen
        // msdf-atlas-gen -font Roboto-Regular.ttf -type mtsdf -size 64 -imageout roboto_mtsdf.png -json roboto.json -pxrange 4

        if (text.empty())
            return;

        auto msdfFont = Assets::AssetManager::instance().get(Assets::Fonts::robotoMsdf);
        if (!msdfFont) {
            BESS_WARN("[VulkanRenderer] MSDF font not available");
            return;
        }

        float scale = size;
        float lineHeight = msdfFont->getLineHeight() * scale;

        MsdfCharacter yCharInfo = msdfFont->getCharacterData('y');
        MsdfCharacter wCharInfo = msdfFont->getCharacterData('W');

        float baseLineOff = yCharInfo.offset.y - wCharInfo.offset.y;

        glm::vec2 charPos = pos;
        std::vector<Vulkan::InstanceVertex> opaqueInstances;
        std::vector<Vulkan::InstanceVertex> translucentInstances;

        for (auto &ch : text) {
            const MsdfCharacter &charInfo = msdfFont->getCharacterData(ch);
            if (ch == ' ') {
                charPos.x += charInfo.advance * scale;
                continue;
            }
            const auto &subTexture = charInfo.subTexture;
            glm::vec2 size_ = charInfo.size * scale;
            float xOff = (charInfo.offset.x + charInfo.size.x / 2.f) * scale;
            float yOff = (charInfo.offset.y + charInfo.size.y / 2.f) * scale;

            Vulkan::InstanceVertex vertex{};
            vertex.position = {charPos.x + xOff, charPos.y - yOff, pos.z};
            vertex.size = size_;
            vertex.angle = angle;
            vertex.color = color;
            vertex.id = id;
            vertex.texSlotIdx = 1;
            vertex.texData = subTexture->getStartWH();

            if (color.a == 1.f) {
                opaqueInstances.emplace_back(vertex);
            } else {
                translucentInstances.emplace_back(vertex);
            }

            charPos.x += charInfo.advance * scale;
        }

        // Set up text uniforms (pxRange for MSDF rendering)
        Vulkan::TextUniforms textUniforms{};
        textUniforms.pxRange = 4.0f; // Standard MSDF pxRange value

        auto primitiveRenderer = VulkanCore::instance().getPrimitiveRenderer().lock();
        primitiveRenderer->updateTextUniforms(textUniforms);
        primitiveRenderer->drawText(opaqueInstances, translucentInstances);
    }

    void VulkanRenderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();
        if (pathRenderer) {
            pathRenderer->beginPathMode(startPos, weight, color, static_cast<int>(id));
        }
    }

    void VulkanRenderer::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();
        if (pathRenderer) {
            pathRenderer->endPathMode(closePath, genFill, fillColor, genStroke);
        }
    }

    void VulkanRenderer::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();
        if (pathRenderer) {
            pathRenderer->pathLineTo(pos, size, color, id);
        }
    }

    void VulkanRenderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                           float weight, const glm::vec4 &color, int id) {
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();
        if (pathRenderer) {
            pathRenderer->pathCubicBeizerTo(end, controlPoint1, controlPoint2, weight, color, id);
        }
    }

    void VulkanRenderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        auto pathRenderer = VulkanCore::instance().getPathRenderer().lock();
        if (pathRenderer) {
            pathRenderer->pathQuadBeizerTo(end, controlPoint, weight, color, id);
        }
    }

    glm::vec2 VulkanRenderer::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        float xSize = 0;
        auto msdfFont = Assets::AssetManager::instance().get(Assets::Fonts::robotoMsdf);
        float ySize = msdfFont->getLineHeight();

        for (auto &ch : str) {
            auto chInfo = msdfFont->getCharacterData(ch);
            xSize += chInfo.advance;
        }
        return glm::vec2({xSize, ySize}) * renderSize;
    }
} // namespace Bess::Renderer2D
