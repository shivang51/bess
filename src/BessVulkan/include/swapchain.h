#pragma once

#include "device.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Vulkan {

    class VulkanSwapchain {
      public:
        VulkanSwapchain(VkInstance instance, std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, VkExtent2D windowExtent);
        VulkanSwapchain(VkInstance instance, std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, VkExtent2D windowExtent, VkSwapchainKHR oldSwapchain);
        ~VulkanSwapchain();

        // Delete copy constructor and assignment operator
        VulkanSwapchain(const VulkanSwapchain &) = delete;
        VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

        // Move constructor and assignment operator
        VulkanSwapchain(VulkanSwapchain &&other) noexcept;
        VulkanSwapchain &operator=(VulkanSwapchain &&other) noexcept;

        void createFramebuffers(VkRenderPass renderPass);
        void cleanup();

        VkSwapchainKHR swapchain() const { return m_swapchain; }
        const std::vector<VkImage> &images() const { return m_images; }
        VkFormat imageFormat() const { return m_imageFormat; }
        VkExtent2D extent() const { return m_extent; }
        const std::vector<VkImageView> &imageViews() const { return m_imageViews; }
        const std::vector<VkFramebuffer> &framebuffers() const { return m_framebuffers; }
        uint32_t imageCount() const { return static_cast<uint32_t>(m_images.size()); }

      private:
        void createSwapchain();
        void createSwapchain(VkSwapchainKHR oldSwapchain);
        void createImageViews();

        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) const;
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const;
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, VkExtent2D windowExtent) const;

        VkInstance m_instance;
        std::shared_ptr<VulkanDevice> m_device;
        VkSurfaceKHR m_surface;
        VkExtent2D m_windowExtent;

        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> m_images;
        std::vector<VkFramebuffer> m_framebuffers;
        VkFormat m_imageFormat;
        VkExtent2D m_extent;
        std::vector<VkImageView> m_imageViews;
    };

} // namespace Bess::Vulkan
