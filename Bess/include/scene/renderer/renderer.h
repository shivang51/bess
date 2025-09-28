#pragma once

#include "fwd.hpp"

#include "scene/renderer/font.h"
#include "scene/renderer/gl/batch_vao.h"
#include "scene/renderer/gl/shader.h"
#include "scene/renderer/gl/subtexture.h"
#include "scene/renderer/gl/texture.h"
#include "scene/renderer/gl/vaos.h"
#include "scene/renderer/gl/vertex.h"
#include "scene/renderer/msdf_font.h"

#include "camera.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D {

    struct PathRenderData {
        std::vector<GLuint> strokeIndices = {};
        std::vector<GLuint> fillIndices = {};
        std::vector<Gl::Vertex> strokeVertices = {};
        std::vector<Gl::Vertex> fillVertices = {};
    };

    struct RenderData {
        std::vector<Gl::CircleVertex> circleVertices;
        std::vector<Gl::InstanceVertex> lineVertices;
        std::vector<Gl::InstanceVertex> fontVertices;
        std::vector<Gl::Vertex> triangleVertices;
        std::vector<Gl::QuadVertex> quadVertices;
        PathRenderData pathData = {};
    };

    struct QuadBezierCurvePoints {
        glm::vec2 startPoint;
        glm::vec2 controlPoint;
        glm::vec2 endPoint;
    };

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

    class Renderer {
      public:
        Renderer() = default;

        static void init();

        static void begin(std::shared_ptr<Camera> camera);

        static void end();

        static glm::vec2 getCharRenderSize(char ch, float renderSize);
        static glm::vec2 getTextRenderSize(const std::string &str, float renderSize);
        static glm::vec2 getMSDFTextRenderSize(const std::string &str, float renderSize);

      public:
        // --- path api start---
        static void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, const uint64_t id);
        static void endPathMode(bool closePath = false, bool genFill = false, const glm::vec4 &fillColor = glm::vec4(1.f), bool genStroke = true);
        static void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, const int id);
        static void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                      float weight, const glm::vec4 &color, const int id);
        static void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id);
        // --- path api end ---

        // --- quad api---
        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, QuadRenderProperties properties = {});

        static void quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::Texture>,
                         const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        static void quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::SubTexture>,
                         const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});
        // --- quad api---

        static void circle(const glm::vec3 &center, float radius,
                           const glm::vec4 &color, int id, float innerRadius = 0.0f);

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const GridColors &colors);

        static void text(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id, float angle = 0.f);

        static void msdfText(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id, float angle = 0.f);

        static void line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id);

        static void drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed = false);
        static void drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const std::vector<int> &ids, bool closed = false);

        static void triangle(const std::vector<glm::vec3> &points, const glm::vec4 &color, const int id);

      private:
        static std::vector<Gl::Vertex> generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                              const glm::vec4 &color, float miterLimit, bool isClosed);

        static std::vector<Gl::Vertex> generateFillGeometry(const std::vector<PathPoint> &points, const glm::vec4 &color);

        static void flush(PrimitiveType type);

        static QuadBezierCurvePoints generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);

        static int calculateQuadBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2);

        static int calculateCubicBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3);

        static glm::vec2 nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                                                       const glm::vec2 &p2, const glm::vec2 &p3, const float t);

        static glm::vec2 nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const float t);

        static std::vector<glm::vec3> generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end);
        static std::vector<glm::vec3> generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end);

      private:
        static std::shared_ptr<Camera> m_camera;
        static std::vector<size_t> m_MaxRenderLimit;

        static std::vector<std::shared_ptr<Gl::Shader>> m_shaders;

        static std::vector<PrimitiveType> m_AvailablePrimitives;

        static RenderData m_renderData;

        static std::shared_ptr<Gl::Shader> m_gridShader;

        static PathContext m_pathData;

        static std::unordered_map<std::shared_ptr<Gl::Texture>, std::vector<Gl::QuadVertex>> m_textureQuadVertices;

        static std::shared_ptr<Font> m_Font;
        static std::shared_ptr<MsdfFont> m_msdfFont;

        static std::unique_ptr<Gl::QuadVao> m_quadRendererVao;
        static std::unique_ptr<Gl::CircleVao> m_circleRendererVao;
        static std::unique_ptr<Gl::TriangleVao> m_triangleRendererVao;
        static std::unique_ptr<Gl::InstancedVao<Gl::InstanceVertex>> m_lineRendererVao;
        static std::unique_ptr<Gl::InstancedVao<Gl::InstanceVertex>> m_textRendererVao;
        static std::unique_ptr<Gl::BatchVao<Gl::Vertex>> m_pathRendererVao;
        static std::unique_ptr<Gl::GridVao> m_gridVao;

        static constexpr auto m_texSlots = [] {
            std::array<int, 32> arr{};
            for (int i = 0; i < 32; ++i)
                arr[i] = i;
            return arr;
        }();
    };

} // namespace Bess::Renderer2D
