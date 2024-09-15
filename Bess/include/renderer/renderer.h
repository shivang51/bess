#pragma once

#include "fwd.hpp"

#include "renderer/font.h"
#include "renderer/gl/shader.h"
#include "renderer/gl/vao.h"
#include "renderer/gl/vertex.h"

#include "camera.h"
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D {

    struct RenderData {
        std::vector<Gl::Vertex> circleVertices;
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

    class Renderer {
      public:
        Renderer() = default;

        static void init();

        static void begin(std::shared_ptr<Camera> camera);

        static void end();

        static glm::vec2 getCharRenderSize(char ch, float renderSize);

      public:
        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         float borderSize = 0.f);

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id,
                         const glm::vec4 &borderRadius,
                         const glm::vec4 &borderColor,
                         const glm::vec4 &borderSize = glm::vec4(0.f));

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, float angle,
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
                         const glm::vec4 &borderSize = glm::vec4(0.f));

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, float angle,
                         const glm::vec4 &borderRadius,
                         const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                         const glm::vec4 &borderSize = glm::vec4(0.f));

        static void curve(const glm::vec3 &start, const glm::vec3 &end, float weight, const glm::vec4 &color, int id);

        static void quadraticBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id, bool pathMode = false);

        static void cubicBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, float weight, const glm::vec4 &color, const int id);

        static void circle(const glm::vec3 &center, float radius,
                           const glm::vec4 &color, int id);

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const glm::vec4 &color);

        static void text(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id);

        static void line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id);

        static void drawPath(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed = false);

        static void triangle(const std::vector<glm::vec3> &points, const glm::vec4 &color, const int id);

      private:
        static void createCurveVertices(const glm::vec3 &start,
                                        const glm::vec3 &end,
                                        const glm::vec4 &color, int id, float weight = 3.0f);

        static int calculateSegments(const glm::vec2 &p1, const glm::vec2 &p2);

        static void addCircleVertices(const std::vector<Gl::Vertex> &vertices);

        static void addTriangleVertices(const std::vector<Gl::Vertex> &vertices);

        static void addCurveVertices(const std::vector<Gl::Vertex> &vertices);

        static void addQuadVertices(const std::vector<Gl::QuadVertex> &vertices);

        static void flush(PrimitiveType type);

        static void drawQuad(const glm::vec3 &pos, const glm::vec2 &size,
                             const glm::vec4 &color, int id, float angle,
                             const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f});

        static QuadBezierCurvePoints generateQuadBezierPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);

      private:
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
        static std::unique_ptr<Gl::Vao> m_GridVao;

        static std::unique_ptr<Font> m_Font;
    };

} // namespace Bess::Renderer2D
