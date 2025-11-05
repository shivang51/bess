#pragma once

#include "device.h"
#include "primitive_vertex.h"
#include "scene/renderer/font.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "vulkan_offscreen_render_pass.h"

#include <memory>
namespace Bess::Renderer {

    class TextRenderer {
      public:
        TextRenderer(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);

        void resize(VkExtent2D size);

        void beginFrame(VkCommandBuffer commandBuffer);
        void endFrame();
        void setCurrentFrameIndex(uint32_t frameIndex);

        void updateUBO(const UniformBufferObject &ubo);

        void drawText(const std::string &text, const glm::vec3 &pos, size_t size,
                      const glm::vec4 &color, int id, float angle = 0);

        glm::vec2 drawTextWrapped(const std::string &text, const glm::vec3 &pos, size_t size,
                                  const glm::vec4 &color, int id, float wrapWidthPx, float angle = 0);

        glm::vec2 getRenderSize(const std::string &text, size_t size);

      private:
        std::unique_ptr<Renderer2D::Vulkan::PathRenderer> m_pathRenderer;
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        Font::FontFile m_font;
    };

} // namespace Bess::Renderer
