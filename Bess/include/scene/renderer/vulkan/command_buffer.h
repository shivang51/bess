#pragma once

#include "device.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Bess::Renderer2D::Vulkan {

    class VulkanCommandBuffer {
    public:
        VulkanCommandBuffer(VulkanDevice& device);
        ~VulkanCommandBuffer();

        // Delete copy constructor and assignment operator
        VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
        VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

        // Move constructor and assignment operator
        VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
        VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;

        void createCommandPool();
        void createCommandBuffers();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent);

        VkCommandPool commandPool() const { return m_commandPool; }
        const std::vector<VkCommandBuffer>& commandBuffers() const { return m_commandBuffers; }

    private:
        VulkanDevice& m_device;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_commandBuffers;
    };

} // namespace Bess::Renderer2D::Vulkan
