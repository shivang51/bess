#pragma once

#include "device.h"
#include "swapchain.h"
#include <string>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class ImGuiPipeline {
      public:
        ImGuiPipeline(std::shared_ptr<VulkanDevice> device, std::shared_ptr<VulkanSwapchain> swapchain);
        ~ImGuiPipeline();

        // Delete copy constructor and assignment operator
        ImGuiPipeline(const ImGuiPipeline &) = delete;
        ImGuiPipeline &operator=(const ImGuiPipeline &) = delete;

        // Move constructor and assignment operator
        ImGuiPipeline(ImGuiPipeline &&other) noexcept;
        ImGuiPipeline &operator=(ImGuiPipeline &&other) noexcept;

        void createRenderPass();
        void createGraphicsPipeline();

        VkRenderPass renderPass() const { return m_renderPass; }
        VkPipeline graphicsPipeline() const { return m_graphicsPipeline; }
        VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
        VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }

      private:
        VkShaderModule createShaderModule(const std::vector<char> &code);
        std::vector<char> readFile(const std::string &filename);

        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanSwapchain> m_swapchain;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan
