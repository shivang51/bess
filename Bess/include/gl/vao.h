#pragma once

#include "gl_wrapper.h"

#include "vertex.h"
#include "gl/primitive_type.h"
#include <vector>
namespace Bess::Gl {
class Vao {
  public:
    Vao(size_t max_vertices, size_t max_indices, Renderer2D::PrimitiveType type);
    ~Vao();

    void bind() const;
    void unbind() const;
    GLuint getId() const;

    void setVertices(const std::vector<Vertex> &verticesconst, const std::vector<QuadVertex>& quadRenderVertices = {});

  private:
    std::vector<Vertex> m_vertices;
    Renderer2D::PrimitiveType m_type;
    GLuint m_vao_id = -1, m_vbo_id = -1, m_ibo_id = -1;
};
} // namespace Bess::Gl
