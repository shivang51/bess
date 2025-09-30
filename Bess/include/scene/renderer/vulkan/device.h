#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    class VulkanDevice {
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

} // namespace Bess::Renderer2D::Vulkan
