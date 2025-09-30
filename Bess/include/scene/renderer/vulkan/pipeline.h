#pragma once

#include "device.h"
#include "swapchain.h"
#include "vulkan_render_pass.h"

#include <string>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanPipeline {
      public:
        VulkanPipeline(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSwapchain> swapchain);
        ~VulkanPipeline();

        VulkanPipeline(const VulkanPipeline &) = delete;
        VulkanPipeline &operator=(const VulkanPipeline &) = delete;

        VulkanPipeline(VulkanPipeline &&other) noexcept;
        VulkanPipeline &operator=(VulkanPipeline &&other) noexcept;

        void createRenderPass();
        void createGraphicsPipeline(const std::string &vertShaderPath, const std::string &fragShaderPath, std::shared_ptr<VulkanRenderPass> renderPass);

        VkPipeline graphicsPipeline() const { return m_graphicsPipeline; }
        VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }

      private:
        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;

        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanSwapchain> m_swapchain;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
