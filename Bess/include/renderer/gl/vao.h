#pragma once

#include "gl_wrapper.h"
#include "primitive_type.h"

#include "vertex.h"

#include <vector>
namespace Bess::Gl {

enum class VaoAttribType { vec2, vec3, vec4, int_t, float_t };

struct VaoAttribAttachment {
    VaoAttribType type;
    size_t offset;

    VaoAttribAttachment(VaoAttribType type, size_t offset)
        : type(type), offset(offset) {}
};

class Vao {
  public:
    Vao(size_t max_vertices, size_t max_indices,
        const std::vector<VaoAttribAttachment> &attachments,
        size_t vertex_size);
    ~Vao();

    void bind() const;
    void unbind() const;
    GLuint getId() const;
    void setVertices(const void *data, size_t count);

  private:
    GLuint m_vao_id = -1, m_vbo_id = -1, m_ibo_id = -1;
    size_t m_vertex_size;
    std::vector<VaoAttribAttachment> m_attachments;
};
} // namespace Bess::Gl
