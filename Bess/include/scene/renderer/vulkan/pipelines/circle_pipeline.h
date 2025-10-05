#pragma once

#include "pipeline.h"
#include "scene/renderer/vulkan/primitive_vertex.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Renderer2D::Vulkan::Pipelines {

    class CirclePipeline : public Pipeline {
      public:
        CirclePipeline(const std::shared_ptr<VulkanDevice> &device,
                       const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                       VkExtent2D extent);
        ~CirclePipeline() override;

        CirclePipeline(const CirclePipeline &) = delete;
        CirclePipeline &operator=(const CirclePipeline &) = delete;
        CirclePipeline(CirclePipeline &&other) noexcept;
        CirclePipeline &operator=(CirclePipeline &&other) noexcept;

        void beginPipeline(VkCommandBuffer commandBuffer) override;
        void endPipeline() override;

        void setCirclesData(const std::vector<CircleInstance> &opaque,
                           const std::vector<CircleInstance> &translucent);

        void cleanup() override;

      private:
        void createGraphicsPipeline();
        void ensureCircleBuffers();
        void ensureCircleInstanceCapacity(size_t instanceCount);

        void createDescriptorPool() override;
        void createDescriptorSets() override;

        BufferSet m_buffers;
        std::vector<CircleInstance> m_opaqueInstances;
        std::vector<CircleInstance> m_translucentInstances;
    };

} // namespace Bess::Renderer2D::Vulkan::Pipelines
