#pragma once
#include "bess_vulkan_api.h"
#include "device.h"
#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Vulkan {

    class BESS_VULKAN_API VulkanImageView {
      public:
        VulkanImageView(const std::shared_ptr<VulkanDevice> &device,
                        VkFormat format,
                        VkExtent2D extent,
                        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // Constructor for dual attachments (color + picking)
        VulkanImageView(const std::shared_ptr<VulkanDevice> &device,
                        VkFormat colorFormat,
                        VkFormat pickingFormat,
                        VkExtent2D extent);
        ~VulkanImageView();

        VulkanImageView(const VulkanImageView &) = delete;
        VulkanImageView &operator=(const VulkanImageView &) = delete;
        VulkanImageView(VulkanImageView &&other) noexcept;
        VulkanImageView &operator=(VulkanImageView &&other) noexcept;

        VkImage getImage() const { return m_image; }
        VkImage getMsaaImage() const { return m_msaaImage; }
        VkImageView getImageView() const { return m_imageView; }
        VkImageView getMsaaImageView() const { return m_msaaImageView; }
        VkFormat getFormat() const { return m_format; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFramebuffer getFramebuffer() const { return m_framebuffer; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
        VkSampler getSampler() const { return m_sampler; }

        // Picking attachment accessors
        VkImage getPickingImage() const { return m_pickingImage; }
        VkImage getMsaaPickingImage() const { return m_msaaPickingImage; }
        VkImageView getPickingImageView() const { return m_pickingImageView; }
        VkImageView getMsaaPickingImageView() const { return m_msaaPickingImageView; }
        VkFormat getPickingFormat() const { return m_pickingFormat; }

        void createFramebuffer(VkRenderPass renderPass);
        void createSampler();
        void createDescriptorSet();
        // Recreate image, image view, and framebuffer for a new extent
        void recreate(VkExtent2D extent, VkRenderPass renderPass);

        // Check if this image view has picking attachments
        bool hasPickingAttachments() const { return m_hasPickingAttachments; }

      private:
        void createImage();
        void createMsaaImage();
        void createImageView();
        void createMsaaImageView();
        void createPickingImage();
        void createMsaaPickingImage();
        void createPickingImageView();
        void createMsaaPickingImageView();
        void createDepthImage();
        void createDepthImageView();

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_format;
        VkFormat m_pickingFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent;
        VkImageUsageFlags m_usage;
        bool m_hasPickingAttachments = false;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkImage m_msaaImage = VK_NULL_HANDLE;
        VkDeviceMemory m_msaaImageMemory = VK_NULL_HANDLE;
        VkImageView m_msaaImageView = VK_NULL_HANDLE;

        // Picking attachments
        VkImage m_pickingImage = VK_NULL_HANDLE;
        VkDeviceMemory m_pickingImageMemory = VK_NULL_HANDLE;
        VkImageView m_pickingImageView = VK_NULL_HANDLE;
        VkImage m_msaaPickingImage = VK_NULL_HANDLE;
        VkDeviceMemory m_msaaPickingImageMemory = VK_NULL_HANDLE;
        VkImageView m_msaaPickingImageView = VK_NULL_HANDLE;
        // Depth attachment
        VkImage m_depthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
        VkImageView m_depthImageView = VK_NULL_HANDLE;

        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    };

} // namespace Bess::Vulkan
