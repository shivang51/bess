#pragma once

#include "device.h"
#include "swapchain.h"

#include <string>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class VulkanRenderPass;

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

        void createGraphicsPipeline(std::shared_ptr<VulkanRenderPass> renderPass);

        VkPipeline graphicsPipeline() const { return m_graphicsPipeline; }
        VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
        VkDescriptorSetLayout descriptorSetLayout() const { return m_descriptorSetLayout; }
        std::shared_ptr<VulkanRenderPass> renderPass() const { return m_renderPass; }

      private:
        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;

        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanSwapchain> m_swapchain;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;

        std::shared_ptr<VulkanRenderPass> m_renderPass;
    };

} // namespace Bess::Renderer2D::Vulkan
