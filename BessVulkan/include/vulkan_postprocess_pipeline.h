#pragma once

#include "device.h"
#include <memory>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {

    class VulkanPostprocessPipeline {
      public:
        VulkanPostprocessPipeline(const std::shared_ptr<VulkanDevice> &device, VkFormat colorFormat);
        ~VulkanPostprocessPipeline();

        VulkanPostprocessPipeline(const VulkanPostprocessPipeline &) = delete;
        VulkanPostprocessPipeline &operator=(const VulkanPostprocessPipeline &) = delete;
        VulkanPostprocessPipeline(VulkanPostprocessPipeline &&other) noexcept;
        VulkanPostprocessPipeline &operator=(VulkanPostprocessPipeline &&other) noexcept;

        VkPipeline getPipeline() const { return m_pipeline; }
        VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
        VkRenderPass getRenderPass() const { return m_renderPass; }
        VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

        void createDescriptorSet(VkImageView inputImageView, VkSampler inputSampler);
        void updateDescriptorSet(VkImageView inputImageView, VkSampler inputSampler);
        VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }

      private:
        void createRenderPass();
        void createDescriptorSetLayout();
        void createPipeline();

        std::vector<char> readFile(const std::string &filename) const;
        VkShaderModule createShaderModule(const std::vector<char> &code) const;

        std::shared_ptr<VulkanDevice> m_device;
        VkFormat m_colorFormat;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;

        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    };

} // namespace Bess::Vulkan
