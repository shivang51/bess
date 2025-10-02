#pragma once

#include "pipeline.h"
#include "scene/renderer/vulkan/primitive_vertex.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D::Vulkan {
    class VulkanTexture;
}

namespace Bess::Renderer2D::Vulkan::Pipelines {

    class QuadPipeline : public Pipeline {
      public:
        QuadPipeline(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~QuadPipeline() override;

        QuadPipeline(const QuadPipeline &) = delete;
        QuadPipeline &operator=(const QuadPipeline &) = delete;
        QuadPipeline(QuadPipeline &&other) noexcept;
        QuadPipeline &operator=(QuadPipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;

        void setQuadsData(
            const std::vector<QuadInstance> &data,
            std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> &texutredData);

        void cleanup() override;

      private:
        void createGraphicsPipeline();
        void ensureQuadBuffers();
        void ensureQuadInstanceCapacity(size_t instanceCount);

        void createDescriptorPool() override;
        void createDescriptorSets() override;

        static constexpr size_t m_texArraySize = 32;

        BufferSet m_buffers;
        std::vector<QuadInstance> m_pendingQuadInstances;
        std::vector<VkDescriptorSet> m_textureArraySets;
        VkDescriptorPool m_textureArrayDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureArrayLayout = VK_NULL_HANDLE;
        std::shared_ptr<VulkanTexture> m_fallbackTexture;
        std::array<VkDescriptorImageInfo, 32> m_textureInfos;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines
