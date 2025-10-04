#pragma once

#include "../device.h"
#include "../primitive_vertex.h"
#include "../vulkan_offscreen_render_pass.h"
#include <memory>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan::Pipelines {

    struct BufferSet {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        VkBuffer instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
        size_t instanceCapacity = 0;
    };

    class Pipeline {
      public:
        Pipeline(const std::shared_ptr<VulkanDevice> &device,
                 const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                 VkExtent2D extent);
        virtual ~Pipeline() = default;

        Pipeline(const Pipeline &) = delete;
        Pipeline &operator=(const Pipeline &) = delete;
        Pipeline(Pipeline &&other) noexcept;
        Pipeline &operator=(Pipeline &&other) noexcept;

        // Pure virtual functions that each pipeline must implement
        virtual void beginPipeline(VkCommandBuffer commandBuffer) = 0;
        virtual void endPipeline() = 0;
        virtual void cleanup() = 0;

        // Common functions
        void updateUniformBuffer(const UniformBufferObject &ubo);
        void setCurrentFrameIndex(uint32_t frameIndex) { m_currentFrameIndex = frameIndex; }
        VkPipeline getPipeline() const { return m_pipeline; }
        VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

      protected:
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_descriptorSets;
        uint32_t m_currentFrameIndex = 0;

        // Uniform buffers
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBufferMemory;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        // Helper functions
        VkShaderModule createShaderModule(const std::vector<char> &code) const;
        std::vector<char> readFile(const std::string &filename) const;
        virtual void createDescriptorSetLayout();
        virtual void createDescriptorPool();
        virtual void createDescriptorSets();
        void createUniformBuffers();

        // Common graphics pipeline creation helpers
        VkPipelineInputAssemblyStateCreateInfo createInputAssemblyState() const;
        VkPipelineViewportStateCreateInfo createViewportState() const;
        VkPipelineRasterizationStateCreateInfo createRasterizationState() const;
        VkPipelineMultisampleStateCreateInfo createMultisampleState() const;
        VkPipelineDepthStencilStateCreateInfo createDepthStencilState() const;
        VkPipelineColorBlendStateCreateInfo createColorBlendState(const std::vector<VkPipelineColorBlendAttachmentState> &colorBlendAttachments) const;
        VkPipelineDynamicStateCreateInfo createDynamicState() const;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines
