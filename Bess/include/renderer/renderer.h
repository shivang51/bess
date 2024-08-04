#pragma once

#include "fwd.hpp"

#include "renderer/gl/shader.h"
#include "renderer/gl/vao.h"
#include "renderer/gl/vertex.h"
#include "renderer/font.h"

#include "camera.h"
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D {

struct RenderData {
    std::vector<Gl::Vertex> circleVertices;
    std::vector<Gl::Vertex> curveVertices;
    std::vector<Gl::Vertex> fontVertices;
    std::vector<Gl::QuadVertex> quadVertices;
};

class Renderer {
  public:
    Renderer() = default;

    static void init();

    static void begin(std::shared_ptr<Camera> camera);
    static void end();

    static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                     const glm::vec3 &color, int id,
                     const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f},
                     const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                     float borderSize = 0.f);

    static void quad(const glm::vec3 &pos, const glm::vec2 &size,
                     const glm::vec3 &color, int id, float angle,
                     const glm::vec4 &borderRadius = {0.f, 0.f, 0.f, 0.f},
                     const glm::vec4 &borderColor = {0.f, 0.f, 0.f, 0.f},
                     float borderSize = 0.f);

    static void curve(const glm::vec3 &start, const glm::vec3 &end,
                      const glm::vec3 &color, int id);

    static void circle(const glm::vec3 &center, float radius,
                       const glm::vec3 &color, int id);

    static void grid(const glm::vec3 &pos, const glm::vec2 &size, int id);

    static void text(const std::string& data, const glm::vec3& pos, const size_t size, const glm::vec3& color, const int id);

  private:
    static void createCurveVertices(const glm::vec3 &start,
                                    const glm::vec3 &end,
                                    const glm::vec3 &color, int id, float weight = 3.0f);

    static void addCircleVertices(const std::vector<Gl::Vertex> &vertices);

    static void addCurveVertices(const std::vector<Gl::Vertex> &vertices);

    static void addQuadVertices(const std::vector<Gl::QuadVertex> &vertices);

    static void flush(PrimitiveType type);

  private:
    static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>>
        m_shaders;

    static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> m_vaos;

    static std::shared_ptr<Camera> m_camera;

    static std::vector<PrimitiveType> m_AvailablePrimitives;

    static std::vector<glm::vec4> m_StandardQuadVertices;

    static std::unordered_map<PrimitiveType, size_t> m_MaxRenderLimit;

    static RenderData m_RenderData;

    static std::unique_ptr<Gl::Shader> m_GridShader;
    static std::unique_ptr<Gl::Vao> m_GridVao;

    static std::unique_ptr<Font> m_Font;
};

} // namespace Bess::Renderer2D
