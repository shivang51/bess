#pragma once

#include "device.h"

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {

    class VulkanCommandBuffer {
      public:
        VulkanCommandBuffer(VkCommandBuffer vkHandle);

        VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
        VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;

        VkCommandBuffer getVkHandle() const;
        VkCommandBuffer *getVkHandlePtr();

        VkCommandBuffer beginRecording() const;
        void endRecording() const;

      private:
        VkCommandBuffer m_vkCmdBufferHandel = VK_NULL_HANDLE;
    };

    class VulkanCommandBuffers {
      public:
        VulkanCommandBuffers(const std::shared_ptr<VulkanDevice> &device, size_t count);
        ~VulkanCommandBuffers();

        const std::vector<std::shared_ptr<VulkanCommandBuffer>> &getCmdBuffers() const;

        std::shared_ptr<VulkanCommandBuffer> at(u_int32_t idx);

      private:
        std::shared_ptr<VulkanDevice> m_device;
        VkCommandPool m_commandPool;
        std::vector<std::shared_ptr<VulkanCommandBuffer>> m_commandBuffers;
        std::vector<VkCommandBuffer> m_commandBuffersVkHnd;
    };
} // namespace Bess::Vulkan
