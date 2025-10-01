#pragma once

#include "device.h"
#include "glm.hpp"
#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanOffscreenRenderPass {
      public:
        VulkanOffscreenRenderPass(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat, VkFormat pickingFormat = VK_FORMAT_R32_SINT);
        ~VulkanOffscreenRenderPass();

        VulkanOffscreenRenderPass(const VulkanOffscreenRenderPass &) = delete;
        VulkanOffscreenRenderPass &operator=(const VulkanOffscreenRenderPass &) = delete;
        VulkanOffscreenRenderPass(VulkanOffscreenRenderPass &&other) noexcept;
        VulkanOffscreenRenderPass &operator=(VulkanOffscreenRenderPass &&other) noexcept;

        VkRenderPass getVkHandle() const { return m_renderPass; }

        void begin(VkCommandBuffer cmdBuffer,
                   VkFramebuffer framebuffer,
                   VkExtent2D extent,
                   const glm::vec4 &clearColor = glm::vec4(1.0F, 0.0F, 1.0F, 1.0F), // Pink clear color
                   int clearPickingId = -1); // Clear picking ID (-1 = no object)

        void end();

      private:
        void createRenderPass();

        VkCommandBuffer m_recordingCmdBuffer = VK_NULL_HANDLE;

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_colorFormat;
        VkFormat m_pickingFormat;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
