#pragma once

#include "pipeline.h"
#include <vector>

namespace Bess::Renderer2D::Vulkan::Pipelines {

    class GridPipeline : public Pipeline {
    public:
        GridPipeline(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~GridPipeline() override;

        GridPipeline(const GridPipeline &) = delete;
        GridPipeline &operator=(const GridPipeline &) = delete;
        GridPipeline(GridPipeline &&other) noexcept;
        GridPipeline &operator=(GridPipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;
        void cleanup() override;

        void drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms);
        void updateGridUniforms(const GridUniforms &gridUniforms);

    private:
        void createGraphicsPipeline();
        void createGridUniformBuffers();
        void createVertexBuffer();
        void createIndexBuffer();
        void updateVertexBuffer(const std::vector<GridVertex> &vertices);
        void updateIndexBuffer(const std::vector<uint32_t> &indices);

        BufferSet m_buffers;
        std::vector<GridVertex> m_currentVertices;
        std::vector<uint32_t> m_currentIndices;
        uint32_t m_currentVertexCount = 0;
        uint32_t m_currentIndexCount = 0;

        // Grid-specific uniform buffer
        std::vector<VkBuffer> m_gridUniformBuffers;
        std::vector<VkDeviceMemory> m_gridUniformBufferMemory;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines
