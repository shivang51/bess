#pragma once

#include "gl_wrapper.h"
#include "primitive_type.h"
#include "vertex.h"

#include <array>
#include <iostream>
#include <memory>
#include <vector>

namespace Bess::Gl {
    // Describes a single element in a buffer layout
    struct BufferElement {
        GLenum type;
        uint32_t count;
        size_t size;
        size_t offset;
        bool normalized;
        bool isInstanced = false;
    };

    // Describes the memory layout of a VBO
    class BufferLayout {
      public:
        BufferLayout() = default;
        BufferLayout(const std::initializer_list<BufferElement> &elements)
            : m_Elements(elements) {
            calculateOffsetsAndStride();
        }

        uint32_t getStride() const { return m_Stride; }
        const std::vector<BufferElement> &getElements() const { return m_Elements; }

      private:
        void calculateOffsetsAndStride();
        std::vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };

    class Vbo {
      public:
        Vbo(size_t size, const void *data = nullptr);
        ~Vbo();
        void bind() const;
        void unbind() const;
        void setData(const void *data, size_t size);

      private:
        GLuint m_id = 0;
    };

    class Ibo {
      public:
        Ibo(size_t count, const GLuint *data = nullptr);
        ~Ibo();
        void bind() const;
        void unbind() const;
        size_t getCount() const { return m_Count; }
        void setData(const void *data, size_t size);

      private:
        GLuint m_id = 0;
        size_t m_Count;
    };

    enum class VaoAttribType { vec2,
                               vec3,
                               vec4,
                               int_t,
                               float_t };

    enum class VaoElementType { quad,
                                triangle,
                                triangleStrip };

    struct VaoAttribAttachment {
        VaoAttribType type;
        size_t offset;

        VaoAttribAttachment(VaoAttribType type, size_t offset)
            : type(type), offset(offset) {}
    };

    class Vao {
      public:
        Vao();
        ~Vao();

        void bind() const;
        void unbind() const;

        void addVertexBuffer(const Vbo &vbo, const BufferLayout &layout);
        void setIndexBuffer(const Ibo &ibo);

      private:
        GLuint m_vao_id = 0;
        uint32_t m_attribIndex = 0;
    };

    struct TemplateVertex {
        glm::vec2 position;
        glm::vec2 texCoord;
    };

    namespace {
        constexpr std::array<TemplateVertex, 4> QuadTemplateVertices = {{
            {{-0.5f, -0.5f}, {0.0f, 0.0f}}, // Position, TexCoord (Bottom-left)
            {{0.5f, -0.5f}, {1.0f, 0.0f}},  // (Bottom-right)
            {{0.5f, 0.5f}, {1.0f, 1.0f}},   // (Top-right)
            {{-0.5f, 0.5f}, {0.0f, 1.0f}}   // (Top-left)
        }};

        constexpr std::array<GLuint, 6> QuadIndices = {
            0, 1, 2, // First triangle
            2, 3, 0  // Second triangle
        };

        constexpr std::array<TemplateVertex, 3> TriangleTemplateVertices = {{
            {{-0.5f, -0.5f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f}, {1.0f, 0.0f}},
            {{0.f, 0.5f}, {1.0f, 1.0f}},
        }};

        constexpr std::array<GLuint, 3> TriangleTemplateIndices = {
            0, 1, 2, // First triangle
        };
    } // namespace
} // namespace Bess::Gl
