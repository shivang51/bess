#pragma once

#include "device.h"
#include <string>
#include <vulkan/vulkan.h>
#include <vector>

namespace Bess::Renderer2D::Vulkan {

    class VulkanFramebuffer {
    public:
        VulkanFramebuffer(VulkanDevice& device, VkExtent2D extent, VkRenderPass renderPass, 
                         VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB, 
                         VkFormat depthFormat = VK_FORMAT_D32_SFLOAT);
        ~VulkanFramebuffer();

        VulkanFramebuffer(const VulkanFramebuffer&) = delete;
        VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;
        VulkanFramebuffer(VulkanFramebuffer&& other) noexcept;
        VulkanFramebuffer& operator=(VulkanFramebuffer&& other) noexcept;

        void resize(VkExtent2D newExtent);
        void cleanup();

        VkFramebuffer framebuffer() const { return m_framebuffer; }
        VkImageView colorImageView() const { return m_colorImageView; }
        VkImageView depthImageView() const { return m_depthImageView; }
        VkImage colorImage() const { return m_colorImage; }
        VkImage depthImage() const { return m_depthImage; }
        VkExtent2D extent() const { return m_extent; }
        VkFormat colorFormat() const { return m_colorFormat; }
        VkFormat depthFormat() const { return m_depthFormat; }

        // For reading pixel data (for hover detection, selection, etc.)
        std::vector<uint8_t> readColorPixels(int x, int y, int width, int height);
        std::vector<int32_t> readIntPixels(int x, int y, int width, int height);
        int32_t readIntPixel(int x, int y);

        // For saving to file
        bool saveToPNG(const std::string& path);

    private:
        void createColorImage();
        void createDepthImage();
        void createFramebuffer();
        void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView);
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

        VulkanDevice& m_device;
        VkExtent2D m_extent;
        VkRenderPass m_renderPass;
        VkFormat m_colorFormat;
        VkFormat m_depthFormat;

        VkImage m_colorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_colorImageMemory = VK_NULL_HANDLE;
        VkImageView m_colorImageView = VK_NULL_HANDLE;

        VkImage m_depthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
        VkImageView m_depthImageView = VK_NULL_HANDLE;

        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
