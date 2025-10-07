#include "scene/viewport.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include <cstdint>
#include <memory>

namespace Bess::Canvas {

    constexpr int framesInFlight = 2;

    Viewport::Viewport(const std::shared_ptr<Vulkan::VulkanDevice> &device, VkFormat imgFormat, VkExtent2D size)
        : m_device(device), m_imgFormat(imgFormat), m_size(size) {

        m_cmdBuffers = std::make_unique<Vulkan::VulkanCommandBuffers>(m_device, framesInFlight);

        m_camera = std::make_shared<Camera>(size.width, size.height);

        m_renderPass = std::make_shared<Vulkan::VulkanOffscreenRenderPass>(m_device, m_imgFormat, m_pickingIdFormat);

        m_imgView = std::make_unique<Vulkan::VulkanImageView>(m_device, m_imgFormat, m_pickingIdFormat, size);
        m_imgView->createFramebuffer(m_renderPass->getVkHandle());

        m_primitiveRenderer = std::make_unique<Vulkan::PrimitiveRenderer>(m_device, m_renderPass, size);
        m_pathRenderer = std::make_unique<Vulkan::PathRenderer>(m_device, m_renderPass, size);

        createFences(framesInFlight);
    }

    Viewport::~Viewport() {
        m_device->waitForIdle();

        deleteFences();

        m_imgView.reset();
        m_renderPass.reset();
        m_cmdBuffers.reset();
    }

    void Viewport::begin(int frameIdx, const glm::vec4 &clearColor, int clearPickingId) {
        m_currentFrameIdx = frameIdx;

        vkWaitForFences(m_device->device(), 1, &m_fences[frameIdx], VK_TRUE, UINT64_MAX);
        m_cmdBuffers->at(frameIdx)->beginRecording();
        vkResetFences(m_device->device(), 1, &m_fences[frameIdx]);

        const auto cmdBuffer = m_cmdBuffers->at(frameIdx)->getVkHandle();

        m_renderPass->begin(cmdBuffer, m_imgView->getFramebuffer(), m_size, clearColor, clearPickingId);

        m_primitiveRenderer->setCurrentFrameIndex(m_currentFrameIdx);
        m_pathRenderer->setCurrentFrameIndex(m_currentFrameIdx);

        m_primitiveRenderer->beginFrame(cmdBuffer);
        m_pathRenderer->beginFrame(cmdBuffer);

        Vulkan::UniformBufferObject ubo{};
        ubo.mvp = m_camera->getTransform();
        ubo.ortho = m_camera->getOrtho();
        m_primitiveRenderer->updateUBO(ubo);
        m_pathRenderer->updateUniformBuffer(ubo);
    }

    VkCommandBuffer Viewport::end() {
        m_primitiveRenderer->endFrame();
        m_pathRenderer->endFrame();
        m_renderPass->end();
        m_cmdBuffers->at(m_currentFrameIdx)->endRecording();
        return m_cmdBuffers->at(m_currentFrameIdx)->getVkHandle();
    }

    void Viewport::submit() {
        m_device->submitCmdBuffers({m_cmdBuffers->at(m_currentFrameIdx)->getVkHandle()}, m_fences[m_currentFrameIdx]);
    }

    void Viewport::resize(VkExtent2D size) {
        m_size = size;
        m_device->waitForIdle();
        m_imgView->recreate(m_size, m_renderPass->getVkHandle());
        m_camera->resize((float)size.width, (float)size.height);
        m_primitiveRenderer->resize(size);
    }

    VkCommandBuffer Viewport::getVkCmdBuffer(int idx) {
        if (idx == -1)
            idx = m_currentFrameIdx;

        return m_cmdBuffers->at(idx)->getVkHandle();
    }

    void Viewport::createFences(size_t count) {
        m_fences.resize(count);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < count; i++) {
            if (vkCreateFence(m_device->device(), &fenceInfo, nullptr, &m_fences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create in flight fence!");
            }
        }
    }

    void Viewport::deleteFences() {
        for (auto &fence : m_fences) {
            vkWaitForFences(m_device->device(), 1, &fence, VK_TRUE, UINT64_MAX);
            vkResetFences(m_device->device(), 1, &fence);
        }
    }

    std::shared_ptr<Camera> Viewport::getCamera() {
        return m_camera;
    }

    void Viewport::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors) {
        const auto &camPos = m_camera->getPos();

        Vulkan::GridUniforms gridUniforms{};
        gridUniforms.zoom = m_camera->getZoom();
        gridUniforms.cameraOffset = glm::vec2({-camPos.x, camPos.y});
        gridUniforms.gridMinorColor = colors.minorColor;
        gridUniforms.gridMajorColor = colors.majorColor;
        gridUniforms.axisXColor = colors.axisXColor;
        gridUniforms.axisYColor = colors.axisYColor;
        gridUniforms.resolution = glm::vec2(m_camera->getSize());

        m_primitiveRenderer->updateUniformBuffer(gridUniforms);
        m_primitiveRenderer->drawGrid(pos, size, id, gridUniforms);
    }

    void Viewport::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id, QuadRenderProperties properties) {
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

    void Viewport::texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                const std::shared_ptr<Vulkan::VulkanTexture> &texture,
                                const glm::vec4 &tintColor, int id, QuadRenderProperties properties) {
        m_primitiveRenderer->drawTexturedQuad(
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

    void Viewport::circle(const glm::vec3 &center, const float radius,
                          const glm::vec4 &color, const int id, float innerRadius) {
        m_primitiveRenderer->drawCircle(center, radius, color, id, innerRadius);
    }

    void Viewport::msdfText(const std::string &text, const glm::vec3 &pos, const size_t size,
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

        m_primitiveRenderer->updateTextUniforms(textUniforms);
        m_primitiveRenderer->drawText(opaqueInstances, translucentInstances);
    }

    void Viewport::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        m_pathRenderer->beginPathMode(startPos, weight, color, static_cast<int>(id));
    }

    void Viewport::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        m_pathRenderer->endPathMode(closePath, genFill, fillColor, genStroke);
    }

    void Viewport::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        m_pathRenderer->pathLineTo(pos, size, color, id);
    }

    void Viewport::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                     float weight, const glm::vec4 &color, int id) {
        m_pathRenderer->pathCubicBeizerTo(end, controlPoint1, controlPoint2, weight, color, id);
    }

    void Viewport::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        m_pathRenderer->pathQuadBeizerTo(end, controlPoint, weight, color, id);
    }

    glm::vec2 Viewport::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        float xSize = 0;
        auto msdfFont = Assets::AssetManager::instance().get(Assets::Fonts::robotoMsdf);
        float ySize = msdfFont->getLineHeight();

        for (auto &ch : str) {
            auto chInfo = msdfFont->getCharacterData(ch);
            xSize += chInfo.advance;
        }
        return glm::vec2({xSize, ySize}) * renderSize;
    }

    uint64_t Viewport::getViewportTexture() {
        return (uint64_t)m_imgView->getDescriptorSet();
    }
}; // namespace Bess::Canvas
