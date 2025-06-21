#pragma once

#include "gl_wrapper.h"
#include "primitive_type.h"

#include "vertex.h"

#include <vector>
namespace Bess::Gl {

enum class VaoAttribType { vec2, vec3, vec4, int_t, float_t };

enum class VaoElementType {quad, triangle, triangleStrip};

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
        size_t vertex_size, VaoElementType elementType = VaoElementType::quad);
    ~Vao();

    void bind() const;
    void unbind() const;
    GLuint getId() const;
    GLuint getVboId() const;
    void setVertices(const void *data, size_t count);
    void setVerticesAndIndices(const void *vertices, size_t verticesCount, const void *indices, size_t indicesCount);

  private:
    GLuint m_vao_id = -1, m_vbo_id = -1, m_ibo_id = -1;
    size_t m_vertex_size;
    std::vector<VaoAttribAttachment> m_attachments;
};
} // namespace Bess::Gl
