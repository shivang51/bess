#pragma once

#include "pipeline.h"
#include "primitive_vertex.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Bess::Vulkan::Pipelines {

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

        void beginPipeline(VkCommandBuffer commandBuffer, bool isTranslucent) override;
        void endPipeline() override;

        void setCirclesData(const std::vector<CircleInstance> &opaque);

        void cleanup() override;

      private:
        void createGraphicsPipeline(bool isTranslucent) override;
        void createCircleBuffers();
        void ensureCircleInstanceCapacity(size_t instanceCount);

        void createDescriptorPool() override;
        void createDescriptorSets() override;

        BufferSet m_buffers;
        std::vector<CircleInstance> m_instances;
    };

} // namespace Bess::Vulkan::Pipelines
