#include "scene/renderer/vulkan/vulkan_framebuffer.h"
#include "common/log.h"
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice& device, VkExtent2D extent, VkRenderPass renderPass, 
                                       VkFormat colorFormat, VkFormat depthFormat)
        : m_device(device), m_extent(extent), m_renderPass(renderPass), 
          m_colorFormat(colorFormat), m_depthFormat(depthFormat) {
        createColorImage();
        createDepthImage();
        createFramebuffer();
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        cleanup();
    }

    VulkanFramebuffer::VulkanFramebuffer(VulkanFramebuffer&& other) noexcept
        : m_device(other.m_device),
          m_extent(other.m_extent),
          m_renderPass(other.m_renderPass),
          m_colorFormat(other.m_colorFormat),
          m_depthFormat(other.m_depthFormat),
          m_colorImage(other.m_colorImage),
          m_colorImageMemory(other.m_colorImageMemory),
          m_colorImageView(other.m_colorImageView),
          m_depthImage(other.m_depthImage),
          m_depthImageMemory(other.m_depthImageMemory),
          m_depthImageView(other.m_depthImageView),
          m_framebuffer(other.m_framebuffer) {
        other.m_colorImage = VK_NULL_HANDLE;
        other.m_colorImageMemory = VK_NULL_HANDLE;
        other.m_colorImageView = VK_NULL_HANDLE;
        other.m_depthImage = VK_NULL_HANDLE;
        other.m_depthImageMemory = VK_NULL_HANDLE;
        other.m_depthImageView = VK_NULL_HANDLE;
        other.m_framebuffer = VK_NULL_HANDLE;
    }

    VulkanFramebuffer& VulkanFramebuffer::operator=(VulkanFramebuffer&& other) noexcept {
        if (this != &other) {
            cleanup();
            
            m_extent = other.m_extent;
            m_renderPass = other.m_renderPass;
            m_colorFormat = other.m_colorFormat;
            m_depthFormat = other.m_depthFormat;
            m_colorImage = other.m_colorImage;
            m_colorImageMemory = other.m_colorImageMemory;
            m_colorImageView = other.m_colorImageView;
            m_depthImage = other.m_depthImage;
            m_depthImageMemory = other.m_depthImageMemory;
            m_depthImageView = other.m_depthImageView;
            m_framebuffer = other.m_framebuffer;

            other.m_colorImage = VK_NULL_HANDLE;
            other.m_colorImageMemory = VK_NULL_HANDLE;
            other.m_colorImageView = VK_NULL_HANDLE;
            other.m_depthImage = VK_NULL_HANDLE;
            other.m_depthImageMemory = VK_NULL_HANDLE;
            other.m_depthImageView = VK_NULL_HANDLE;
            other.m_framebuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanFramebuffer::resize(VkExtent2D newExtent) {
        if (m_extent.width == newExtent.width && m_extent.height == newExtent.height) {
            return;
        }

        cleanup();
        m_extent = newExtent;
        createColorImage();
        createDepthImage();
        createFramebuffer();
    }

    void VulkanFramebuffer::cleanup() {
        if (m_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device.device(), m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
        }

        if (m_colorImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.device(), m_colorImageView, nullptr);
            m_colorImageView = VK_NULL_HANDLE;
        }

        if (m_colorImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device.device(), m_colorImage, nullptr);
            m_colorImage = VK_NULL_HANDLE;
        }

        if (m_colorImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device.device(), m_colorImageMemory, nullptr);
            m_colorImageMemory = VK_NULL_HANDLE;
        }

        if (m_depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.device(), m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }

        if (m_depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device.device(), m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }

        if (m_depthImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device.device(), m_depthImageMemory, nullptr);
            m_depthImageMemory = VK_NULL_HANDLE;
        }
    }

    void VulkanFramebuffer::createColorImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_colorFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device.device(), &imageInfo, nullptr, &m_colorImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create color image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device.device(), m_colorImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device.device(), &allocInfo, nullptr, &m_colorImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate color image memory!");
        }

        vkBindImageMemory(m_device.device(), m_colorImage, m_colorImageMemory, 0);

        createImageView(m_colorImage, m_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, m_colorImageView);
    }

    void VulkanFramebuffer::createDepthImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device.device(), &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device.device(), m_depthImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device.device(), &allocInfo, nullptr, &m_depthImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate depth image memory!");
        }

        vkBindImageMemory(m_device.device(), m_depthImage, m_depthImageMemory, 0);

        createImageView(m_depthImage, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, m_depthImageView);
    }

    void VulkanFramebuffer::createFramebuffer() {
        std::array<VkImageView, 2> attachments = {
            m_colorImageView,
            m_depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device.device(), &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    void VulkanFramebuffer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView) {
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

        if (vkCreateImageView(m_device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    void VulkanFramebuffer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = m_device.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        m_device.endSingleTimeCommands(commandBuffer);
    }

    std::vector<uint8_t> VulkanFramebuffer::readColorPixels(int x, int y, int width, int height) {
        // TODO: Implement pixel reading from color attachment
        // This would require creating a staging buffer and copying from the color image
        BESS_WARN("VulkanFramebuffer::readColorPixels not yet implemented");
        return std::vector<uint8_t>();
    }

    std::vector<int32_t> VulkanFramebuffer::readIntPixels(int x, int y, int width, int height) {
        // TODO: Implement pixel reading for hover detection
        // This would require reading from a separate ID attachment
        BESS_WARN("VulkanFramebuffer::readIntPixels not yet implemented");
        return std::vector<int32_t>();
    }

    int32_t VulkanFramebuffer::readIntPixel(int x, int y) {
        // TODO: Implement single pixel reading for hover detection
        BESS_WARN("VulkanFramebuffer::readIntPixel not yet implemented");
        return -1;
    }

    bool VulkanFramebuffer::saveToPNG(const std::string& path) {
        // TODO: Implement PNG saving
        // This would require reading the color image and encoding as PNG
        BESS_WARN("VulkanFramebuffer::saveToPNG not yet implemented");
        return false;
    }

} // namespace Bess::Renderer2D::Vulkan
