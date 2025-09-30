#pragma once

#include "device.h"

#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanRenderPass {
    public:
        VulkanRenderPass(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat, VkFormat depthFormat);
        ~VulkanRenderPass();

        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
        VulkanRenderPass(VulkanRenderPass&& other) noexcept;
        VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;

        VkRenderPass renderPass() const { return m_renderPass; }

    private:
        void createRenderPass();

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_colorFormat;
        VkFormat m_depthFormat;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
