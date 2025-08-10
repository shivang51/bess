#pragma once
#include "instanced_vao.h"

namespace Bess::Gl {
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

    class TriangleVao : public InstancedVao<InstanceVertex> {
      public:
        TriangleVao(size_t limit) : InstancedVao(limit, 
            sizeof(TriangleTemplateVertices), (void*)TriangleTemplateVertices.data(),
            sizeof(TriangleTemplateIndices), (GLuint*)TriangleTemplateIndices.data()
        ) {}
    };

    class GridVao {
      public:
        GridVao() {
            m_vao = std::make_unique<Vao>();

            m_vbo = std::make_unique<Vbo>(sizeof(GridVertex) * 4);
            m_vao->addVertexBuffer(*m_vbo, getVertexLayout());

            GLuint indicies[] = {0, 1, 2, 2, 3, 0};
            m_ibo = std::make_unique<Ibo>(6, indicies);
            m_vao->setIndexBuffer(*m_ibo);
        }

        void bind() const {
            m_vao->bind();
        }

        void unbind() const {
            m_vao->unbind();
        }

        void setVertices(const std::vector<GridVertex> &verticies) {
            m_vbo->setData(verticies.data(), verticies.size() * sizeof(Vertex));
        }

        BufferLayout getVertexLayout() {
            return BufferLayout{
                {GL_FLOAT, 3, sizeof(GridVertex), offsetof(GridVertex, position), false, false},
                {GL_FLOAT, 2, sizeof(GridVertex), offsetof(GridVertex, texCoord), false, false},
                {GL_INT  , 1, sizeof(GridVertex), offsetof(GridVertex, id), false, false},
                {GL_FLOAT, 4, sizeof(GridVertex), offsetof(GridVertex, color), false, false},
                {GL_FLOAT, 1, sizeof(GridVertex), offsetof(GridVertex, ar), false, false},
            };
        }
      protected:
        std::unique_ptr<Vao> m_vao;
        std::unique_ptr<Ibo> m_ibo;
        std::unique_ptr<Vbo> m_vbo;
        bool m_isQuad;
    };

}