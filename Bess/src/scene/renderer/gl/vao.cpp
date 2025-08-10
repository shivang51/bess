#include "scene/renderer/gl/vao.h"
#include <cstddef>
#include <iostream>
#include <vector>

#include "scene/renderer/gl/gl_wrapper.h"
#include "scene/renderer/gl/vertex.h"

namespace Bess::Gl {
    static uint32_t getGLTypeSize(GLenum type) {
        switch (type) {
        case GL_FLOAT:
            return 4;
        case GL_UNSIGNED_INT:
            return 4;
        case GL_INT:
            return 4;
        case GL_UNSIGNED_BYTE:
            return 1;
        }
        return 0;
    }

    void BufferLayout::calculateOffsetsAndStride() {
        size_t offset = 0;
        m_Stride = 0;
        for (auto &element : m_Elements) {
            element.offset = offset;
            offset += element.count * getGLTypeSize(element.type);
            m_Stride += element.count * getGLTypeSize(element.type);
        }
    }

    // --- Vbo Implementations ---

    Vbo::Vbo(size_t size, const void *data) {
        GL_CHECK(glGenBuffers(1, &m_id));
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_id));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW));
    }

    Vbo::~Vbo() {
        GL_CHECK(glDeleteBuffers(1, &m_id));
    }

    void Vbo::bind() const {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_id));
    }

    void Vbo::unbind() const {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    void Vbo::setData(const void *data, size_t size) {
        bind();
        GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, 0, size, data));
    }

    // --- Ibo Implementations ---

    Ibo::Ibo(size_t count, const GLuint *data) : m_Count(count) {
        GL_CHECK(glGenBuffers(1, &m_id));
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_id));
        GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(GLuint), data, GL_STATIC_DRAW));
    }

    Ibo::~Ibo() {
        GL_CHECK(glDeleteBuffers(1, &m_id));
    }

    void Ibo::bind() const {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_id));
    }

    void Ibo::unbind() const {
        GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    }

    void Ibo::setData(const void *data, size_t size) {
        bind();
        GL_CHECK(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size * sizeof(GLuint), data));
    }

    Vao::Vao() {
        GL_CHECK(glGenVertexArrays(1, &m_vao_id));
    }

    Vao::~Vao() {
        GL_CHECK(glDeleteVertexArrays(1, &m_vao_id));
    }

    void Vao::bind() const {
        GL_CHECK(glBindVertexArray(m_vao_id));
    }

    void Vao::unbind() const {
        GL_CHECK(glBindVertexArray(0));
    }

    void Vao::setIndexBuffer(const Ibo &ibo) {
        bind();
        ibo.bind();
    }

    void Vao::addVertexBuffer(const Vbo &vbo, const BufferLayout &layout) {
        bind();
        vbo.bind();

        for (const auto &element : layout.getElements()) {
            GL_CHECK(glEnableVertexAttribArray(m_attribIndex));
            if (element.type == GL_INT) {
                GL_CHECK(glVertexAttribIPointer(
                    m_attribIndex,
                    element.count,
                    element.type,
                    layout.getStride(),
                    (const void *)element.offset // Use the calculated offset
                    ));
            } else {
                GL_CHECK(glVertexAttribPointer(
                    m_attribIndex,
                    element.count,
                    element.type,
                    element.normalized ? GL_TRUE : GL_FALSE,
                    layout.getStride(),
                    (const void *)element.offset // Use the calculated offset
                    ));
            }

            if (element.isInstanced) {
                GL_CHECK(glVertexAttribDivisor(m_attribIndex, 1));
            }
            m_attribIndex++;
        }
    }

} // namespace Bess::Gl
