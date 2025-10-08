#include "scene/viewport.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "scene/scene.h"
#include <cstdint>
#include <memory>
#include <vulkan/vulkan_core.h>

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

        createPickingResources();
        createFences(framesInFlight);

        m_mousePickingData = {};
        m_mousePickingData.ids = {-1};

        m_artistManager = std::make_shared<ArtistManager>(std::shared_ptr<Viewport>(this, [](Viewport *) {}));

        auto cmd = m_device->beginSingleTimeCommands();
        transitionImageLayout(cmd, m_imgView->getImage(), m_imgView->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        m_device->endSingleTimeCommands(cmd);
    }

    Viewport::~Viewport() {
        m_device->waitForIdle();

        m_artistManager->destroy();

        deleteFences();

        cleanupPickingResources();
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
        m_pathRenderer->updateUniformBuffer(ubo);
        m_primitiveRenderer->updateUBO(ubo);
    }

    VkCommandBuffer Viewport::end() {
        m_pathRenderer->endFrame(); // important to end path renderer first for now (temp fix for alpha blending)
        m_primitiveRenderer->endFrame();
        m_renderPass->end();

        if (m_mousePickingData.pending)
            copyIdForPicking();

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
        m_pathRenderer->resize(size);
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

    void Viewport::texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                const std::shared_ptr<Vulkan::SubTexture> &texture,
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

    void Viewport::setPickingCoord(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
        m_mousePickingData.startPos = {x, y};
        m_mousePickingData.extent = {w, h};
        m_mousePickingData.pending = true;
    }

    std::vector<int32_t> Viewport::getPickingIdsResult() {
        if (m_pickingCopyInFlight) {
            VkFence fence = m_fences[m_pickingCopyRecordedFrameIdx];
            if (vkGetFenceStatus(m_device->device(), fence) == VK_SUCCESS) {
                auto count = m_mousePickingData.extent.width * m_mousePickingData.extent.height;
                void *data = nullptr;
                vkMapMemory(m_device->device(), m_pickingStagingBufferMemory, 0,
                            sizeof(int32_t) * count, 0, &data);

                auto &ids = m_mousePickingData.ids;
                ids.clear();
                ids.resize(count);
                std::memcpy(ids.data(), data, sizeof(int32_t) * count);

                vkUnmapMemory(m_device->device(), m_pickingStagingBufferMemory);
                m_pickingCopyInFlight = false;
                m_mousePickingData.pending = false;
            }
        }

        return m_mousePickingData.ids;
    }

    void Viewport::createPickingResources() {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(int);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_pickingStagingBuffer) != VK_SUCCESS) {
            BESS_ERROR("Failed to create picking staging buffer");
            return;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_pickingStagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_pickingStagingBufferMemory) != VK_SUCCESS) {
            BESS_ERROR("Failed to allocate picking staging buffer memory");
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            return;
        }

        vkBindBufferMemory(m_device->device(), m_pickingStagingBuffer, m_pickingStagingBufferMemory, 0);
    }

    void Viewport::cleanupPickingResources() {
        if (m_pickingStagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            m_pickingStagingBuffer = VK_NULL_HANDLE;
        }

        if (m_pickingStagingBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->device(), m_pickingStagingBufferMemory, nullptr);
            m_pickingStagingBufferMemory = VK_NULL_HANDLE;
        }
    }

    void Viewport::copyIdForPicking() {
        const VkCommandBuffer cmd = m_cmdBuffers->at(m_currentFrameIdx)->getVkHandle();
        const VkImage idImage = m_imgView->getPickingImage(); // resolve single-sample image
        if (idImage != VK_NULL_HANDLE) {
            if (m_pickingStagingBuffer == VK_NULL_HANDLE) {
                createPickingResources();
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = idImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            // Clamp coordinates
            VkExtent2D extent = m_imgView->getExtent();
            const auto px = std::max((int32_t)0, (int32_t)std::min(m_mousePickingData.startPos.width, extent.width - 1));
            const auto py = std::max((int32_t)0, (int32_t)std::min(m_mousePickingData.startPos.height, extent.height - 1));

            const auto ex = std::max((uint32_t)1, std::min(m_mousePickingData.extent.width, extent.width));
            const auto ey = std::max((uint32_t)1, std::min(m_mousePickingData.extent.height, extent.height));

            VkDeviceSize requiredSize = (uint64_t)ex * ey * sizeof(int32_t);
            resizePickingBuffer(requiredSize);

            VkBufferImageCopy copy{};
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = 0;
            copy.imageSubresource.baseArrayLayer = 0;
            copy.imageSubresource.layerCount = 1;
            copy.imageOffset = {px, py, 0};
            copy.imageExtent = {ex, ey, 1};

            vkCmdCopyImageToBuffer(cmd, idImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   m_pickingStagingBuffer, 1, &copy);

            // Transition image back for next frame
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &barrier);

            m_pickingCopyInFlight = true;
            m_pickingCopyRecordedFrameIdx = m_currentFrameIdx;
        }
    }

    void Viewport::resizePickingBuffer(VkDeviceSize newSize) {
        if (newSize == 0) {
            BESS_WARN("[Viewport] Requested picking buffer size 0 — ignoring.");
            return;
        }

        if (m_pickingStagingBuffer != VK_NULL_HANDLE && newSize <= m_pickingStagingBufferSize) {
            return;
        }

        if (m_pickingStagingBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            vkFreeMemory(m_device->device(), m_pickingStagingBufferMemory, nullptr);
            m_pickingStagingBuffer = VK_NULL_HANDLE;
            m_pickingStagingBufferMemory = VK_NULL_HANDLE;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = newSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &m_pickingStagingBuffer) != VK_SUCCESS) {
            BESS_ERROR("[Viewport] Failed to create picking staging buffer");
            return;
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), m_pickingStagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_pickingStagingBufferMemory) != VK_SUCCESS) {
            BESS_ERROR("[Viewport] Failed to allocate picking staging buffer memory");
            vkDestroyBuffer(m_device->device(), m_pickingStagingBuffer, nullptr);
            m_pickingStagingBuffer = VK_NULL_HANDLE;
            return;
        }

        vkBindBufferMemory(m_device->device(), m_pickingStagingBuffer, m_pickingStagingBufferMemory, 0);

        m_pickingStagingBufferSize = newSize;
    }

    entt::registry &Viewport::getEnttRegistry() {
        return Scene::instance()->getEnttRegistry();
    }

    entt::entity Viewport::getEntityWithUuid(const UUID &uuid) {
        return Scene::instance()->getEntityWithUuid(uuid);
    }

    std::shared_ptr<ArtistManager> Viewport::getArtistManager() {
        return m_artistManager;
    }

    std::vector<unsigned char> Viewport::getPixelData() {
        VkDeviceSize byteSize = m_size.width * m_size.height * 4;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        VkCommandBuffer cmd = m_device->beginSingleTimeCommands();
        transitionImageLayout(cmd, m_imgView->getImage(), m_imgView->getFormat(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_size.width, m_size.height, 1};
        vkCmdCopyImageToBuffer(cmd, m_imgView->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

        transitionImageLayout(cmd, m_imgView->getImage(), m_imgFormat, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_device->endSingleTimeCommands(cmd);

        std::vector<unsigned char> out(byteSize);
        void *mapped = nullptr;
        vkMapMemory(m_device->device(), stagingMemory, 0, byteSize, 0, &mapped);
        std::memcpy(out.data(), mapped, static_cast<size_t>(byteSize));
        vkUnmapMemory(m_device->device(), stagingMemory);

        vkDestroyBuffer(m_device->device(), stagingBuffer, nullptr);
        vkFreeMemory(m_device->device(), stagingMemory, nullptr);
        return out;
    }

    void Viewport::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage{};
        VkPipelineStageFlags dstStage{};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void Viewport::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) const {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        vkBindBufferMemory(m_device->device(), buffer, bufferMemory, 0);
    }

}; // namespace Bess::Canvas
