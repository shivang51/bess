#include "scene/renderer/vulkan/command_buffer.h"
#include "common/log.h"
#include "imgui_impl_vulkan.h"
#include "imgui.h"
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    VulkanCommandBuffer::VulkanCommandBuffer(const std::shared_ptr<VulkanDevice> &device) : m_device(device) {
        createCommandPool();
        createCommandBuffers();
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
        }
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer &&other) noexcept
        : m_device(other.m_device),
          m_commandPool(other.m_commandPool),
          m_commandBuffers(std::move(other.m_commandBuffers)) {
        other.m_commandPool = VK_NULL_HANDLE;
    }

    VulkanCommandBuffer &VulkanCommandBuffer::operator=(VulkanCommandBuffer &&other) noexcept {
        if (this != &other) {
            if (m_commandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_device->device(), m_commandPool, nullptr);
            }

            m_commandPool = other.m_commandPool;
            m_commandBuffers = std::move(other.m_commandBuffers);

            other.m_commandPool = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanCommandBuffer::createCommandPool() {
        const QueueFamilyIndices queueFamilyIndices = m_device->queueFamilyIndices();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_device->device(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    void VulkanCommandBuffer::createCommandBuffers() {
        m_commandBuffers.resize(2); // MAX_FRAMES_IN_FLIGHT

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

        if (vkAllocateCommandBuffers(m_device->device(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void VulkanCommandBuffer::recordCommandBuffer(const VkCommandBuffer commandBuffer, uint32_t imageIndex, const VkRenderPass renderPass, const VkFramebuffer framebuffer, const VkExtent2D extent, VkPipelineLayout pipelineLayout) const {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = extent;

        constexpr VkClearValue clearColor = {{{0.0F, 0.0F, 0.0F, 1.0F}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->CmdListsCount > 0) {
            ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

} // namespace Bess::Renderer2D::Vulkan