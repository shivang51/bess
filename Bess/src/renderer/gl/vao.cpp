#include "renderer/gl/vao.h"
#include <cstddef>
#include <iostream>
#include <vector>

#include "renderer/gl/gl_wrapper.h"
#include "renderer/gl/vertex.h"

namespace Bess::Gl
{

    Vao::Vao(size_t max_vertices, size_t max_indices, const std::vector<VaoAttribAttachment> &attachments, size_t vertex_size)
    {
        m_vertex_size = vertex_size;
        GL_CHECK(glGenVertexArrays(1, &m_vao_id));
        GL_CHECK(glBindVertexArray(m_vao_id));

        GL_CHECK(glGenBuffers(1, &m_vbo_id));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo_id));

        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, max_vertices * vertex_size,
                              nullptr, GL_DYNAMIC_DRAW));

        for (int i = 0; i < attachments.size(); i++)
        {
            GLenum type;
            GLuint size;

            auto attachment = attachments[i];

            switch (attachment.type)
            {
            case VaoAttribType::vec2:
                type = GL_FLOAT;
                size = 2;
                break;
            case VaoAttribType::vec3:
                type = GL_FLOAT;
                size = 3;
                break;
            case VaoAttribType::vec4:
                type = GL_FLOAT;
                size = 4;
                break;
            case VaoAttribType::float_t:
                type = GL_FLOAT;
                size = 1;
                break;
            case VaoAttribType::int_t:
                type = GL_INT;
                size = 1;
                break;
            default:
                break;
            }

            GL_CHECK(glEnableVertexAttribArray(i));
            if (attachment.type == VaoAttribType::int_t)
            {
                GL_CHECK(glVertexAttribIPointer(i, 1, GL_INT, vertex_size, (const void *)attachment.offset));
            }
            else
            {
                GL_CHECK(glVertexAttribPointer(i, size, type, GL_FALSE, vertex_size, (const void *)attachment.offset));
            }
        }

        std::vector<GLuint> indices = {0, 1, 2, 2, 3, 0};

        for (size_t i = 6; i < max_indices; i++)
        {
            indices.push_back(indices[i - 6] + 4);
        }

        GL_CHECK(glGenBuffers(1, &m_ibo_id));
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_indices * sizeof(GLuint),
                              indices.data(), GL_STATIC_DRAW));

        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
        GL_CHECK(glBindVertexArray(0));
    }

    Vao::~Vao()
    {
        glDeleteBuffers(1, &m_vbo_id);
        glDeleteBuffers(1, &m_ibo_id);
        glDeleteVertexArrays(1, &m_vao_id);
    }

    GLuint Vao::getId() const { return m_vao_id; }
    
    GLuint Vao::getVboId() const { return m_vbo_id; }

    void Vao::bind() const { GL_CHECK(glBindVertexArray(m_vao_id)); }

    void Vao::unbind() const { glBindVertexArray(0); }

    void Vao::setVertices(const void *data, size_t count)
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo_id));
        GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertex_size * count, data));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
} // namespace Bess::Gl
