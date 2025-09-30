#pragma once

#include "device.h"

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanCommandBuffer {
    public:
        VulkanCommandBuffer() = default;
        ~VulkanCommandBuffer();

        VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
        VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

        static std::vector<std::shared_ptr<VulkanCommandBuffer>> createCommandBuffers(const std::shared_ptr<VulkanDevice> &device, size_t count);
        static void cleanCommandBuffers(const std::shared_ptr<VulkanDevice> &device);

        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer framebuffer, VkExtent2D extent, VkPipelineLayout pipelineLayout) const;

        VkCommandBuffer getVkHandle() const;
        VkCommandBuffer *getVkHandlePtr();

        VkCommandBuffer beginRecording() const;
        void endRecording() const;

    private:
        static VkCommandPool s_commandPool;
        static std::vector<VkCommandBuffer> s_commandBuffers;
        VkCommandBuffer m_vkCmdBufferHandel = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
