#pragma once

#include "fwd.hpp"

#include "scene/renderer/font.h"
#include "scene/renderer/gl/shader.h"
#include "scene/renderer/gl/vao.h"
#include "scene/renderer/gl/vertex.h"
#include "scene/renderer/gl/texture.h"
#include "scene/renderer/gl/subtexture.h"
#include "scene/renderer/msdf_font.h"

#include "camera.h"
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D {

    struct RenderData {
        std::vector<Gl::CircleVertex> circleVertices;
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

    struct PathContext {
        bool ended = true;
        float weight = 1.f;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec3 currentPos;
        std::vector<glm::vec3> points;

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
        static void doShadowRenderPass(float width, float height);
        static void doCompositeRenderPass(float width, float height);

        // --- path api start---
        static void beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color);
        static void endPathMode(bool closePath = false);
        static void pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, const int id);
        static void pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                      float weight, const glm::vec4 &color, const int id);
        static void pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id);
        // --- path api end ---

        static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                         const glm::vec4 &color, int id, QuadRenderProperties properties = {});

        static void quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::Texture>,
                         const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        static void quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::SubTexture>,
                         const glm::vec4 &tintColor, int id, QuadRenderProperties properties = {});

        static void curve(const glm::vec3 &start, const glm::vec3 &end, float weight, const glm::vec4 &color, int id);

        static void quadraticBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id, bool breakCurve = true);

        static void cubicBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, float weight, const glm::vec4 &color, const int id, bool breakCurve = true);

        static void circle(const glm::vec3 &center, float radius,
                           const glm::vec4 &color, int id, float innerRadius = 0.0f);

        static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const glm::vec4 &color);

        static void text(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id, float angle = 0.f);

        static void msdfText(const std::string &data, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id, float angle = 0.f);

        static void line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id);

        static void drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed = false);
        static void drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const std::vector<int> &ids, bool closed = false);

        static void triangle(const std::vector<glm::vec3> &points, const glm::vec4 &color, const int id);

      private:
        static std::vector<Gl::Vertex> generateStrokeGeometry(const std::vector<glm::vec3> &points,
                                                              float width,
                                                              const glm::vec4 &color,
                                                              int id, float miterLimit, bool isClosed);

        static void addCurveSegmentStrip( const glm::vec3 &prev_, const glm::vec3 &curr_, const glm::vec4 &color,
            int id, float weight, bool firstSegment);

        static int calculateQuadBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2);

        static int calculateCubicBezierSegments(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const glm::vec2 &p3);

        static void addLineVertices(const std::vector<Gl::Vertex> &vertices);

        static void addCircleVertices(const std::vector<Gl::CircleVertex> &vertices);

        static void addTriangleVertices(const std::vector<Gl::Vertex> &vertices);

        static void addQuadVertices(const std::vector<Gl::QuadVertex> &vertices);

        static void flush(PrimitiveType type);

        static QuadBezierCurvePoints generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius);

        static glm::vec2 nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                                              const glm::vec2 &p2, const glm::vec2 &p3, const float t);

        static glm::vec2 nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const float t);

        static std::vector<glm::vec3> generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end);
        static std::vector<glm::vec3> generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end);

      private:
        static std::vector<Gl::RenderPassVertex> getRenderPassVertices(float width, float height);

        static std::vector<std::shared_ptr<Gl::Shader>> m_shaders;
        static std::vector<std::unique_ptr<Gl::Vao>> m_vaos;
        static std::vector<size_t> m_MaxRenderLimit;

        static std::shared_ptr<Camera> m_camera;

        static std::vector<PrimitiveType> m_AvailablePrimitives;

        static std::vector<glm::vec4> m_StandardQuadVertices;

        static std::vector<glm::vec4> m_StandardTriVertices;


        static RenderData m_RenderData;

        static std::shared_ptr<Gl::Shader> m_GridShader;
        static std::unique_ptr<Gl::Shader> m_shadowPassShader;
        static std::unique_ptr<Gl::Shader> m_compositePassShader;
        static std::unique_ptr<Gl::Vao> m_GridVao;
        static std::unique_ptr<Gl::Vao> m_renderPassVao;

        static std::shared_ptr<Font> m_Font;

        static std::vector<Gl::Vertex> m_curveStripVertices;
        static std::vector<GLuint> m_curveStripIndices;

        static std::vector<Gl::Vertex> m_pathStripVertices;
        static std::vector<GLuint> m_pathStripIndices;

        static bool m_curveBroken;

        static PathContext m_pathData;

        static std::unordered_map<std::shared_ptr<Gl::Texture>, std::vector<Gl::QuadVertex>> m_textureQuadVertices;

        static std::shared_ptr<MsdfFont> m_msdfFont;
    };

} // namespace Bess::Renderer2D
