#include "scene/renderer/vulkan/vulkan_texture.h"
#include "scene/renderer/vulkan/device.h"
#include <stdexcept>
#include <cstring>

namespace Bess::Renderer2D::Vulkan {

    VulkanTexture::VulkanTexture(VulkanDevice& device, const std::string& path)
        : m_device(device) {
        // TODO: Load texture from file
        // For now, create a 1x1 white texture
        uint32_t whitePixel = 0xFFFFFFFF;
        createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);
        
        createImageView(m_image, m_format, VK_IMAGE_ASPECT_COLOR_BIT);
        createTextureSampler();
        
        m_width = 1;
        m_height = 1;
    }

    VulkanTexture::VulkanTexture(VulkanDevice& device, const uint32_t width, const uint32_t height, const VkFormat format, const void* data)
        : m_device(device), m_width(width), m_height(height), m_format(format) {
        createImage(width, height, format, VK_IMAGE_TILING_OPTIMAL,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);
        
        createImageView(m_image, format, VK_IMAGE_ASPECT_COLOR_BIT);
        createTextureSampler();
    }

    VulkanTexture::~VulkanTexture() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device.device(), m_sampler, nullptr);
        }
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.device(), m_imageView, nullptr);
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device.device(), m_image, nullptr);
        }
        if (m_imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device.device(), m_imageMemory, nullptr);
        }
    }

    VulkanTexture::VulkanTexture(VulkanTexture&& other) noexcept
        : m_device(other.m_device),
          m_image(other.m_image),
          m_imageMemory(other.m_imageMemory),
          m_imageView(other.m_imageView),
          m_sampler(other.m_sampler),
          m_format(other.m_format),
          m_width(other.m_width),
          m_height(other.m_height) {
        other.m_image = VK_NULL_HANDLE;
        other.m_imageMemory = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
    }

    VulkanTexture& VulkanTexture::operator=(VulkanTexture&& other) noexcept {
        if (this != &other) {
            if (m_sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_device.device(), m_sampler, nullptr);
            }
            if (m_imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device.device(), m_imageView, nullptr);
            }
            if (m_image != VK_NULL_HANDLE) {
                vkDestroyImage(m_device.device(), m_image, nullptr);
            }
            if (m_imageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device.device(), m_imageMemory, nullptr);
            }
            
            m_image = other.m_image;
            m_imageMemory = other.m_imageMemory;
            m_imageView = other.m_imageView;
            m_sampler = other.m_sampler;
            m_format = other.m_format;
            m_width = other.m_width;
            m_height = other.m_height;

            other.m_image = VK_NULL_HANDLE;
            other.m_imageMemory = VK_NULL_HANDLE;
            other.m_imageView = VK_NULL_HANDLE;
            other.m_sampler = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanTexture::createImage(const uint32_t width, const uint32_t height, const VkFormat format, const VkImageTiling tiling,
                                    const VkImageUsageFlags usage, const VkMemoryPropertyFlags properties,
                                   VkImage& image, VkDeviceMemory& imageMemory) const {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device.device(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device.device(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(m_device.device(), image, imageMemory, 0);
    }

    void VulkanTexture::createImageView(const VkImage image, const VkFormat format, const VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device.device(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }
    }

    void VulkanTexture::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0F;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(m_device.device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void VulkanTexture::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) const {
        // TODO: Implement image layout transition
    }

    void VulkanTexture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
        // TODO: Implement buffer to image copy
    }

    uint32_t VulkanTexture::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

} // namespace Bess::Renderer2D::Vulkan
