#pragma once

#include "fwd.hpp"

#include "renderer/gl/shader.h"
#include "renderer/gl/vao.h"
#include "renderer/gl/vertex.h"

#include "camera.h"
#include <memory>
#include <unordered_map>

namespace Bess::Renderer2D
{

  enum class BorderSide
  {
    none = 0,
    top = 1,
    right = 2,
    bottom = 4,
    left = 8
  };

  class Renderer
  {
  public:
    Renderer() = default;

    static void init();

    static void begin(std::shared_ptr<Camera> camera);
    static void end();

    static void quad(const glm::vec2 &pos, const glm::vec2 &size,
                     const glm::vec3 &color, const int id, const glm::vec4 borderRadius = {0.f, 0.f, 0.f, 0.f});

    static void quad(const glm::vec2 &pos, const glm::vec2 &size,
                     const glm::vec3 &color, const int id, const float angle, const glm::vec4 borderRadius = {0.f, 0.f, 0.f, 0.f});

    static void curve(const glm::vec2 &start, const glm::vec2 &end,
                      const glm::vec3 &color, const int id);

    static void circle(const glm::vec2 &center, const float radius,
                       const glm::vec3 &color, const int id);

  private:
    static glm::vec2 createCurveVertices(const glm::vec2 &start,
                                         const glm::vec2 &end,
                                         const glm::vec3 &color, const int id);

  private:
    static void addVertices(PrimitiveType type,
                            const std::vector<Gl::Vertex> &vertices, const std::vector<Gl::QuadVertex> &quadVertices);

  private:
    static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>> m_shaders;

    static std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> m_vaos;

    static std::unordered_map<PrimitiveType, std::vector<Gl::Vertex>> m_vertices;

    static std::vector<Gl::QuadVertex> m_quadRenderVertices;

    static std::unordered_map<PrimitiveType, size_t> m_maxRenderCount;

    static std::shared_ptr<Camera> m_camera;

    static std::vector<PrimitiveType> m_AvailablePrimitives;

    static void flush(PrimitiveType type);

    static std::vector<glm::vec4> m_QuadVertices;

    static int m_currentId;
    static int m_currentSubId;
  };

} // namespace Bess::Renderer2D
