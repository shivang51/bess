#pragma once

#include "gl_wrapper.h"
#include "primitive_type.h"
#include "vertex.h"

#include <iostream>
#include <array>
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

    // VBO Wrapper
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

    // IBO Wrapper
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

        // New methods to link buffers and layouts
        void addVertexBuffer(const Vbo &vbo, const BufferLayout &layout);
        void setIndexBuffer(const Ibo &ibo);

      private:
        GLuint m_vao_id = 0;
        uint32_t m_attribIndex = 0;
    };

    // A simple internal struct for the template vertex data.
    // This is separate from QuadVertex because it only contains per-vertex attributes.
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
            {{-0.5f, -0.5f}, {0.0f, 0.0f}}, // Position, TexCoord (Bottom-left)
            {{0.5f, -0.5f}, {1.0f, 0.0f}},  // (Bottom-right)
            {{0.f, 0.5f}, {1.0f, 1.0f}},   // (Top-right)
        }};

        constexpr std::array<GLuint, 3> TriangleTemplateIndices = {
            0, 1, 2, // First triangle
        };
    } // namespace

    template <typename VertexType>
    class InstancedVao {
      public:
        InstancedVao(size_t maxInstances) {
            m_vao = std::make_unique<Vao>();
            m_templateVbo = std::make_unique<Vbo>(QuadTemplateVertices.size() * sizeof(TemplateVertex), QuadTemplateVertices.data());
            m_vao->addVertexBuffer(*m_templateVbo, getTemplateLayout());

            m_ibo = std::make_unique<Ibo>(QuadIndices.size(), QuadIndices.data());
            m_vao->setIndexBuffer(*m_ibo);

            m_instanceVbo = std::make_unique<Vbo>(maxInstances * sizeof(VertexType));
            m_vao->addVertexBuffer(*m_instanceVbo, getInstanceLayout());
        }

        InstancedVao(size_t maxInstances, BufferLayout instanceLayout) {
            m_vao = std::make_unique<Vao>();
            m_templateVbo = std::make_unique<Vbo>(QuadTemplateVertices.size() * sizeof(TemplateVertex), QuadTemplateVertices.data());
            m_vao->addVertexBuffer(*m_templateVbo, getTemplateLayout());

            m_ibo = std::make_unique<Ibo>(QuadIndices.size(), QuadIndices.data());
            m_vao->setIndexBuffer(*m_ibo);

            m_instanceVbo = std::make_unique<Vbo>(maxInstances * sizeof(VertexType));
            m_vao->addVertexBuffer(*m_instanceVbo, instanceLayout);
        }

        InstancedVao(size_t maxInstances, size_t templateVerticiesSize,
                     void *templateVertices, size_t templateIndiciesSize, GLuint *templateIndicies) {
            m_vao = std::make_unique<Vao>();
            m_templateVbo = std::make_unique<Vbo>(templateVerticiesSize, templateVertices);
            m_vao->addVertexBuffer(*m_templateVbo, getTemplateLayout());

            m_ibo = std::make_unique<Ibo>(templateIndiciesSize, templateIndicies);
            m_vao->setIndexBuffer(*m_ibo);

            m_instanceVbo = std::make_unique<Vbo>(maxInstances * sizeof(VertexType));
            m_vao->addVertexBuffer(*m_instanceVbo, getInstanceLayout());
        }

        ~InstancedVao() = default;

        InstancedVao(const InstancedVao &) = delete;
        InstancedVao &operator=(const InstancedVao &) = delete;
        InstancedVao(InstancedVao &&) = delete;
        InstancedVao &operator=(InstancedVao &&) = delete;

        void bind() const {
            m_vao->bind();
        }

        void unbind() const {
            m_vao->unbind();
        }

        virtual BufferLayout getTemplateLayout() {
            return BufferLayout{
                {GL_FLOAT, 2, sizeof(TemplateVertex), offsetof(TemplateVertex, position), false}, // a_LocalPosition
                {GL_FLOAT, 2, sizeof(TemplateVertex), offsetof(TemplateVertex, texCoord), false}  // a_TexCoord
            };
        };

        virtual BufferLayout getInstanceLayout() {
            return BufferLayout{
                {GL_FLOAT, 3, sizeof(InstanceVertex), offsetof(InstanceVertex, position), false, true},
                {GL_FLOAT, 2, sizeof(InstanceVertex), offsetof(InstanceVertex, size), false, true},
                {GL_FLOAT, 1, sizeof(InstanceVertex), offsetof(InstanceVertex, angle), false, true},
                {GL_FLOAT, 4, sizeof(InstanceVertex), offsetof(InstanceVertex, color), false, true},
                {GL_INT, 1, sizeof(InstanceVertex), offsetof(InstanceVertex, id), false, true},
                {GL_INT, 1, sizeof(InstanceVertex), offsetof(InstanceVertex, texSlotIdx), false, true},
                {GL_FLOAT, 4, sizeof(InstanceVertex), offsetof(InstanceVertex, texData), false, true},
            };
        }

        void updateInstanceData(const std::vector<VertexType> &vertexData) {
            m_instanceVbo->setData(vertexData.data(), vertexData.size() * sizeof(VertexType));
        }

        size_t getIndexCount() const {
            if (m_ibo) {
                return m_ibo->getCount();
            }
            return 0;
        }

      protected:
        std::unique_ptr<Vao> m_vao;
        std::unique_ptr<Vbo> m_templateVbo;
        std::unique_ptr<Ibo> m_ibo;
        std::unique_ptr<Vbo> m_instanceVbo;
    };

    class QuadVao : public InstancedVao<QuadVertex> {
      public:
        QuadVao(size_t limit) : InstancedVao(limit, getInstanceLayout()) {}
        BufferLayout getInstanceLayout() override;
    };

    class CircleVao : public InstancedVao<CircleVertex> {
      public:
        CircleVao(size_t limit) : InstancedVao(limit, getInstanceLayout()) {}
        BufferLayout getInstanceLayout() override;
    };
    
    template<typename VertexType>
    class BatchVao {
      public:
        BatchVao(size_t maxEntities, size_t verticesPerEntt, size_t indicesPerEntt, bool genIndicies = true, bool isQuad = true) : m_isQuad(isQuad) {
            m_vao = std::make_unique<Vao>();

            m_vbo = std::make_unique<Vbo>(sizeof(VertexType) * maxEntities * verticesPerEntt);
            m_vao->addVertexBuffer(*m_vbo, getVertexLayout());
            

            GLuint *indicies = nullptr;
            if (genIndicies) {
                auto info = getIndiciesInfo();
                indicies = genIndiciesData(maxEntities, info.first, info.second);
            }

            // passing count here not size, need to fix this
            m_ibo = std::make_unique<Ibo>(maxEntities * indicesPerEntt, indicies);
            m_vao->setIndexBuffer(*m_ibo);

            if (indicies != nullptr)
                delete indicies;
        }

		~BatchVao() = default;
        BatchVao(const BatchVao &) = delete;
        BatchVao &operator=(const BatchVao &) = delete;
        BatchVao(BatchVao &&) = delete;
        BatchVao &operator=(BatchVao &&) = delete;
        
        virtual BufferLayout getVertexLayout() {
            return BufferLayout{
                {GL_FLOAT, 3, sizeof(Vertex), offsetof(Vertex, position), false, false},
                {GL_FLOAT, 4, sizeof(Vertex), offsetof(Vertex, color), false, false},
                {GL_FLOAT, 2, sizeof(Vertex), offsetof(Vertex, texCoord), false, false},
                {GL_INT, 1, sizeof(Vertex), offsetof(Vertex, id), false, false},
                {GL_INT, 1, sizeof(Vertex), offsetof(Vertex, texSlotIdx), false, false},
            };
        }
        
        virtual std::pair<int, const std::vector<GLuint>> getIndiciesInfo() {
            if (m_isQuad) {
                return {
                    4,
                    {0, 1, 2, 2, 3, 0}
                };
            } else {
                return {
                    3,
                    {0, 1, 2}
                };
            }
        }

        void bind() const {
            m_vao->bind();
        }

        void unbind() const {
            m_vao->unbind();
        }

        void setVertices(const std::vector<VertexType> &verticies) {
            m_vbo->setData(verticies.data(), verticies.size() * sizeof(Vertex));
        }

        void setVerticesAndIndicies(const std::vector<VertexType>& verticies, const std::vector<GLuint>& indicies){
            m_vbo->setData(verticies.data(), verticies.size() * sizeof(VertexType));
            m_ibo->setData(indicies.data(), indicies.size() * sizeof(GLuint));
        }

        private:
        GLuint *genIndiciesData(
            size_t maxEntt, 
            int increment, 
            const std::vector<GLuint>& enttIndicies // indicies of single entity
        ) {
            size_t len = enttIndicies.size();
            size_t maxIndicies = len * maxEntt;
            GLuint *indicies = new GLuint[maxIndicies];
            for (int i = 0; i < len; i++)
                indicies[i] = enttIndicies[i];

            for (size_t i = len; i < maxIndicies; i++) {
                indicies[i] = indicies[i - len] + increment;
            }

            return indicies;
        }

      protected:
        std::unique_ptr<Vao> m_vao;
        std::unique_ptr<Ibo> m_ibo;
        std::unique_ptr<Vbo> m_vbo;
        bool m_isQuad;
    };

    class TriangleVao : public InstancedVao<InstanceVertex> {
      public:
        TriangleVao(size_t limit) : InstancedVao(limit, 
            sizeof(TriangleTemplateVertices), (void*)TriangleTemplateVertices.data(),
            sizeof(TriangleTemplateIndices), (GLuint*)TriangleTemplateIndices.data()
        ) {}
    };

} // namespace Bess::Gl
