#include "scene/renderer/vulkan/swapchain.h"
#include "common/log.h"
#include <algorithm>
#include <stdexcept>

namespace Bess::Renderer2D::Vulkan {

    VulkanSwapchain::VulkanSwapchain(VkInstance instance, std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, VkExtent2D windowExtent)
        : m_instance(instance), m_device(device), m_surface(surface), m_windowExtent(windowExtent) {
        createSwapchain();
        createImageViews();
    }

    VulkanSwapchain::VulkanSwapchain(VkInstance instance, std::shared_ptr<VulkanDevice> device, VkSurfaceKHR surface, VkExtent2D windowExtent, VkSwapchainKHR oldSwapchain)
        : m_instance(instance), m_device(device), m_surface(surface), m_windowExtent(windowExtent) {
        createSwapchain(oldSwapchain);
        createImageViews();
    }

    VulkanSwapchain::~VulkanSwapchain() {
        cleanup();
    }

    VulkanSwapchain::VulkanSwapchain(VulkanSwapchain &&other) noexcept
        : m_instance(other.m_instance),
          m_device(other.m_device),
          m_surface(other.m_surface),
          m_windowExtent(other.m_windowExtent),
          m_swapchain(other.m_swapchain),
          m_images(std::move(other.m_images)),
          m_framebuffers(std::move(other.m_framebuffers)),
          m_imageFormat(other.m_imageFormat),
          m_extent(other.m_extent),
          m_imageViews(std::move(other.m_imageViews)) {
        other.m_swapchain = VK_NULL_HANDLE;
    }

    VulkanSwapchain &VulkanSwapchain::operator=(VulkanSwapchain &&other) noexcept {
        if (this != &other) {
            cleanup();

            m_instance = other.m_instance;
            // m_device is a reference and cannot be reassigned
            m_surface = other.m_surface;
            m_windowExtent = other.m_windowExtent;
            m_swapchain = other.m_swapchain;
            m_images = std::move(other.m_images);
            m_framebuffers = std::move(other.m_framebuffers);
            m_imageFormat = other.m_imageFormat;
            m_extent = other.m_extent;
            m_imageViews = std::move(other.m_imageViews);

            other.m_swapchain = VK_NULL_HANDLE;
        }
        return *this;
    }

    void VulkanSwapchain::createSwapchain(VkSwapchainKHR oldSwapchain) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_device->physicalDevice());

        BESS_INFO("Available surface formats: {}", swapChainSupport.formats.size());
        for (const auto& format : swapChainSupport.formats) {
            BESS_INFO("  Format: {}, ColorSpace: {}", static_cast<int>(format.format), static_cast<int>(format.colorSpace));
        }

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, m_windowExtent);
        
        BESS_INFO("Selected surface format: {}, ColorSpace: {}", static_cast<int>(surfaceFormat.format), static_cast<int>(surfaceFormat.colorSpace));
        BESS_INFO("Selected present mode: {}", static_cast<int>(presentMode));
        BESS_INFO("Selected extent: {}x{}", extent.width, extent.height);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = m_device->queueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        BESS_INFO("Swapchain creation parameters:");
        BESS_INFO("  Image count: {}", imageCount);
        BESS_INFO("  Image format: {}", static_cast<int>(createInfo.imageFormat));
        BESS_INFO("  Image color space: {}", static_cast<int>(createInfo.imageColorSpace));
        BESS_INFO("  Image extent: {}x{}", createInfo.imageExtent.width, createInfo.imageExtent.height);
        BESS_INFO("  Image usage: {}", static_cast<int>(createInfo.imageUsage));
        BESS_INFO("  Sharing mode: {}", static_cast<int>(createInfo.imageSharingMode));
        BESS_INFO("  Present mode: {}", static_cast<int>(createInfo.presentMode));

        VkResult result = vkCreateSwapchainKHR(m_device->device(), &createInfo, nullptr, &m_swapchain);
        if (result != VK_SUCCESS) {
            BESS_ERROR("Failed to create swap chain! Error code: {}", static_cast<int>(result));
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imageCount, nullptr);
        m_images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imageCount, m_images.data());

        m_imageFormat = surfaceFormat.format;
        m_extent = extent;
    }

    void VulkanSwapchain::createSwapchain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_device->physicalDevice());

        BESS_INFO("Available surface formats: {}", swapChainSupport.formats.size());
        for (const auto& format : swapChainSupport.formats) {
            BESS_INFO("  Format: {}, ColorSpace: {}", static_cast<int>(format.format), static_cast<int>(format.colorSpace));
        }

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, m_windowExtent);
        
        BESS_INFO("Selected surface format: {}, ColorSpace: {}", static_cast<int>(surfaceFormat.format), static_cast<int>(surfaceFormat.colorSpace));
        BESS_INFO("Selected present mode: {}", static_cast<int>(presentMode));
        BESS_INFO("Selected extent: {}x{}", extent.width, extent.height);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = m_device->queueFamilyIndices();
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        BESS_INFO("Swapchain creation parameters:");
        BESS_INFO("  Image count: {}", imageCount);
        BESS_INFO("  Image format: {}", static_cast<int>(createInfo.imageFormat));
        BESS_INFO("  Image color space: {}", static_cast<int>(createInfo.imageColorSpace));
        BESS_INFO("  Image extent: {}x{}", createInfo.imageExtent.width, createInfo.imageExtent.height);
        BESS_INFO("  Image usage: {}", static_cast<int>(createInfo.imageUsage));
        BESS_INFO("  Sharing mode: {}", static_cast<int>(createInfo.imageSharingMode));
        BESS_INFO("  Present mode: {}", static_cast<int>(createInfo.presentMode));

        VkResult result = vkCreateSwapchainKHR(m_device->device(), &createInfo, nullptr, &m_swapchain);
        if (result != VK_SUCCESS) {
            BESS_ERROR("Failed to create swap chain! Error code: {}", static_cast<int>(result));
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imageCount, nullptr);
        m_images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imageCount, m_images.data());

        m_imageFormat = surfaceFormat.format;
        m_extent = extent;
    }

    void VulkanSwapchain::createImageViews() {
        m_imageViews.resize(m_images.size());

        for (size_t i = 0; i < m_images.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_imageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device->device(), &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create texture image view!");
            }
        }
    }

    void VulkanSwapchain::createFramebuffers(VkRenderPass renderPass) {
        m_framebuffers.resize(m_imageViews.size());

        for (size_t i = 0; i < m_imageViews.size(); i++) {
            VkImageView attachments[] = {
                m_imageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_extent.width;
            framebufferInfo.height = m_extent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device->device(), &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void VulkanSwapchain::cleanup() {
        for (auto framebuffer : m_framebuffers) {
            vkDestroyFramebuffer(m_device->device(), framebuffer, nullptr);
        }
        m_framebuffers.clear();

        for (auto imageView : m_imageViews) {
            vkDestroyImageView(m_device->device(), imageView, nullptr);
        }
        m_imageViews.clear();

        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device->device(), m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
    }

    VulkanSwapchain::SwapChainSupportDetails VulkanSwapchain::querySwapChainSupport(VkPhysicalDevice device) {
        VulkanSwapchain::SwapChainSupportDetails details{};

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        // Prefer UNORM format with SRGB color space for proper gamma correction
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        
        // Fallback to any UNORM format with SRGB color space
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        
        // Fallback to any format with SRGB color space
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        // If no SRGB format available, use the first available format
        if (!availableFormats.empty()) {
            return availableFormats[0];
        }
        
        // This should never happen, but just in case
        VkSurfaceFormatKHR fallback{};
        fallback.format = VK_FORMAT_B8G8R8A8_UNORM;
        fallback.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return fallback;
    }

    VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, VkExtent2D windowExtent) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent = windowExtent;
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

} // namespace Bess::Renderer2D::Vulkan
