#pragma once

#include "pipeline.h"
#include <vector>

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
        void cleanup() override;

        void drawQuad(const glm::vec3 &pos,
                      const glm::vec2 &size,
                      const glm::vec4 &color,
                      int id,
                      const glm::vec4 &borderRadius,
                      const glm::vec4 &borderSize,
                      const glm::vec4 &borderColor,
                      int isMica);

    private:
        void createQuadPipeline();
        void ensureQuadBuffers();
        void ensureQuadInstanceCapacity(size_t instanceCount);

        BufferSet m_buffers;
        std::vector<QuadInstance> m_pendingQuadInstances;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines

