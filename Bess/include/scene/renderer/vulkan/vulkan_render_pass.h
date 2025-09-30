#pragma once

#include "device.h"

#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {
    class VulkanSwapchain;
    class VulkanCommandBuffer;

    class VulkanRenderPass {
    public:
        VulkanRenderPass(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat, VkFormat depthFormat);
        ~VulkanRenderPass();

        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
        VulkanRenderPass(VulkanRenderPass&& other) noexcept;
        VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;

        VkRenderPass getVkHandle() const { return m_renderPass; }

        void begin(VkCommandBuffer cmdBuffer,
            VkFramebuffer framebuffer,
            VkExtent2D extent,
            VkPipelineLayout pipelineLayout);

        void end();

    private:
        void createRenderPass();

        VkCommandBuffer m_recordingCmdBuffer = VK_NULL_HANDLE;

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_colorFormat;
        VkFormat m_depthFormat;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
