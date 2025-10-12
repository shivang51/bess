#pragma once

#include "camera.h"
#include "device.h"
#include "primitive_vertex.h"
#include "scene/renderer/material.h"
#include "scene/renderer/vulkan/pipelines/circle_pipeline.h"
#include "scene/renderer/vulkan/pipelines/grid_pipeline.h"
#include "scene/renderer/vulkan/pipelines/quad_pipeline.h"
#include "scene/renderer/vulkan/primitive_renderer.h"
#include "scene/renderer/vulkan/text_renderer.h"
#include "vulkan_offscreen_render_pass.h"
#include "vulkan_texture.h"
#include <memory>
#include <queue>
#include <vector>
#include <vulkan/vulkan.h>

using namespace Bess::Vulkan;
using namespace Bess::Renderer2D::Vulkan;
namespace Bess::Renderer {
    class MaterialRenderer {
      public:
        MaterialRenderer(const std::shared_ptr<VulkanDevice> &device,
                         const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                         VkExtent2D extent);
        ~MaterialRenderer();

        void beginFrame(VkCommandBuffer commandBuffer);
        void endFrame();
        void setCurrentFrameIndex(uint32_t frameIndex);

        void drawMaterial(const Material2D &material);

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
        void drawText(const std::string &text, const glm::vec3 &pos, const size_t size,
                      const glm::vec4 &color, const int id, float angle = 0);

        void resize(VkExtent2D extent);
        void updateUBO(const UniformBufferObject &ubo);

        glm::vec2 getMSDFTextRenderSize(const std::string &str, float renderSize);

      private:
        void flushVertices(bool isTranslucent);

      private:
        std::unique_ptr<Pipelines::CirclePipeline> m_circlePipeline;
        std::unique_ptr<Pipelines::GridPipeline> m_gridPipeline;
        std::unique_ptr<Pipelines::QuadPipeline> m_quadPipeline;
        std::unique_ptr<Renderer::TextRenderer> m_textRenderer;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        struct MaterialComp {
            bool operator()(const Material2D &l, const Material2D &r) const { return l.z < r.z; }
        };

        std::priority_queue<Material2D, std::vector<Material2D>, MaterialComp> m_translucentMaterials;

        std::vector<QuadInstance> m_quadInstances;
        std::vector<CircleInstance> m_circleInstances;

        VkCommandBuffer m_cmdBuffer;
    };
} // namespace Bess::Renderer
