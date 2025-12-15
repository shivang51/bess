#pragma once

#include "device.h"
#include "primitive_vertex.h"
#include "scene/camera.h"
#include "scene/renderer/material.h"
#include "scene/renderer/vulkan/pipelines/circle_pipeline.h"
#include "scene/renderer/vulkan/pipelines/grid_pipeline.h"
#include "scene/renderer/vulkan/pipelines/quad_pipeline.h"
#include "scene/renderer/vulkan/text_renderer.h"
#include "vulkan_offscreen_render_pass.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"
#include <cstdint>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

using namespace Bess::Vulkan;
using namespace Bess::Renderer2D::Vulkan;
namespace Bess::Renderer {

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
                      uint64_t id,
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

        glm::vec2 drawTextWrapped(const std::string &text, const glm::vec3 &pos, const size_t size,
                                  const glm::vec4 &color, const int id, float wrapWidthPx, float angle = 0);

        void resize(VkExtent2D extent);
        void updateUBO(const std::shared_ptr<Camera> &camera);

        glm::vec2 getTextRenderSize(const std::string &str, float renderSize);

      private:
        void flushVertices(bool isTranslucent);

      private:
        std::unique_ptr<Pipelines::CirclePipeline> m_circlePipeline;
        std::unique_ptr<Pipelines::GridPipeline> m_gridPipeline;
        std::unique_ptr<Pipelines::QuadPipeline> m_quadPipeline;
        std::unique_ptr<Renderer::TextRenderer> m_textRenderer;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        struct MaterialComp {
            bool operator()(const Material2D &l, const Material2D &r) const { return l.z > r.z; }
        };

        std::priority_queue<Material2D, std::vector<Material2D>, MaterialComp> m_translucentMaterials;

        std::vector<QuadInstance> m_quadInstances;
        std::unordered_map<std::shared_ptr<VulkanTexture>, std::vector<QuadInstance>> m_texturedQuadInstances;
        std::vector<CircleInstance> m_circleInstances;

        Material2D m_gridMaterial = {};
        std::shared_ptr<VulkanTexture> m_shadowTexture = nullptr;

        VkCommandBuffer m_cmdBuffer;
    };
} // namespace Bess::Renderer
