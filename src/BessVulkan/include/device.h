#pragma once

#include "bess_vulkan_api.h"
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Vulkan {

    struct BESS_VULKAN_API QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class BESS_VULKAN_API VulkanDevice {
      public:
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice &) = delete;
        VulkanDevice &operator=(const VulkanDevice &) = delete;

        VulkanDevice(VulkanDevice &&other) noexcept;
        VulkanDevice &operator=(VulkanDevice &&other) noexcept;

        VkDevice device() const { return m_vkDevice; }
        VkPhysicalDevice physicalDevice() const { return m_vkPhysicalDevice; }
        QueueFamilyIndices queueFamilyIndices() const { return m_queueFamilyIndices; }
        VkQueue graphicsQueue() const { return m_graphicsQueue; }
        VkQueue presentQueue() const { return m_presentQueue; }

        VkCommandBuffer beginSingleTimeCommands() const;
        void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

        void waitForIdle();

        void submitCmdBuffers(const std::vector<VkCommandBuffer> &cmdBuffer, VkFence fence);

      private:
        void pickPhysicalDevice();
        void createLogicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device) const;
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

        VkInstance m_instance;
        VkSurfaceKHR m_surface;
        VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_vkDevice = VK_NULL_HANDLE;
        QueueFamilyIndices m_queueFamilyIndices;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
    };

} // namespace Bess::Vulkan
