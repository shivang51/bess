#pragma once

#include "device.h"
#include "pipelines/circle_pipeline.h"
#include "pipelines/grid_pipeline.h"
#include "pipelines/quad_pipeline.h"
#include "pipelines/text_pipeline.h"
#include "primitive_vertex.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include "scene/renderer/vulkan/vulkan_texture.h"
#include "vulkan_offscreen_render_pass.h"
#include <memory>
#include <unordered_map>
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
        void setCurrentFrameIndex(uint32_t frameIndex);

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

        void drawTexturedQuad(const glm::vec3 &pos,
                              const glm::vec2 &size,
                              const glm::vec4 &tint,
                              int id,
                              const glm::vec4 &borderRadius,
                              const glm::vec4 &borderSize,
                              const glm::vec4 &borderColor,
                              int isMica,
                              const std::shared_ptr<VulkanTexture> &texture);

        void drawTexturedQuad(const glm::vec3 &pos,
                              const glm::vec2 &size,
                              const glm::vec4 &tint,
                              int id,
                              const glm::vec4 &borderRadius,
                              const glm::vec4 &borderSize,
                              const glm::vec4 &borderColor,
                              int isMica,
                              const std::shared_ptr<SubTexture> &subTexture);

        void drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius = 0.0F);
        void drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id);
        void drawText(const std::vector<InstanceVertex> &opaque, const std::vector<InstanceVertex> &translucent);

        // Buffer management
        void updateUniformBuffer(const GridUniforms &gridUniforms);
        void updateUBO(const UniformBufferObject &ubo);
        void updateTextUniforms(const TextUniforms &textUniforms);

      private:
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        // Pipeline instances
        std::unique_ptr<Pipelines::CirclePipeline> m_circlePipeline;
        std::unique_ptr<Pipelines::GridPipeline> m_gridPipeline;
        std::unique_ptr<Pipelines::QuadPipeline> m_quadPipeline;
        std::unique_ptr<Pipelines::TextPipeline> m_textPipeline;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        std::vector<CircleInstance> m_opaqueCircleInstances;
        std::vector<CircleInstance> m_translucentCircleInstances;
        std::vector<InstanceVertex> m_opaqueTextInstances;
        std::vector<InstanceVertex> m_translucentTextInstances;
        std::vector<QuadInstance> m_opaqueQuadInstances;
        std::vector<QuadInstance> m_translucentQuadInstances;
        std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> m_texturedQuadInstances;

        std::vector<VkDescriptorSet> m_textureArraySets;

        GridVertex m_gridVertex;
    };

} // namespace Bess::Renderer2D::Vulkan
