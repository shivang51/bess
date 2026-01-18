#pragma once
#include "bess_vulkan_api.h"
#include "device.h"
#include "glm.hpp"
#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Vulkan {

    class VulkanDevice;

    class BESS_VULKAN_API VulkanOffscreenRenderPass {
      public:
        VulkanOffscreenRenderPass(const std::shared_ptr<VulkanDevice> &device,
                                  VkFormat colorFormat,
                                  VkFormat pickingFormat = VK_FORMAT_R32G32_UINT,
                                  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT);
        ~VulkanOffscreenRenderPass();

        VulkanOffscreenRenderPass(const VulkanOffscreenRenderPass &) = delete;
        VulkanOffscreenRenderPass &operator=(const VulkanOffscreenRenderPass &) = delete;
        VulkanOffscreenRenderPass(VulkanOffscreenRenderPass &&other) noexcept;
        VulkanOffscreenRenderPass &operator=(VulkanOffscreenRenderPass &&other) noexcept;

        void begin(VkCommandBuffer cmdBuffer, VkFramebuffer framebuffer, VkExtent2D extent,
                   const glm::vec4 &clearColor = glm::vec4(1.0F, 0.0F, 1.0F, 1.0F),
                   const glm::uvec2 &clearPickingId = {0, 0});
        void end();

        VkRenderPass getVkHandle() const { return m_renderPass; }
        VkFormat colorFormat() const { return m_colorFormat; }
        VkFormat pickingFormat() const { return m_pickingFormat; }
        VkFormat depthFormat() const { return m_depthFormat; }

      private:
        void createRenderPass();

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_colorFormat;
        VkFormat m_pickingFormat;
        VkFormat m_depthFormat;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkCommandBuffer m_recordingCmdBuffer = VK_NULL_HANDLE;
    };

} // namespace Bess::Vulkan
