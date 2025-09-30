#include "scene/renderer/vulkan/command_buffer.h"
#include "common/log.h"
#include "imgui_impl_vulkan.h"
#include "imgui.h"
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {
    VulkanCommandBuffer::~VulkanCommandBuffer() {
    }


    VkCommandPool VulkanCommandBuffer::s_commandPool;
    std::vector<VkCommandBuffer> VulkanCommandBuffer::s_commandBuffers;


    std::vector<std::shared_ptr<VulkanCommandBuffer>> VulkanCommandBuffer::createCommandBuffers(const std::shared_ptr<VulkanDevice> &device, const size_t count) {
        const QueueFamilyIndices queueFamilyIndices = device->queueFamilyIndices();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device->device(), &poolInfo, nullptr, &s_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        s_commandBuffers.resize(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = s_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(s_commandBuffers.size());

        if (vkAllocateCommandBuffers(device->device(), &allocInfo, s_commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        std::vector<std::shared_ptr<VulkanCommandBuffer>> commandBuffers;
        commandBuffers.reserve(count);
        for (size_t i = 0; i < count; i++) {
            auto cmdBufferObj = std::make_shared<VulkanCommandBuffer>();
            cmdBufferObj->m_vkCmdBufferHandel = s_commandBuffers[i];
            commandBuffers.emplace_back(std::move(cmdBufferObj));
        }

        return commandBuffers;
    }

    void VulkanCommandBuffer::cleanCommandBuffers(const std::shared_ptr<VulkanDevice> &device) {
        vkFreeCommandBuffers(device->device(), s_commandPool, s_commandBuffers.size(), s_commandBuffers.data());
        if (s_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device->device(), s_commandPool, nullptr);
        }
    }

    void VulkanCommandBuffer::recordCommandBuffer(const VkCommandBuffer commandBuffer, uint32_t imageIndex, const VkRenderPass renderPass, const VkFramebuffer framebuffer, const VkExtent2D extent, VkPipelineLayout pipelineLayout) const {
    }

    VkCommandBuffer VulkanCommandBuffer::getVkHandle() const {
        return m_vkCmdBufferHandel;
    }

    VkCommandBuffer* VulkanCommandBuffer::getVkHandlePtr() {
        return &m_vkCmdBufferHandel;
    }

    VkCommandBuffer VulkanCommandBuffer::beginRecording() const {
        vkResetCommandBuffer(m_vkCmdBufferHandel, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(m_vkCmdBufferHandel, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        return m_vkCmdBufferHandel;
    }

    void VulkanCommandBuffer::endRecording() const {
        if (vkEndCommandBuffer(m_vkCmdBufferHandel) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }
} // namespace Bess::Renderer2D::Vulkan