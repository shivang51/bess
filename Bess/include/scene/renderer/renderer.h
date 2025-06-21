#pragma once

#include "fwd.hpp"

#include "scene/renderer/font.h"
#include "scene/renderer/gl/shader.h"
#include "scene/renderer/gl/vao.h"
#include "scene/renderer/gl/vertex.h"

#include "camera.h"
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D {

    struct RenderData {
        std::vector<Gl::Vertex> circleVertices;
        std::vector<Gl::Vertex> lineVertices;
        std::vector<Gl::Vertex> curveVertices;
        std::vector<Gl::Vertex> fontVertices;
        std::vector<Gl::Vertex> triangleVertices;
        std::vector<Gl::QuadVertex> quadVertices;
        std::vector<Gl::QuadVertex> quadShadowVertices;
    };

    struct QuadBezierCurvePoints {
        glm::vec2 startPoint;
        glm::vec2 controlPoint;
        glm::vec2 endPoint;
    };

    struct PathData{
        glm::vec3 currentPos;
        glm::vec3 prevPos;
        bool ended = true;

        void setCurrentPos(const glm::vec3& pos){
          prevPos = currentPos;
          currentPos = pos;
        }
    };

    class Renderer {
      public:
        Renderer() = default;

        static void init();

        static void begin(std::shared_ptr<Camera> camera);

        static void end();

        static glm::vec2 getCharRenderSize(char ch, float renderSize);
        static glm::vec2 getStringRenderSize(const std::string &str, float renderSize);

      public:
        static void doShadowRenderPass(float width, float height);
        static void doCompositeRenderPass(float width, float height);

        // --- path api start---
        // TODO(Shivang): fix angle lines and closing path
        static void beginPathMode(const glm::vec3& startPos);
        static void endPathMode(bool closePath = false);
        static void pathLineTo(const glm::vec3& pos, float size, const glm::vec4 &color, const int id);
        // --- path api end ---

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         float borderSize = 0.f);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius,
                         const glm::vec4 &borderSize,
                         const glm::vec4 &borderColor);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, float angle, bool isMica = true,
                         const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         float borderSize = 0.f);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius,
                         bool shadow,
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderSize = glm::vec4(0.f));

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius,
                         bool shadow,
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         float borderSize = 0.f);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, float angle,
                         const glm::vec4 &borderRadius,
                         bool shadow,
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderSize = glm::vec4(0.f), bool isMica = true);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, float angle,
                         const glm::vec4 &borderRadius,
                         const glm::vec4 &borderSize,
                         const glm::vec4 &borderColor,
                         bool isMica = true);

        static void curve(const glm::vec3 &start, const glm::vec3 &end, float weight, const glm::vec4 &color, int id);

        static void quadraticBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id, bool breakCurve = true);

        static void cubicBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, float weight, const glm::vec4 &color, const int id, bool breakCurve = true);

        static void circle(const glm::vec3 &center, float radius,
                           const glm::vec4 &color, int id);

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const glm::vec4 &color);

        static void text(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id, float angle = 0.f);

        static void line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id);

        static void drawPath(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed = false);
        static void drawPath(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const std::vector<int> &ids, bool closed = false);

        static void triangle(const std::vector<glm::vec3> &points, const glm::vec4 &color, const int id);

      private:
        static void addCurveSegmentStrip(
            const glm::vec3 &prev_,
            const glm::vec3 &curr_,
            const glm::vec4 &color,
            int id,
            float weight,
            bool firstSegment);

        static void addPathSegmentStrip(
            const glm::vec3 &prev_,
            const glm::vec3 &curr_,
            const glm::vec4 &color,
            int id,
            float weight
          );

        static void addSharpJoinTriangle(
            const glm::vec3 &prev,
            const glm::vec3 &joint,
            const glm::vec3 &next,
            const glm::vec4 &color,
            int id,
            float weight);

        static int calculateQuadBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2);

        static int calculateCubicBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3);

        static void addLineVertices(const std::vector<Gl::Vertex> &vertices);

        static void addCircleVertices(const std::vector<Gl::Vertex> &vertices);

        static void addTriangleVertices(const std::vector<Gl::Vertex> &vertices);

        static void addQuadVertices(const std::vector<Gl::QuadVertex> &vertices);

        static void flush(PrimitiveType type);

        static void drawQuad(const glm::vec3 &pos, const glm::vec2 &size,
                             const glm::vec4 &color, int id, float angle,
                             bool isMica,
                             const glm::vec4 &borderRadius,
                             const glm::vec4 &borderSize, const glm::vec4 &borderColor);

        static QuadBezierCurvePoints generateQuadBezierPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);

      private:
        static std::vector<Gl::RenderPassVertex> getRenderPassVertices(float width, float height);

        static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>> m_shaders;

        static std::unique_ptr<Gl::Shader> m_quadShadowShader;

        static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> m_vaos;

        static std::shared_ptr<Camera> m_camera;

        static std::vector<PrimitiveType> m_AvailablePrimitives;

        static std::vector<glm::vec4> m_StandardQuadVertices;

        static std::vector<glm::vec4> m_StandardTriVertices;

        static std::unordered_map<PrimitiveType, size_t> m_MaxRenderLimit;

        static RenderData m_RenderData;

        static std::unique_ptr<Gl::Shader> m_GridShader;
        static std::unique_ptr<Gl::Shader> m_shadowPassShader;
        static std::unique_ptr<Gl::Shader> m_compositePassShader;
        static std::unique_ptr<Gl::Vao> m_GridVao;
        static std::unique_ptr<Gl::Vao> m_renderPassVao;
        static std::unordered_map<std::string, glm::vec2> m_charSizeCache;

        static std::unique_ptr<Font> m_Font;

        static std::vector<Gl::Vertex> m_curveStripVertices;
        static std::vector<GLuint> m_curveStripIndices;

        static std::vector<Gl::Vertex> m_pathStripVertices;
        static std::vector<GLuint> m_pathStripIndices;

        static bool m_curveBroken;

        static PathData m_pathData;
    };

} // namespace Bess::Renderer2D
