#pragma once

#include "pipeline.h"
#include "primitive_vertex.h"
#include <memory>
#include <vector>

namespace Bess::Vulkan::Pipelines {

    class PathFillPipeline : public Pipeline {
      public:
        PathFillPipeline(const std::shared_ptr<VulkanDevice> &device,
                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                         VkExtent2D extent);
        ~PathFillPipeline();

        PathFillPipeline(const PathFillPipeline &) = delete;
        PathFillPipeline &operator=(const PathFillPipeline &) = delete;
        PathFillPipeline(PathFillPipeline &&other) noexcept;
        PathFillPipeline &operator=(PathFillPipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;

        void setPathData(const std::vector<CommonVertex> &fillVertices,
                         const std::vector<uint32_t> &fillIndices);
        
        void setInstancedPathData(const std::vector<CommonVertex> &fillVertices,
                                 const std::vector<uint32_t> &fillIndices,
                                 const std::vector<PathInstance> &instances);
        
        void setPathDataWithTranslation(const std::vector<CommonVertex> &fillVertices,
                                       const std::vector<uint32_t> &fillIndices,
                                       const glm::vec3 &translation);

        // Batched draw calls using push constants per draw
        struct FillDrawCall {
            uint32_t firstIndex;
            uint32_t indexCount;
            uint32_t firstInstance;
            uint32_t instanceCount;
        };

        void setBatchedPathData(const std::vector<CommonVertex> &fillVertices,
                                const std::vector<uint32_t> &fillIndices,
                                const std::vector<FillDrawCall> &drawCalls);

        void setInstanceData(const std::vector<FillInstance> &instances);

        void updateUniformBuffer(const UniformBufferObject &ubo);

        void cleanup() override;

      private:
        void createGraphicsPipeline() override;
        void createVertexBuffer();
        void createIndexBuffer();
        void createInstanceBuffer();
        void createDescriptorSetLayout() override;
        void createDescriptorPool() override;
        void createDescriptorSets() override;

        void ensurePathBuffers();
        void ensurePathCapacity(size_t vertexCount, size_t indexCount);
        void ensureInstanceCapacity(size_t instanceCount);

        // Path-specific data
        std::vector<CommonVertex> m_fillVertices;
        std::vector<uint32_t> m_fillIndices;
        std::vector<PathInstance> m_instances;
        glm::vec3 m_translation = glm::vec3(0.0f);
        std::vector<FillDrawCall> m_drawCalls;
        std::vector<FillInstance> m_instancesCpu;

        // Buffers for path data
        std::vector<VkBuffer> m_vertexBuffers;
        std::vector<VkDeviceMemory> m_vertexBufferMemory;
        std::vector<VkBuffer> m_indexBuffers;
        std::vector<VkDeviceMemory> m_indexBufferMemory;
        std::vector<VkBuffer> m_instanceBuffers;
        std::vector<VkDeviceMemory> m_instanceBufferMemory;

        // Current buffer sizes
        size_t m_currentVertexCapacity = 0;
        size_t m_currentIndexCapacity = 0;
        size_t m_currentInstanceCapacity = 0;

        // Uniform buffer for zoom
        std::vector<VkBuffer> m_zoomUniformBuffers;
        std::vector<VkDeviceMemory> m_zoomUniformBufferMemory;

        // Shader stages
        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

        void createZoomUniformBuffers();
        void updateZoomUniformBuffer(float zoom);
    };

} // namespace Bess::Vulkan::Pipelines
