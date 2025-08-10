#pragma once

#include "vao.h"

namespace Bess::Gl {
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
} // namespace Bess::Gl