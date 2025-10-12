#pragma once

#include "bess_uuid.h"
#include "device.h"
#include "pipelines/path_fill_pipeline.h"
#include "pipelines/path_stroke_pipeline.h"
#include "primitive_vertex.h"
#include "scene/renderer/path.h"
#include "vulkan_offscreen_render_pass.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

using namespace Bess::Vulkan;
namespace Bess::Renderer2D::Vulkan {
    struct PathContext {
        bool ended = true;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec3 currentPos;
        glm::vec3 startPos;
        std::vector<PathPoint> points;
        std::vector<std::vector<PathPoint>> contours;
        int64_t id = 0;

        void setCurrentPos(const glm::vec3 &pos) {
            currentPos = pos;
        }
    };

    struct QuadBezierCurvePoints {
        glm::vec2 startPoint;
        glm::vec2 controlPoint;
        glm::vec2 endPoint;
    };

    struct ContoursDrawInfo {
        bool genStroke = true;
        bool genFill = false;
        bool closePath = false;
        glm::vec3 translate{0.f};
        glm::vec2 scale = glm::vec2(1.0f);
        glm::vec4 fillColor = glm::vec4(1.f, 0.f, 0.f, 1.f);
        glm::vec4 strokeColor;
        int64_t glyphId = 0; // for instanced fill path
    };

    struct PathGeometryCacheEntry {
        UUID pathId;
        std::vector<std::vector<CommonVertex>> strokeVertices;
        std::vector<CommonVertex> fillVertices;
    };

    class PathGeometryCache {
      public:
        bool getEntry(UUID id, PathGeometryCacheEntry &entry) {
            if (!m_cache.contains(id))
                return false;
            entry = m_cache[id];
            return true;
        }

        bool getEntryPtr(UUID id, const PathGeometryCacheEntry *&entryPtr) const {
            auto it = m_cache.find(id);
            if (it == m_cache.end())
                return false;
            entryPtr = &it->second;
            return true;
        }

        void cacheEntry(const PathGeometryCacheEntry &entry) {
            m_cache[entry.pathId] = entry;
        }

        void clearCache() {
            m_cache.clear();
        }

      private:
        std::unordered_map<UUID, PathGeometryCacheEntry> m_cache;
    };

    class PathRenderer {
      public:
        PathRenderer(const std::shared_ptr<VulkanDevice> &device,
                     const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                     VkExtent2D extent);
        ~PathRenderer();

        PathRenderer(const PathRenderer &) = delete;
        PathRenderer &operator=(const PathRenderer &) = delete;

        void beginFrame(VkCommandBuffer commandBuffer);
        void endFrame();
        void setCurrentFrameIndex(uint32_t frameIndex);

        void drawContours(const std::vector<std::vector<PathPoint>> &contours, ContoursDrawInfo info = {});
        void drawPath(Renderer::Path &path, ContoursDrawInfo info = {});

        // Path API functions
        void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id);
        void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = glm::vec4(1.f), bool genStroke = true);
        void pathMoveTo(const glm::vec3 &pos);
        void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id);
        void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                               float weight, const glm::vec4 &color, int id);
        void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id);

        void updateUniformBuffer(const UniformBufferObject &ubo);

        void resize(VkExtent2D extent);

      public:
        static int calculateQuadBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2);
        static int calculateCubicBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3);
        static glm::vec2 nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                                                       const glm::vec2 &p2, const glm::vec2 &p3, float t);
        static glm::vec2 nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, float t);
        static std::vector<glm::vec3> generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end);
        static std::vector<glm::vec3> generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end);

      private:
        void addPathGeometries(const std::vector<std::vector<CommonVertex>> &strokeGeometry);

      private:
        PathGeometryCache m_cache;

        std::vector<CommonVertex> m_strokeVertices;
        std::vector<CommonVertex> m_fillVertices;
        std::vector<uint32_t> m_strokeIndices;
        std::vector<uint32_t> m_fillIndices;
        std::vector<PathInstance> m_fillInstances;
        glm::vec3 m_fillTranslation = glm::vec3(0.0f);
        std::vector<Pipelines::PathFillPipeline::FillDrawCall> m_fillDrawCalls;
        std::vector<FillInstance> m_tempInstances;
        struct MeshRange {
            uint32_t firstIndex;
            uint32_t indexCount;
        };
        std::unordered_map<UUID, MeshRange> m_glyphIdToMesh;
        std::unordered_map<UUID, std::vector<FillInstance>> m_glyphIdToInstances;

        std::shared_ptr<VulkanDevice> m_device;
        std::shared_ptr<VulkanOffscreenRenderPass> m_renderPass;
        VkExtent2D m_extent;

        // Pipeline instance
        std::unique_ptr<Pipelines::PathStrokePipeline> m_pathStrokePipeline;
        std::unique_ptr<Pipelines::PathFillPipeline> m_pathFillPipeline;

        VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;

        // Path context
        PathContext m_pathData;

        std::vector<CommonVertex> generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                         const glm::vec4 &color, bool isClosed);
        std::vector<CommonVertex> generateFillGeometry(const std::vector<PathPoint> &points, const glm::vec4 &color);
        std::vector<CommonVertex> generateFillGeometry(const std::vector<std::vector<PathPoint>> &contours, const glm::vec4 &color);
        std::vector<uint32_t> generateFillIndices(size_t vertexCount);

        QuadBezierCurvePoints generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);
    };

} // namespace Bess::Renderer2D::Vulkan
