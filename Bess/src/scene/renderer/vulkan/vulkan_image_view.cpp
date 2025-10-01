#include "scene/renderer/vulkan/vulkan_image_view.h"
#include "imgui_impl_vulkan.h"
#include <array>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D::Vulkan {

    VulkanImageView::VulkanImageView(const std::shared_ptr<VulkanDevice> &device,
                                     VkFormat format,
                                     VkExtent2D extent,
                                     VkImageUsageFlags usage)
        : m_device(device), m_format(format), m_extent(extent), m_usage(usage), m_hasPickingAttachments(false) {
        createImage();
        createMsaaImage();
        createImageView();
        createMsaaImageView();
        createSampler();
        createDescriptorSet();
    }
    
    VulkanImageView::VulkanImageView(const std::shared_ptr<VulkanDevice> &device,
                                     VkFormat colorFormat,
                                     VkFormat pickingFormat,
                                     VkExtent2D extent)
        : m_device(device), m_format(colorFormat), m_pickingFormat(pickingFormat), m_extent(extent), 
          m_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), m_hasPickingAttachments(true) {
        createImage();
        createMsaaImage();
        createImageView();
        createMsaaImageView();
        createPickingImage();
        createMsaaPickingImage();
        createPickingImageView();
        createMsaaPickingImageView();
        createSampler();
        createDescriptorSet();
    }

    VulkanImageView::~VulkanImageView() {
        if (m_device && m_device->device() != VK_NULL_HANDLE) {
            if (m_descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
            }
            if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
            }
            if (m_sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_device->device(), m_sampler, nullptr);
            }
            if (m_framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_device->device(), m_framebuffer, nullptr);
            }
            
            // Clean up picking attachments if they exist
            if (m_hasPickingAttachments) {
                if (m_msaaPickingImageView != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_device->device(), m_msaaPickingImageView, nullptr);
                }
                if (m_pickingImageView != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_device->device(), m_pickingImageView, nullptr);
                }
                if (m_msaaPickingImage != VK_NULL_HANDLE) {
                    vkDestroyImage(m_device->device(), m_msaaPickingImage, nullptr);
                }
                if (m_pickingImage != VK_NULL_HANDLE) {
                    vkDestroyImage(m_device->device(), m_pickingImage, nullptr);
                }
                if (m_msaaPickingImageMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_msaaPickingImageMemory, nullptr);
                }
                if (m_pickingImageMemory != VK_NULL_HANDLE) {
                    vkFreeMemory(m_device->device(), m_pickingImageMemory, nullptr);
                }
            }
            
            if (m_msaaImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device->device(), m_msaaImageView, nullptr);
            }
            if (m_imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device->device(), m_imageView, nullptr);
            }
            if (m_msaaImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_device->device(), m_msaaImage, nullptr);
            }
            if (m_image != VK_NULL_HANDLE) {
                vkDestroyImage(m_device->device(), m_image, nullptr);
            }
            if (m_msaaImageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_msaaImageMemory, nullptr);
            }
            if (m_imageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_imageMemory, nullptr);
            }
        }
    }

    VulkanImageView::VulkanImageView(VulkanImageView &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_format(other.m_format),
          m_pickingFormat(other.m_pickingFormat),
          m_extent(other.m_extent),
          m_usage(other.m_usage),
          m_hasPickingAttachments(other.m_hasPickingAttachments),
          m_image(other.m_image),
          m_imageMemory(other.m_imageMemory),
          m_imageView(other.m_imageView),
          m_msaaImage(other.m_msaaImage),
          m_msaaImageMemory(other.m_msaaImageMemory),
          m_msaaImageView(other.m_msaaImageView),
          m_pickingImage(other.m_pickingImage),
          m_pickingImageMemory(other.m_pickingImageMemory),
          m_pickingImageView(other.m_pickingImageView),
          m_msaaPickingImage(other.m_msaaPickingImage),
          m_msaaPickingImageMemory(other.m_msaaPickingImageMemory),
          m_msaaPickingImageView(other.m_msaaPickingImageView),
          m_framebuffer(other.m_framebuffer),
          m_sampler(other.m_sampler),
          m_descriptorSet(other.m_descriptorSet),
          m_descriptorPool(other.m_descriptorPool),
          m_descriptorSetLayout(other.m_descriptorSetLayout) {
        other.m_image = VK_NULL_HANDLE;
        other.m_imageMemory = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_msaaImage = VK_NULL_HANDLE;
        other.m_msaaImageMemory = VK_NULL_HANDLE;
        other.m_msaaImageView = VK_NULL_HANDLE;
        other.m_pickingImage = VK_NULL_HANDLE;
        other.m_pickingImageMemory = VK_NULL_HANDLE;
        other.m_pickingImageView = VK_NULL_HANDLE;
        other.m_msaaPickingImage = VK_NULL_HANDLE;
        other.m_msaaPickingImageMemory = VK_NULL_HANDLE;
        other.m_msaaPickingImageView = VK_NULL_HANDLE;
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_descriptorSet = VK_NULL_HANDLE;
        other.m_descriptorPool = VK_NULL_HANDLE;
        other.m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    VulkanImageView &VulkanImageView::operator=(VulkanImageView &&other) noexcept {
        if (this != &other) {
            if (m_device && m_device->device() != VK_NULL_HANDLE) {
                if (m_descriptorPool != VK_NULL_HANDLE) {
                    vkDestroyDescriptorPool(m_device->device(), m_descriptorPool, nullptr);
                }
                if (m_descriptorSetLayout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device->device(), m_descriptorSetLayout, nullptr);
                }
                if (m_sampler != VK_NULL_HANDLE) {
                    vkDestroySampler(m_device->device(), m_sampler, nullptr);
                }
                if (m_framebuffer != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(m_device->device(), m_framebuffer, nullptr);
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

            m_device = other.m_device;
            m_format = other.m_format;
            m_extent = other.m_extent;
            m_usage = other.m_usage;
            m_image = other.m_image;
            m_imageMemory = other.m_imageMemory;
            m_imageView = other.m_imageView;
            m_framebuffer = other.m_framebuffer;
            m_sampler = other.m_sampler;
            m_descriptorSet = other.m_descriptorSet;
            m_descriptorPool = other.m_descriptorPool;
            m_descriptorSetLayout = other.m_descriptorSetLayout;

            other.m_image = VK_NULL_HANDLE;
            other.m_imageMemory = VK_NULL_HANDLE;
            other.m_imageView = VK_NULL_HANDLE;
            other.m_framebuffer = VK_NULL_HANDLE;
            other.m_sampler = VK_NULL_HANDLE;
            other.m_descriptorSet = VK_NULL_HANDLE;
            other.m_descriptorPool = VK_NULL_HANDLE;
            other.m_descriptorSetLayout = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanImageView::createImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = m_usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device->device(), &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create offscreen image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->device(), m_image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(m_device->device(), m_image, m_imageMemory, 0);
    }

    void VulkanImageView::createMsaaImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // MSAA render target only
        imageInfo.samples = VK_SAMPLE_COUNT_4_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device->device(), &imageInfo, nullptr, &m_msaaImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->device(), m_msaaImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_msaaImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate MSAA image memory!");
        }

        vkBindImageMemory(m_device->device(), m_msaaImage, m_msaaImageMemory, 0);
    }

    void VulkanImageView::createImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view!");
        }
    }

    void VulkanImageView::createMsaaImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_msaaImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_msaaImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA image view!");
        }
    }

    void VulkanImageView::createFramebuffer(VkRenderPass renderPass) {
        if (m_hasPickingAttachments) {
            // Offscreen framebuffer has four attachments: [0] MSAA color, [1] resolve color, [2] MSAA picking, [3] resolve picking
            std::array<VkImageView, 4> attachments{m_msaaImageView, m_imageView, m_msaaPickingImageView, m_pickingImageView};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_extent.width;
            framebufferInfo.height = m_extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device->device(), &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create offscreen framebuffer with picking!");
            }
        } else {
            // Offscreen framebuffer has two attachments: [0] MSAA color, [1] resolve (single-sample)
            std::array<VkImageView, 2> attachments{m_msaaImageView, m_imageView};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_extent.width;
            framebufferInfo.height = m_extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device->device(), &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create offscreen framebuffer!");
            }
        }
    }

    void VulkanImageView::createSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0F;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0F;
        samplerInfo.minLod = 0.0F;
        samplerInfo.maxLod = 0.0F;

        if (vkCreateSampler(m_device->device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler!");
        }
    }

    void VulkanImageView::createDescriptorSet() {
        // Create descriptor set layout
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_device->device(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }

        // Create descriptor pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(m_device->device(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool!");
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkAllocateDescriptorSets(m_device->device(), &allocInfo, &m_descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        // Update descriptor set
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_imageView;
        imageInfo.sampler = m_sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);
    }

    void VulkanImageView::recreate(VkExtent2D extent, VkRenderPass renderPass) {
        // Destroy old framebuffer and image views and images
        if (m_framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device->device(), m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
        }
        
        // Destroy picking attachments if they exist
        if (m_hasPickingAttachments) {
            if (m_msaaPickingImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device->device(), m_msaaPickingImageView, nullptr);
                m_msaaPickingImageView = VK_NULL_HANDLE;
            }
            if (m_pickingImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device->device(), m_pickingImageView, nullptr);
                m_pickingImageView = VK_NULL_HANDLE;
            }
            if (m_msaaPickingImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_device->device(), m_msaaPickingImage, nullptr);
                m_msaaPickingImage = VK_NULL_HANDLE;
            }
            if (m_pickingImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_device->device(), m_pickingImage, nullptr);
                m_pickingImage = VK_NULL_HANDLE;
            }
            if (m_msaaPickingImageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_msaaPickingImageMemory, nullptr);
                m_msaaPickingImageMemory = VK_NULL_HANDLE;
            }
            if (m_pickingImageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device->device(), m_pickingImageMemory, nullptr);
                m_pickingImageMemory = VK_NULL_HANDLE;
            }
        }
        
        if (m_msaaImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device->device(), m_msaaImageView, nullptr);
            m_msaaImageView = VK_NULL_HANDLE;
        }
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device->device(), m_imageView, nullptr);
            m_imageView = VK_NULL_HANDLE;
        }
        if (m_msaaImage != VK_NULL_HANDLE) {
            vkDestroyImage(m_device->device(), m_msaaImage, nullptr);
            m_msaaImage = VK_NULL_HANDLE;
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device->device(), m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }
        if (m_msaaImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->device(), m_msaaImageMemory, nullptr);
            m_msaaImageMemory = VK_NULL_HANDLE;
        }
        if (m_imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device->device(), m_imageMemory, nullptr);
            m_imageMemory = VK_NULL_HANDLE;
        }

        m_extent = extent;

        // Recreate resources
        createImage();           // resolve image (1x)
        createMsaaImage();       // msaa image (4x)
        createImageView();
        createMsaaImageView();
        
        // Recreate picking attachments if they exist
        if (m_hasPickingAttachments) {
            createPickingImage();
            createMsaaPickingImage();
            createPickingImageView();
            createMsaaPickingImageView();
        }
        // Sampler and descriptor set remain valid; update descriptor to new imageView
        // Update descriptor set image info
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_imageView;
        imageInfo.sampler = m_sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_device->device(), 1, &descriptorWrite, 0, nullptr);

        createFramebuffer(renderPass);
    }

    void VulkanImageView::createPickingImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_pickingFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device->device(), &imageInfo, nullptr, &m_pickingImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create picking image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->device(), m_pickingImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_pickingImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate picking image memory!");
        }

        vkBindImageMemory(m_device->device(), m_pickingImage, m_pickingImageMemory, 0);
    }

    void VulkanImageView::createMsaaPickingImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_extent.width;
        imageInfo.extent.height = m_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = m_pickingFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_4_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(m_device->device(), &imageInfo, nullptr, &m_msaaPickingImage) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA picking image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_device->device(), m_msaaPickingImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_device->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device->device(), &allocInfo, nullptr, &m_msaaPickingImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate MSAA picking image memory!");
        }

        vkBindImageMemory(m_device->device(), m_msaaPickingImage, m_msaaPickingImageMemory, 0);
    }

    void VulkanImageView::createPickingImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_pickingImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_pickingFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_pickingImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create picking image view!");
        }
    }

    void VulkanImageView::createMsaaPickingImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_msaaPickingImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_pickingFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device->device(), &viewInfo, nullptr, &m_msaaPickingImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA picking image view!");
        }
    }
} // namespace Bess::Renderer2D::Vulkan
