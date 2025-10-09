#pragma once

#include "pipeline.h"
#include "primitive_vertex.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan {
    class VulkanTexture;
}

namespace Bess::Vulkan::Pipelines {

    class TextPipeline : public Pipeline {
      public:
        TextPipeline(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~TextPipeline() override;

        TextPipeline(const TextPipeline &) = delete;
        TextPipeline &operator=(const TextPipeline &) = delete;
        TextPipeline(TextPipeline &&other) noexcept;
        TextPipeline &operator=(TextPipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;

        void setTextData(const std::vector<InstanceVertex> &instances);

        void updateTextUniforms(const TextUniforms &textUniforms);

        void cleanup() override;

      private:
        void createGraphicsPipeline() override;
        void createTextBuffers();
        void ensureTextInstanceCapacity(size_t instanceCount);
        void createTextUniformBuffers();

        void createDescriptorSetLayout() override;
        void createDescriptorPool() override;
        void createDescriptorSets() override;

        static constexpr size_t m_texArraySize = 32;

        BufferSet m_buffers;
        std::vector<InstanceVertex> m_textInstances;
        std::vector<VkDescriptorSet> m_textureArraySets;
        VkDescriptorPool m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureArrayLayout = VK_NULL_HANDLE;
        std::unique_ptr<VulkanTexture> m_fallbackTexture;
        std::array<VkDescriptorImageInfo, 32> m_textureInfos;

        // Text uniforms
        std::vector<VkBuffer> m_textUniformBuffers;
        std::vector<VkDeviceMemory> m_textUniformBufferMemory;
    };

} // namespace Bess::Vulkan::Pipelines
