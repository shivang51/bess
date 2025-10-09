#include "command_buffer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <stdexcept>

namespace Bess::Vulkan {
    VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer vkHandle) : m_vkCmdBufferHandel(vkHandle) {
    }

    VkCommandBuffer VulkanCommandBuffer::getVkHandle() const {
        return m_vkCmdBufferHandel;
    }

    VkCommandBuffer *VulkanCommandBuffer::getVkHandlePtr() {
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

    VulkanCommandBuffers::VulkanCommandBuffers(const std::shared_ptr<VulkanDevice> &device, size_t count) : m_device(device) {
        const QueueFamilyIndices queueFamilyIndices = device->queueFamilyIndices();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device->device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }

        m_commandBuffersVkHnd.resize(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(count);

        if (vkAllocateCommandBuffers(device->device(), &allocInfo, m_commandBuffersVkHnd.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        for (size_t i = 0; i < count; i++) {
            m_commandBuffers.emplace_back(std::move(std::make_shared<VulkanCommandBuffer>(m_commandBuffersVkHnd[i])));
        }
    }

    VulkanCommandBuffers::~VulkanCommandBuffers() {
        vkFreeCommandBuffers(m_device->device(), m_commandPool, m_commandBuffers.size(), m_commandBuffersVkHnd.data());
        vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
    }

    const std::vector<std::shared_ptr<VulkanCommandBuffer>> &VulkanCommandBuffers::getCmdBuffers() const {
        return m_commandBuffers;
    }

    std::shared_ptr<VulkanCommandBuffer> VulkanCommandBuffers::at(u_int32_t idx) {
        return m_commandBuffers[idx];
    }
} // namespace Bess::Vulkan
