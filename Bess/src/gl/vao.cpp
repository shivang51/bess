#include "gl/vao.h"
#include <cstddef>
#include <iostream>
#include <vector>

#include "gl/gl_wrapper.h"
#include "gl/vertex.h"

namespace Bess::Gl {

Vao::Vao(size_t max_vertices, size_t max_indices) {
    GL_CHECK(glGenVertexArrays(1, &m_vao_id));
    GL_CHECK(glBindVertexArray(m_vao_id));

    GL_CHECK(glGenBuffers(1, &m_vbo_id));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo_id));

    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, max_vertices * sizeof(Vertex),
                          nullptr, GL_DYNAMIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(0));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                   (const void *)offsetof(Vertex, position)));

    GL_CHECK(glEnableVertexAttribArray(1));
    GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                   (const void *)offsetof(Vertex, color)));

    GL_CHECK(glEnableVertexAttribArray(2));
    GL_CHECK(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                   (const void *)offsetof(Vertex, texCoord)));

    GL_CHECK(glEnableVertexAttribArray(3));
    GL_CHECK(glVertexAttribIPointer(3, 1, GL_INT, sizeof(Vertex),
                                    (const void *)offsetof(Vertex, id)));

    std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

    for (size_t i = 6; i < max_indices; i++) {
        indices.push_back(indices[i - 6] + 4);
    }

    GL_CHECK(glGenBuffers(1, &m_ibo_id));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo_id));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_indices * sizeof(GLuint),
                          indices.data(), GL_STATIC_DRAW));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GL_CHECK(glBindVertexArray(0));
}

Vao::~Vao() {
    glDeleteBuffers(1, &m_vbo_id);
    glDeleteBuffers(1, &m_ibo_id);
    glDeleteVertexArrays(1, &m_vao_id);
}

GLuint Vao::getId() const { return m_vao_id; }

void Vao::bind() const { GL_CHECK(glBindVertexArray(m_vao_id)); }
void Vao::unbind() const { glBindVertexArray(0); }

void Vao::setVertices(const std::vector<Vertex> &vertices) {
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo_id));
    GL_CHECK(glBufferSubData(
        GL_ARRAY_BUFFER, 0, sizeof(Vertex) * vertices.size(), vertices.data()));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
}
} // namespace Bess::Gl
