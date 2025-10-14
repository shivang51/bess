#include "scene/viewport.h"
#include "common/log.h"
#include "scene/artist/nodes_artist.h"
#include "scene/artist/schematic_artist.h"
#include "scene/scene.h"
#include "vulkan_core.h"
#include <cstdint>
#include <memory>
#include <vulkan/vulkan_core.h>

namespace Bess::Canvas {

    constexpr size_t maxFrames = Bess::Vulkan::VulkanCore::MAX_FRAMES_IN_FLIGHT;

    Viewport::Viewport(const std::shared_ptr<Vulkan::VulkanDevice> &device, VkFormat imgFormat, VkExtent2D size)
        : m_device(device), m_imgFormat(imgFormat), m_size(size) {

        m_cmdBuffers = std::make_unique<Vulkan::VulkanCommandBuffers>(m_device, maxFrames);

        m_camera = std::make_shared<Camera>(size.width, size.height);

        m_renderPass = std::make_shared<Vulkan::VulkanOffscreenRenderPass>(m_device, m_imgFormat, m_pickingIdFormat);

        m_imgView = std::make_unique<Vulkan::VulkanImageView>(m_device, m_imgFormat, m_pickingIdFormat, size);
        m_imgView->createFramebuffer(m_renderPass->getVkHandle());

        // Create straight color image for post-processing
        m_straightColorImageView = std::make_unique<Vulkan::VulkanImageView>(m_device, m_imgFormat, size,
                                                                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // Create post-processing pipeline
        m_postprocessPipeline = std::make_unique<Vulkan::VulkanPostprocessPipeline>(m_device, m_imgFormat);
        m_postprocessPipeline->createDescriptorSet(m_imgView->getImageView(), m_imgView->getSampler());

        createPickingResources();
        createFences(maxFrames);

        m_mousePickingData = {};
        m_mousePickingData.ids = {-1};

        m_artistManager = std::make_shared<ArtistManager>(m_device, m_renderPass, size);

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
        m_postprocessPipeline.reset();
        m_straightColorImageView.reset();
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

        m_artistManager->getCurrentArtist()->begin(cmdBuffer, m_camera, frameIdx);
    }

    VkCommandBuffer Viewport::end() {
        m_artistManager->getCurrentArtist()->end();
        m_renderPass->end();

        if (m_mousePickingData.pending)
            copyIdForPicking();

        // Perform post-processing to un-premultiply alpha
        performPostProcessing();

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
        m_straightColorImageView->recreate(m_size, VK_NULL_HANDLE);
        m_camera->resize((float)size.width, (float)size.height);
        m_artistManager->getSchematicArtist()->resize(size);
        m_artistManager->getNodesArtist()->resize(size);
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
            vkDestroyFence(m_device->device(), fence, nullptr);
        }
    }

    std::shared_ptr<Camera> Viewport::getCamera() {
        return m_camera;
    }

    uint64_t Viewport::getViewportTexture() {
        return (uint64_t)m_straightColorImageView->getDescriptorSet();
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
                count = std::max(count, (uint32_t)1);
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

    void Viewport::performPostProcessing() {
        const VkCommandBuffer cmd = m_cmdBuffers->at(m_currentFrameIdx)->getVkHandle();

        // Transition offscreen image to shader read layout
        transitionImageLayout(cmd, m_imgView->getImage(), m_imgFormat,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Update descriptor set to use the correct image layout
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_imgView->getImageView();
        imageInfo.sampler = m_imgView->getSampler();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_postprocessPipeline->getDescriptorSet();
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);

        // Transition straight color image to color attachment layout
        transitionImageLayout(cmd, m_straightColorImageView->getImage(), m_imgFormat,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Create framebuffer for post-processing
        VkImageView straightColorImageView = m_straightColorImageView->getImageView();
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_postprocessPipeline->getRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &straightColorImageView;
        framebufferInfo.width = m_size.width;
        framebufferInfo.height = m_size.height;
        framebufferInfo.layers = 1;

        VkFramebuffer postprocessFramebuffer;
        if (vkCreateFramebuffer(m_device->device(), &framebufferInfo, nullptr, &postprocessFramebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create postprocess framebuffer!");
        }

        // Begin post-processing render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_postprocessPipeline->getRenderPass();
        renderPassInfo.framebuffer = postprocessFramebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_size;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline and descriptor set
        VkDescriptorSet descriptorSet = m_postprocessPipeline->getDescriptorSet();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postprocessPipeline->getPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postprocessPipeline->getPipelineLayout(),
                                0, 1, &descriptorSet, 0, nullptr);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_size.width);
        viewport.height = static_cast<float>(m_size.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_size;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Draw fullscreen quad
        vkCmdDraw(cmd, 4, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        // Transition straight color image to shader read layout
        transitionImageLayout(cmd, m_straightColorImageView->getImage(), m_imgFormat,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Clean up temporary framebuffer
        vkDestroyFramebuffer(m_device->device(), postprocessFramebuffer, nullptr);
    }

}; // namespace Bess::Canvas
