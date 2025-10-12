#pragma once

#include "camera.h"
#include "device.h"
#include "pipelines/circle_pipeline.h"
#include "pipelines/grid_pipeline.h"
#include "pipelines/quad_pipeline.h"
#include "pipelines/text_pipeline.h"
#include "primitive_vertex.h"
#include "scene/renderer/font.h"
#include "scene/renderer/vulkan/text_renderer.h"
#include "vulkan_offscreen_render_pass.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

using namespace Bess::Vulkan;
namespace Bess::Renderer2D::Vulkan {

    struct QuadRenderProperties {
        float angle = 0.0f;
        glm::vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderRadius = {0.0f, 0.0f, 0.0f, 0.0f};
        glm::vec4 borderSize = {0.0f, 0.0f, 0.0f, 0.0f};
        bool hasShadow = false;
        bool isMica = false;
    };

    struct GridColors {
        glm::vec4 minorColor;
        glm::vec4 majorColor;
        glm::vec4 axisXColor;
        glm::vec4 axisYColor;
    };

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

        void resize(VkExtent2D extent);

        // Primitive rendering functions
        void drawGrid(const glm::vec3 &pos, const glm::vec2 &size, int id,
                      const GridColors &gridColors, const std::shared_ptr<Camera> &camera);

        void drawQuad(const glm::vec3 &pos,
                      const glm::vec2 &size,
                      const glm::vec4 &color,
                      int id,
                      QuadRenderProperties props = {});

        void drawTexturedQuad(const glm::vec3 &pos,
                              const glm::vec2 &size,
                              const glm::vec4 &tint,
                              int id,
                              const std::shared_ptr<VulkanTexture> &texture,
                              QuadRenderProperties props = {});

        void drawTexturedQuad(const glm::vec3 &pos,
                              const glm::vec2 &size,
                              const glm::vec4 &tint,
                              int id,
                              const std::shared_ptr<SubTexture> &subTexture,
                              QuadRenderProperties props = {});

        void drawCircle(const glm::vec3 &center, float radius, const glm::vec4 &color, int id, float innerRadius = 0.0F);
        void drawLine(const glm::vec3 &start, const glm::vec3 &end, float width, const glm::vec4 &color, int id);
        void drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                      const glm::vec4 &color, const int id, float angle = 0);

        // Buffer management
        void updateUBO(const UniformBufferObject &ubo);
        void updateTextUniforms(const TextUniforms &textUniforms);

        glm::vec2 getMSDFTextRenderSize(const std::string &str, float renderSize);

      private:
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        std::unique_ptr<Renderer::TextRenderer> m_textRenderer;

        // Pipeline instances
        std::unique_ptr<Pipelines::CirclePipeline> m_circlePipeline;
        std::unique_ptr<Pipelines::GridPipeline> m_gridPipeline;
        std::unique_ptr<Pipelines::QuadPipeline> m_quadPipeline;
        std::unique_ptr<Pipelines::TextPipeline> m_textPipeline;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        std::vector<CircleInstance> m_opaqueCircleInstances;
        std::vector<CircleInstance> m_translucentCircleInstances;
        std::vector<InstanceVertex> m_textInstances;
        std::vector<QuadInstance> m_opaqueQuadInstances;
        std::vector<QuadInstance> m_translucentQuadInstances;
        std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> m_texturedQuadInstances;

        std::vector<VkDescriptorSet> m_textureArraySets;

        GridVertex m_gridVertex;
        UniformBufferObject m_ubo;
    };

} // namespace Bess::Renderer2D::Vulkan
