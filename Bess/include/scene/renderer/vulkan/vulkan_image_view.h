#pragma once

#include "device.h"
#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanImageView {
      public:
        VulkanImageView(const std::shared_ptr<VulkanDevice> &device,
                        VkFormat format,
                        VkExtent2D extent,
                        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        ~VulkanImageView();

        VulkanImageView(const VulkanImageView &) = delete;
        VulkanImageView &operator=(const VulkanImageView &) = delete;
        VulkanImageView(VulkanImageView &&other) noexcept;
        VulkanImageView &operator=(VulkanImageView &&other) noexcept;

        VkImage getImage() const { return m_image; }
        VkImageView getImageView() const { return m_imageView; }
        VkFormat getFormat() const { return m_format; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFramebuffer getFramebuffer() const { return m_framebuffer; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
        VkSampler getSampler() const { return m_sampler; }

        void createFramebuffer(VkRenderPass renderPass);
        void createSampler();
        void createDescriptorSet();

      private:
        void createImage();
        void createImageView();

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_format;
        VkExtent2D m_extent;
        VkImageUsageFlags m_usage;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
