#include "vulkan_texture.h"
#include "device.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include <cstring>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {

    VulkanTexture::VulkanTexture(std::shared_ptr<VulkanDevice> device, const std::string &path)
        : m_device(std::move(device)) {
        int w = 0, h = 0, bpp = 0;
        stbi_uc *pixels = stbi_load(path.c_str(), &w, &h, &bpp, STBI_rgb_alpha);
        if (pixels == nullptr) {
            throw std::runtime_error("Failed to load texture from path: " + path);
        }
        m_width = static_cast<uint32_t>(w);
        m_height = static_cast<uint32_t>(h);
        m_format = VK_FORMAT_R8G8B8A8_UNORM;

        createImage(m_width, m_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);

        createImageView(m_image, m_format, VK_IMAGE_ASPECT_COLOR_BIT);
        createTextureSampler();

        setData(pixels, static_cast<size_t>(m_width) * m_height * 4);
        stbi_image_free(pixels);
    }

    VulkanTexture::VulkanTexture(std::shared_ptr<VulkanDevice> device, const uint32_t width, const uint32_t height, const VkFormat format, const void *data)
        : m_device(std::move(device)), m_width(width), m_height(height), m_format(format) {
        createImage(width, height, format, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);

        createImageView(m_image, format, VK_IMAGE_ASPECT_COLOR_BIT);
        createTextureSampler();

        if (data != nullptr) {
            setData(data, static_cast<size_t>(m_width) * m_height * 4);
        } else {
            std::vector<uint32_t> zeros(m_width * m_height, 0x000000FF);
            setData(zeros.data(), zeros.size() * sizeof(uint32_t));
        }
    }

    VulkanTexture::~VulkanTexture() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device->device(), m_sampler, nullptr);
        }
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device->device(), m_imageView, nullptr);
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device->device(), m_image, nullptr);
        }
        if (m_imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->device(), m_imageMemory, nullptr);
        }
    }

    VulkanTexture::VulkanTexture(VulkanTexture &&other) noexcept
        : m_device(std::move(other.m_device)),
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

    VulkanTexture &VulkanTexture::operator=(VulkanTexture &&other) noexcept {
        if (this != &other) {
            if (m_sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_device->device(), m_sampler, nullptr);
            }
            if (m_imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device->device(), m_imageView, nullptr);
            }
            if (m_image != VK_NULL_HANDLE) {
                vkDestroyImage(m_device->device(), m_image, nullptr);
            }
            if (m_imageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_imageMemory, nullptr);
            }
            m_device = std::move(other.m_device);
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
                                    VkImage &image, VkDeviceMemory &imageMemory) const {

        assert(m_device != nullptr);
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

        if (vkCreateImage(m_device->device(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->device(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(m_device->device(), image, imageMemory, 0);
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

        if (vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }
    }

    void VulkanTexture::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 2.0F;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.f;
        samplerInfo.maxLod = 0.f;

        if (vkCreateSampler(m_device->device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void VulkanTexture::transitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout) const {
        VkCommandBuffer cmd = m_device->beginSingleTimeCommands();

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

        VkPipelineStageFlags srcStage{};
        VkPipelineStageFlags dstStage{};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        m_device->endSingleTimeCommands(cmd);
    }

    void VulkanTexture::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
        VkCommandBuffer cmd = m_device->beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        m_device->endSingleTimeCommands(cmd);
    }

    void VulkanTexture::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                     VkBuffer &buffer, VkDeviceMemory &bufferMemory) const {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(m_device->device(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device->device(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, properties);
        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        vkBindBufferMemory(m_device->device(), buffer, bufferMemory, 0);
    }

    void VulkanTexture::setData(const void *data, size_t byteSize) {
        if (m_image == VK_NULL_HANDLE)
            return;
        if (byteSize == 0) {
            byteSize = static_cast<size_t>(m_width) * m_height * 4;
        }

        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        void *mapped = nullptr;
        vkMapMemory(m_device->device(), stagingMemory, 0, byteSize, 0, &mapped);
        std::memcpy(mapped, data, byteSize);
        vkUnmapMemory(m_device->device(), stagingMemory);

        transitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, m_image, m_width, m_height);
        transitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(m_device->device(), stagingBuffer, nullptr);
        vkFreeMemory(m_device->device(), stagingMemory, nullptr);
    }

    void VulkanTexture::resize(uint32_t width, uint32_t height, const void *data) {
        if (m_imageView != VK_NULL_HANDLE)
            vkDestroyImageView(m_device->device(), m_imageView, nullptr);
        if (m_image != VK_NULL_HANDLE)
            vkDestroyImage(m_device->device(), m_image, nullptr);
        if (m_imageMemory != VK_NULL_HANDLE)
            vkFreeMemory(m_device->device(), m_imageMemory, nullptr);

        m_width = width;
        m_height = height;

        createImage(m_width, m_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_imageMemory);
        createImageView(m_image, m_format, VK_IMAGE_ASPECT_COLOR_BIT);
        if (m_sampler == VK_NULL_HANDLE) {
            createTextureSampler();
        }

        if (data != nullptr) {
            setData(data, static_cast<size_t>(m_width) * m_height * 4);
        }
    }

    void VulkanTexture::saveToPath(const std::string &path) const {
        std::vector<unsigned char> rgba = getData();
        if (rgba.empty()) {
            // BESS_ERROR("[VulkanTexture] saveToPath: empty buffer");
            return;
        }
        stbi_flip_vertically_on_write(1);
        int result = stbi_write_png(path.c_str(), static_cast<int>(m_width), static_cast<int>(m_height), 4, rgba.data(), static_cast<int>(m_width * 4));
        if (result == 0) {
            // BESS_ERROR("[VulkanTexture] Failed to write file %s", path.c_str());
        }
    }

    std::vector<unsigned char> VulkanTexture::getData() const {
        VkDeviceSize byteSize = static_cast<VkDeviceSize>(m_width) * m_height * 4;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
        createBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer, stagingMemory);

        transitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkCommandBuffer cmd = m_device->beginSingleTimeCommands();
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};
        vkCmdCopyImageToBuffer(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);
        m_device->endSingleTimeCommands(cmd);

        transitionImageLayout(m_image, m_format, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        std::vector<unsigned char> out(byteSize);
        void *mapped = nullptr;
        vkMapMemory(m_device->device(), stagingMemory, 0, byteSize, 0, &mapped);
        std::memcpy(out.data(), mapped, static_cast<size_t>(byteSize));
        vkUnmapMemory(m_device->device(), stagingMemory);

        vkDestroyBuffer(m_device->device(), stagingBuffer, nullptr);
        vkFreeMemory(m_device->device(), stagingMemory, nullptr);
        return out;
    }

    uint32_t VulkanTexture::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_device->physicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type!");
    }

} // namespace Bess::Vulkan
