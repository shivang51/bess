#pragma once

#include "device.h"
#include "pipelines/path_pipeline.h"
#include "primitive_vertex.h"
#include "vulkan_offscreen_render_pass.h"
#include <memory>
#include <vector>

namespace Bess::Renderer2D::Vulkan {

    // Path-related structures from the old GL renderer
    struct PathPoint {
        glm::vec3 pos;
        float weight = 1.f;
        uint64_t id = 0.f;
    };

    struct PathContext {
        bool ended = true;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec3 currentPos;
        std::vector<PathPoint> points;

        void setCurrentPos(const glm::vec3 &pos) {
            currentPos = pos;
        }
    };

    struct QuadBezierCurvePoints {
        glm::vec2 startPoint;
        glm::vec2 controlPoint;
        glm::vec2 endPoint;
    };

    class PathRenderer {
      public:
        PathRenderer(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~PathRenderer();

        PathRenderer(const PathRenderer &) = delete;
        PathRenderer &operator=(const PathRenderer &) = delete;
        PathRenderer(PathRenderer &&other) noexcept;
        PathRenderer &operator=(PathRenderer &&other) noexcept;

        void beginFrame(VkCommandBuffer commandBuffer);
        void endFrame();
        void setCurrentFrameIndex(uint32_t frameIndex);

        // Path API functions
        void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id);
        void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = glm::vec4(1.f), bool genStroke = true);
        void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id);
        void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                               float weight, const glm::vec4 &color, int id);
        void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id);

        void updateUniformBuffer(const UniformBufferObject &ubo);

      private:
        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        // Pipeline instance
        std::unique_ptr<Pipelines::PathPipeline> m_pathPipeline;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        // Path context
        PathContext m_pathData;

        // Path generation functions (from old GL renderer)
        std::vector<CommonVertex> generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                         const glm::vec4 &color, bool isClosed);
        std::vector<CommonVertex> generateFillGeometry(const std::vector<PathPoint> &points, const glm::vec4 &color);
        QuadBezierCurvePoints generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);
        int calculateQuadBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2);
        int calculateCubicBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3);
        glm::vec2 nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                                                const glm::vec2 &p2, const glm::vec2 &p3, float t);
        glm::vec2 nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, float t);
        std::vector<glm::vec3> generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end);
        std::vector<glm::vec3> generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end);

        // Helper functions
        float cross_product(const glm::vec2 &O, const glm::vec2 &A, const glm::vec2 &B);
    };

} // namespace Bess::Renderer2D::Vulkan
