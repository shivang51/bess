#include "renderer/renderer.h"
#include "camera.h"
#include "fwd.hpp"
#include "gl/gl_wrapper.h"
#include "gl/vertex.h"
#include "glm.hpp"
#include "ui.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <ext/matrix_transform.hpp>
#include <iostream>
#include <ostream>

using namespace Bess::Renderer2D;

namespace Bess
{

    std::vector<PrimitiveType> Renderer::m_AvailablePrimitives;

    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>>
        Renderer::m_shaders;
    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> Renderer::m_vaos;
    std::unordered_map<PrimitiveType, std::vector<Gl::Vertex>> Renderer::m_vertices;

    std::unordered_map<PrimitiveType, size_t> Renderer::m_maxRenderCount;

    std::shared_ptr<Camera> Renderer::m_camera;

    std::vector<glm::vec4> Renderer::m_QuadVertices;

    int Renderer::m_currentId;
    int Renderer::m_currentSubId;

    void Renderer::init()
    {

        m_AvailablePrimitives = {PrimitiveType::curve, PrimitiveType::quad,
                                 PrimitiveType::circle};
        m_maxRenderCount[PrimitiveType::quad] = 250;
        m_maxRenderCount[PrimitiveType::curve] = 250;
        m_maxRenderCount[PrimitiveType::circle] = 250;

        std::string vertexShader = "", fragmentShader = "";

        for (auto primitive : m_AvailablePrimitives)
        {
            switch (primitive)
            {
            case PrimitiveType::quad:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/quad_frag.glsl";
                break;
            case PrimitiveType::curve:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/curve_frag.glsl";
                break;
            case PrimitiveType::circle:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/circle_frag.glsl";
                break;
            }

            if (vertexShader.empty() || fragmentShader.empty())
            {
                std::cerr << "[-] Primitive " << (int)primitive
                          << "is not available" << std::endl;
                return;
            }

            auto max_render_count = m_maxRenderCount[primitive];

            m_shaders[primitive] =
                std::make_unique<Gl::Shader>(vertexShader, fragmentShader);
            m_vaos[primitive] = std::make_unique<Gl::Vao>(max_render_count * 4,
                                                          max_render_count * 6);
            m_vertices[primitive] = {};

            vertexShader.clear();
            fragmentShader.clear();
        }

        m_QuadVertices = {
            {-0.5f, 0.5f, 0.f, 1.f},
            {-0.5f, -0.5f, 0.f, 1.f},
            {0.5f, -0.5f, 0.f, 1.f},
            {0.5f, 0.5f, 0.f, 1.f},
        };

        m_currentId = 1;
    }

    void Renderer::quad(const glm::vec2 &pos, const glm::vec2 &size,
                        const glm::vec3 &color, const int id)
    {
        std::vector<Gl::Vertex> vertices(4);

        vertices[0].position = {pos.x, pos.y, 0.0f};
        vertices[1].position = {pos.x, pos.y - size.y, 0.0f};
        vertices[2].position = {pos.x + size.x, pos.y - size.y, 0.0f};
        vertices[3].position = {pos.x + size.x, pos.y, 0.0f};

        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[3].texCoord = {1.0f, 1.0f};
        vertices[2].texCoord = {1.0f, 0.0f};

        for (auto &vertex : vertices)
        {
            vertex.id = id;
            vertex.color = color;
        }

        addVertices(PrimitiveType::quad, vertices);
    }

    void Renderer::quad(const glm::vec2 &pos, const glm::vec2 &size,
                        const glm::vec3 &color, const int id, const float angle)
    {
        std::vector<Gl::Vertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), glm::vec3(pos, 0.f));
        transform = glm::rotate(transform, glm::radians(angle), {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++)
        {
            auto &vertex = vertices[i];
            vertex.position = transform * m_QuadVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addVertices(PrimitiveType::quad, vertices);
    }

    glm::vec2 bernstine(const glm::vec2 &p0, const glm::vec2 &p1,
                        const glm::vec2 &p2, const glm::vec2 &p3, const float t)
    {
        auto t_ = 1 - t;

        glm::vec2 B0 = (float)std::pow(t_, 3) * p0;
        glm::vec2 B1 = (float)(3 * t * std::pow(t_, 2)) * p1;
        glm::vec2 B2 = (float)(3 * t * t * std::pow(t_, 1)) * p2;
        glm::vec2 B3 = (float)(t * t * t) * p3;

        return B0 + B1 + B2 + B3;
    }

    glm::vec2 Renderer::createCurveVertices(const glm::vec2 &start,
                                            const glm::vec2 &end,
                                            const glm::vec3 &color, const int id)
    {
        auto dx = end.x - start.x;
        auto dy = end.y - start.y;
        auto angle = std::atan(dy / dx);
        float dis = std::sqrt((dx * dx) + (dy * dy));

        float sizeX = std::max(dis, 0.01f);
        sizeX = dis;

        glm::vec3 pos = {start.x, start.y - 0.005f, 0.f};
        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {sizeX, 0.01, 1.f});

        glm::vec3 p;

        std::vector<Gl::Vertex> vertices(4);
        for (int i = 0; i < 4; i++)
        {
            auto &vertex = vertices[i];
            vertex.position = transform * m_QuadVertices[i];
            vertex.id = id;
            vertex.color = color;

            if (i == 3)
                p = vertex.position;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addVertices(PrimitiveType::curve, vertices);
        return p;
    }

    int calculateSegments(const glm::vec2 &p1, const glm::vec2 &p2)
    {
        return (int)(glm::distance(p1, p2) / 0.01f);
    }

    void Renderer::curve(const glm::vec2 &start, const glm::vec2 &end,
                         const glm::vec3 &color, const int id)
    {
        const int segments = (int)(calculateSegments(start, end));
        double dx = end.x - start.x;
        double offsetX = dx * 0.5;
        glm::vec2 cp2 = {end.x - offsetX, end.y};
        glm::vec2 cp1 = {start.x + offsetX, start.y};
        std::vector<Gl::Vertex> verticies(4);
        auto prev = start;
        for (int i = 1; i <= segments; i++)
        {
            glm::vec2 p =
                bernstine(start, cp1, cp2, end, (float)i / (float)segments);
            createCurveVertices(prev, p, color, id);
            prev = p;
        }
    }

    void Renderer::circle(const glm::vec2 &center, const float radius,
                          const glm::vec3 &color, const int id)
    {
        glm::vec2 pos = {center.x - radius, center.y + radius};
        glm::vec2 size = {radius * 2, radius * 2};

        std::vector<Gl::Vertex> vertices(4);

        vertices[0].position = {pos.x, pos.y, 0.0f};
        vertices[1].position = {pos.x, pos.y - size.y, 0.0f};
        vertices[2].position = {pos.x + size.x, pos.y - size.y, 0.0f};
        vertices[3].position = {pos.x + size.x, pos.y, 0.0f};

        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[3].texCoord = {1.0f, 1.0f};
        vertices[2].texCoord = {1.0f, 0.0f};

        for (auto &vertex : vertices)
        {
            vertex.id = id;
            vertex.color = color;
        }

        addVertices(PrimitiveType::circle, vertices);
    }

    void Renderer::addVertices(PrimitiveType type,
                               const std::vector<Gl::Vertex> &vertices)
    {
        auto &primitive_vertices = m_vertices[type];
        auto max_render_count = m_maxRenderCount[type];

        if (primitive_vertices.size() >= (max_render_count - 1) * 4)
        {
            flush(type);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                                  vertices.end());
    }

    void Renderer::flush(PrimitiveType type)
    {
        auto &vertices = m_vertices[type];
        auto &vao = m_vaos[type];
        auto &shader = m_shaders[type];

        vao->bind();
        shader->bind();
        shader->setUniformMat4("u_mvp", m_camera->getTransform());

        auto selId = Simulator::ComponentsManager::compIdToRid(ApplicationState::getSelectedId());
        auto hoveredId = ApplicationState::hoveredId;

        shader->setUniform1i("u_SelectedObjId",
                             type == PrimitiveType::circle ? hoveredId : selId);

        vao->setVertices(vertices);
        GL_CHECK(glDrawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6,
                                GL_UNSIGNED_INT, nullptr));
        vertices.clear();
        vao->unbind();
        shader->unbind();
    }

    void Renderer::begin(std::shared_ptr<Camera> camera)
    {
        m_camera = camera;
        m_currentSubId = m_currentId;
    }

    void Renderer::end()
    {
        for (auto primitive : m_AvailablePrimitives)
        {
            flush(primitive);
        }
    }

    int Renderer::getId() { return m_currentId++; }

    int Renderer::getSubId() { return m_currentSubId++; }
} // namespace Bess
