#pragma once

#include "pipeline.h"
#include "primitive_vertex.h"
#include <memory>
#include <vector>

namespace Bess::Vulkan::Pipelines {

    class PathStrokePipeline : public Pipeline {
      public:
        PathStrokePipeline(const std::shared_ptr<VulkanDevice> &device,
                           const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                           VkExtent2D extent);
        ~PathStrokePipeline();

        PathStrokePipeline(const PathStrokePipeline &) = delete;
        PathStrokePipeline &operator=(const PathStrokePipeline &) = delete;
        PathStrokePipeline(PathStrokePipeline &&other) noexcept;
        PathStrokePipeline &operator=(PathStrokePipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;

        void setPathData(const std::vector<CommonVertex> &strokeVertices, const std::vector<uint32_t> &strokeIndices);

        void updateUniformBuffer(const UniformBufferObject &ubo);

        void cleanup() override;

      private:
        void createGraphicsPipeline() override;
        void createVertexBuffer();
        void createIndexBuffer();
        void createDescriptorSetLayout() override;
        void createDescriptorPool() override;
        void createDescriptorSets() override;

        void ensurePathBuffers();
        void ensurePathCapacity(size_t vertexCount, size_t indexCount);

        // Path-specific data
        std::vector<CommonVertex> m_strokeVertices;
        std::vector<uint32_t> m_strokeIndices;

        // Buffers for path data
        std::vector<VkBuffer> m_vertexBuffers;
        std::vector<VkDeviceMemory> m_vertexBufferMemory;
        std::vector<VkBuffer> m_indexBuffers;
        std::vector<VkDeviceMemory> m_indexBufferMemory;

        // Current buffer sizes
        size_t m_currentVertexCapacity = 0;
        size_t m_currentIndexCapacity = 0;

        // Uniform buffer for zoom
        std::vector<VkBuffer> m_zoomUniformBuffers;
        std::vector<VkDeviceMemory> m_zoomUniformBufferMemory;

        // Shader stages
        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

        void createZoomUniformBuffers();
        void updateZoomUniformBuffer(float zoom);
    };

} // namespace Bess::Vulkan::Pipelines
