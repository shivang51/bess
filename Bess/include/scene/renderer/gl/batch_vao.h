#pragma once
#include "vao.h"

namespace Bess::Gl {
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
}
