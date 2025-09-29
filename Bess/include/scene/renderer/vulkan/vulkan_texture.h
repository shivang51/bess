#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>
#include "glm.hpp"

namespace Bess::Renderer2D::Vulkan {

    class VulkanDevice;

    class VulkanTexture {
    public:
        VulkanTexture(VulkanDevice& device, const std::string& path);
        VulkanTexture(VulkanDevice& device, uint32_t width, uint32_t height, VkFormat format, const void* data = nullptr);
        ~VulkanTexture();

        // Delete copy constructor and assignment operator
        VulkanTexture(const VulkanTexture&) = delete;
        VulkanTexture& operator=(const VulkanTexture&) = delete;

        // Move constructor and assignment operator
        VulkanTexture(VulkanTexture&& other) noexcept;
        VulkanTexture& operator=(VulkanTexture&& other) noexcept;

        VkImage getImage() const { return m_image; }
        VkImageView getImageView() const { return m_imageView; }
        VkSampler getSampler() const { return m_sampler; }
        VkFormat getFormat() const { return m_format; }
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

    private:
        VulkanDevice& m_device;
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                        VkImage& image, VkDeviceMemory& imageMemory);
        void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
        void createTextureSampler();
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    };

} // namespace Bess::Renderer2D::Vulkan
