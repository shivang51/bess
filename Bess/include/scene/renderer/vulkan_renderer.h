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

        static void msdfText(const std::string &text, const glm::vec3 &pos, const size_t size,
                            const glm::vec4 &color, const int id, float angle = 0.0f);

        // Path API
        static void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id);
        static void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = glm::vec4(1.f), bool genStroke = true);
        static void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id);
        static void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                      float weight, const glm::vec4 &color, int id);
        static void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id);

      private:
        static std::shared_ptr<Camera> m_camera;
    };

} // namespace Bess::Renderer2D
