#pragma once

#include "device.h"
#include "swapchain.h"
#include <vulkan/vulkan.h>
#include <string>

namespace Bess::Renderer2D::Vulkan {

    class VulkanPipeline {
    public:
        VulkanPipeline(VulkanDevice& device, VulkanSwapchain& swapchain);
        ~VulkanPipeline();

        // Delete copy constructor and assignment operator
        VulkanPipeline(const VulkanPipeline&) = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;

        // Move constructor and assignment operator
        VulkanPipeline(VulkanPipeline&& other) noexcept;
        VulkanPipeline& operator=(VulkanPipeline&& other) noexcept;

        void createRenderPass();
        void createGraphicsPipeline(const std::string& vertShaderPath, const std::string& fragShaderPath);

        VkRenderPass renderPass() const { return m_renderPass; }
        VkPipeline graphicsPipeline() const { return m_graphicsPipeline; }

    private:
        VkShaderModule createShaderModule(const std::vector<char>& code);
        std::vector<char> readFile(const std::string& filename);

        VulkanDevice& m_device;
        VulkanSwapchain& m_swapchain;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
