#pragma once

#include "camera.h"
#include "scene/renderer/vulkan/vulkan_core.h"
#include "scene/renderer/vulkan/vulkan_subtexture.h"
#include <glm.hpp>
#include <memory>

namespace Bess::Renderer2D {

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

    class VulkanRenderer {
      public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        static void init();
        static void shutdown();

        static void beginScene(const std::shared_ptr<Camera> &camera);
        static void end();

        static void clearColor();
        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors);
        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, QuadRenderProperties properties = {});
        static void texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                 const std::shared_ptr<Vulkan::VulkanTexture> &texture,
                                 const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        static void texturedQuad(const glm::vec3 &pos, const glm::vec2 &size,
                                 const std::shared_ptr<Vulkan::SubTexture> &texture,
                                 const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        static void circle(const glm::vec3 &center, float radius,
                          const glm::vec4 &color, int id, float innerRadius = 0.0f);

      private:
        static std::shared_ptr<Camera> m_camera;
    };

} // namespace Bess::Renderer2D
