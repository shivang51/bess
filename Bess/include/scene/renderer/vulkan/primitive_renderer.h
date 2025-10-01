#pragma once

#include "device.h"
#include "primitive_vertex.h"
#include "vulkan_offscreen_render_pass.h"
#include "pipelines/grid_pipeline.h"
#include "pipelines/quad_pipeline.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Bess::Renderer2D::Vulkan {

    class PrimitiveRenderer {
      public:
        PrimitiveRenderer(const std::shared_ptr<VulkanDevice> &device,
                          const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                          VkExtent2D extent);
        ~PrimitiveRenderer();

        PrimitiveRenderer(const PrimitiveRenderer &) = delete;
        PrimitiveRenderer &operator=(const PrimitiveRenderer &) = delete;
        PrimitiveRenderer(PrimitiveRenderer &&other) noexcept;
        PrimitiveRenderer &operator=(PrimitiveRenderer &&other) noexcept;

        void beginFrame(VkCommandBuffer commandBuffer);
        void endFrame();

        // Primitive rendering functions
        void drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridUniforms &gridUniforms);
        void drawQuad(const glm::vec3 &pos,
                      const glm::vec2 &size,
                      const glm::vec4 &color,
                      int id,
                      const glm::vec4 &borderRadius,
                      const glm::vec4 &borderSize,
                      const glm::vec4 &borderColor,
                      int isMica);
        void drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius = 0.0F);
        void drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id);

        // Buffer management
        void updateUniformBuffer(const UniformBufferObject &ubo, const GridUniforms &gridUniforms);
        void updateMvp(const UniformBufferObject &ubo);

      private:
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        // Pipeline instances
        std::unique_ptr<Pipelines::GridPipeline> m_gridPipeline;
        std::unique_ptr<Pipelines::QuadPipeline> m_quadPipeline;

        // Current frame data
        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
    };

} // namespace Bess::Renderer2D::Vulkan