#include "renderer/renderer.h"
#include "camera.h"
#include "fwd.hpp"
#include "geometric.hpp"
#include "glm.hpp"
#include "renderer/gl/gl_wrapper.h"
#include "renderer/gl/primitive_type.h"
#include "renderer/gl/vertex.h"
#include "ui/ui_main/ui_main.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <ext/matrix_transform.hpp>
#include <iostream>
#include <ostream>

using namespace Bess::Renderer2D;

namespace Bess {

    std::vector<PrimitiveType> Renderer::m_AvailablePrimitives;

    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>> Renderer::m_shaders;
    std::unique_ptr<Gl::Shader> Renderer::m_quadShadowShader;
    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> Renderer::m_vaos;

    std::shared_ptr<Camera> Renderer::m_camera;

    std::vector<glm::vec4> Renderer::m_StandardQuadVertices;
    std::vector<glm::vec4> Renderer::m_StandardTriVertices;
    std::unordered_map<PrimitiveType, size_t> Renderer::m_MaxRenderLimit;

    RenderData Renderer::m_RenderData;

    std::unique_ptr<Gl::Shader> Renderer::m_GridShader;
    std::unique_ptr<Gl::Vao> Renderer::m_GridVao;

    std::unique_ptr<Font> Renderer::m_Font;

    void Renderer::init() {
        {
            m_GridShader = std::make_unique<Gl::Shader>("assets/shaders/grid_vert.glsl", "assets/shaders/grid_frag.glsl");
            std::vector<Gl::VaoAttribAttachment> attachments;
            attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::GridVertex, position)));
            attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::GridVertex, texCoord)));
            attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::GridVertex, id)));
            attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::GridVertex, color)));
            attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::float_t, offsetof(Gl::GridVertex, ar)));

            m_GridVao = std::make_unique<Gl::Vao>(8, 12, attachments, sizeof(Gl::GridVertex));
        }

        m_AvailablePrimitives = {PrimitiveType::curve, PrimitiveType::quad,
                                 PrimitiveType::circle, PrimitiveType::font, PrimitiveType::triangle};
        m_MaxRenderLimit[PrimitiveType::quad] = 2000;
        m_MaxRenderLimit[PrimitiveType::curve] = 2000;
        m_MaxRenderLimit[PrimitiveType::circle] = 2000;
        m_MaxRenderLimit[PrimitiveType::font] = 2000;
        m_MaxRenderLimit[PrimitiveType::triangle] = 2000;

        std::string vertexShader, fragmentShader;

        for (auto primitive : m_AvailablePrimitives) {
            switch (primitive) {
            case PrimitiveType::quad:
                vertexShader = "assets/shaders/quad_vert.glsl";
                fragmentShader = "assets/shaders/quad_frag.glsl";
                m_quadShadowShader = std::make_unique<Gl::Shader>("assets/shaders/quad_vert.glsl", "assets/shaders/shadow_frag.glsl");
                break;
            case PrimitiveType::curve:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/curve_frag.glsl";
                break;
            case PrimitiveType::circle:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/circle_frag.glsl";
                break;
            case PrimitiveType::font:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/font_frag.glsl";
                break;
            case PrimitiveType::triangle:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/triangle_frag.glsl";
                break;
            }

            if (vertexShader.empty() || fragmentShader.empty()) {
                std::cerr << "[-] Primitive " << (int)primitive
                          << "is not available" << std::endl;
                return;
            }

            auto max_render_count = m_MaxRenderLimit[primitive];

            m_shaders[primitive] =
                std::make_unique<Gl::Shader>(vertexShader, fragmentShader);

            if (primitive == PrimitiveType::quad) {
                std::vector<Gl::VaoAttribAttachment> attachments;
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::QuadVertex, position)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, color)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::QuadVertex, texCoord)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, borderRadius)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::QuadVertex, size)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::QuadVertex, id)));
                m_vaos[primitive] = std::make_unique<Gl::Vao>(max_render_count * 4, max_render_count * 6, attachments, sizeof(Gl::QuadVertex));
            } else {
                std::vector<Gl::VaoAttribAttachment> attachments;
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::Vertex, position)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::Vertex, color)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::Vertex, texCoord)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::Vertex, id)));
                bool triangle = primitive == PrimitiveType::triangle;
                m_vaos[primitive] = std::make_unique<Gl::Vao>(max_render_count * 4, max_render_count * 6, attachments, sizeof(Gl::Vertex), triangle);
            }

            vertexShader.clear();
            fragmentShader.clear();
        }

        m_StandardQuadVertices = {
            {-0.5f, 0.5f, 0.f, 1.f},
            {-0.5f, -0.5f, 0.f, 1.f},
            {0.5f, -0.5f, 0.f, 1.f},
            {0.5f, 0.5f, 0.f, 1.f},
        };

        m_StandardTriVertices = {
            {-0.5f, 0.5f, 0.f, 1.f},
            {0.f, -0.5f, 0.f, 1.f},
            {0.5f, 0.5f, 0.f, 1.f}};

        m_Font = std::make_unique<Font>("assets/fonts/Roboto/Roboto-Regular.ttf");
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id,
                        const glm::vec4 &borderRadius, const glm::vec4 &borderColor,
                        float borderSize) {
        Renderer::quad(pos, size, color, id, 0.f, borderRadius, borderColor,
                       borderSize);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id,
                        const glm::vec4 &borderRadius, const glm::vec4 &borderColor,
                        const glm::vec4 &borderSize) {
        Renderer::quad(pos, size, color, id, 0.f, borderRadius, borderColor,
                       borderSize);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id, float angle,
                        const glm::vec4 &borderRadius, const glm::vec4 &borderColor,
                        float borderSize) {
        Renderer::quad(pos, size, color, id, angle, borderRadius, borderColor, glm::vec4(borderSize));
    }

    void Renderer2D::Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &color, int id, float angle, const glm::vec4 &borderRadius, const glm::vec4 &borderColor, const glm::vec4 &borderSize) {

        if (borderSize.x || borderSize.y || borderSize.z || borderSize.w) {
            glm::vec3 borderPos = pos;
            glm::vec2 borderSize_ = size + glm::vec2(borderSize.w + borderSize.y, borderSize.x + borderSize.z);
            auto borderRadius_ = borderRadius + borderSize;
            Renderer::drawQuad(borderPos, borderSize_, borderColor, id, angle, borderRadius_);
        }
        Renderer::drawQuad(pos, size, color, id, angle, borderRadius);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id,
                        const glm::vec4 &borderRadius,
                        bool shadow,
                        const glm::vec4 &borderColor,
                        const glm::vec4 &borderSize) {
        Renderer::quad(pos, size, color, id, 0.f, borderRadius, shadow, borderColor, borderSize);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id,
                        const glm::vec4 &borderRadius,
                        bool shadow,
                        const glm::vec4 &borderColor,
                        float borderSize) {
        Renderer::quad(pos, size, color, id, 0.f, borderRadius, shadow, borderColor, glm::vec4(borderSize));
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id, float angle,
                        const glm::vec4 &borderRadius,
                        bool shadow,
                        const glm::vec4 &borderColor,
                        const glm::vec4 &borderSize) {

        if (shadow) {
            std::vector<Gl::QuadVertex> vertices(4);

            auto transform = glm::translate(glm::mat4(1.0f), pos + glm::vec3(6.f, -4.f, 0.f));
            transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
            transform = glm::scale(transform, {size.x, size.y, 1.f});

            for (int i = 0; i < 4; i++) {
                auto &vertex = vertices[i];
                vertex.position = transform * m_StandardQuadVertices[i];
                vertex.id = id;
                vertex.color = color;
                vertex.borderRadius = borderRadius;
                vertex.size = size;
            }

            vertices[0].texCoord = {0.0f, 1.0f};
            vertices[1].texCoord = {0.0f, 0.0f};
            vertices[2].texCoord = {1.0f, 0.0f};
            vertices[3].texCoord = {1.0f, 1.0f};
            m_RenderData.quadShadowVertices.insert(m_RenderData.quadShadowVertices.end(), vertices.begin(), vertices.end());
        }
        Renderer::quad(pos, size, color, id, angle, borderRadius, borderColor, borderSize);
    }

    void Renderer2D::Renderer::drawQuad(const glm::vec3 &pos, const glm::vec2 &size, const glm::vec4 &color, int id, float angle, const glm::vec4 &borderRadius) {
        std::vector<Gl::QuadVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
            vertex.borderRadius = borderRadius;
            vertex.size = size;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addQuadVertices(vertices);
    }

    void Renderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const glm::vec4 &color) {
        std::vector<Gl::GridVertex> vertices(4);

        auto size_ = size;
        // size_.x = std::max(size.y, size.x);
        // size_.y = std::max(size.y, size.x);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::scale(transform, {size_.x, size_.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.ar = size_.x / size_.y;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        m_GridShader->bind();
        m_GridVao->bind();

        m_GridShader->setUniformMat4("u_mvp", m_camera->getOrtho());
        m_GridShader->setUniform1f("u_zoom", m_camera->getZoom());
        m_GridShader->setUniformVec2("u_cameraOffset", -m_camera->getPos());
        m_GridVao->setVertices(vertices.data(), vertices.size());

        Gl::Api::drawElements(GL_TRIANGLES, 6);

        m_GridShader->unbind();
        m_GridVao->unbind();
    }

    glm::vec2 bernstine(const glm::vec2 &p0, const glm::vec2 &p1,
                        const glm::vec2 &p2, const glm::vec2 &p3, const float t) {
        auto t_ = 1 - t;

        glm::vec2 B0 = (float)std::pow(t_, 3) * p0;
        glm::vec2 B1 = (float)(3 * t * std::pow(t_, 2)) * p1;
        glm::vec2 B2 = (float)(3 * t * t * std::pow(t_, 1)) * p2;
        glm::vec2 B3 = (float)(t * t * t) * p3;

        return B0 + B1 + B2 + B3;
    }

    glm::vec2 bernstineQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const float t) {
        float t_ = 1.0f - t;
        glm::vec2 point = (t_ * t_) * p0 + 2.0f * (t_ * t) * p1 + (t * t) * p2;
        return point;
    }

    void Renderer::createCurveVertices(const glm::vec3 &start_, const glm::vec3 &end_, const glm::vec4 &color, const int id, float weight) {
        auto end = end_;
        auto start = start_;

        float dy = end.y - start.y;
        if (std::abs(dy) < 0.0001f) {
            end.y = start.y;
        }
        glm::vec2 direction = glm::normalize(end - start);
        float length = glm::distance(start, end);
        glm::vec2 pos = (start + end) * 0.5f;
        float angle = glm::atan(direction.y, direction.x);
        glm::vec2 size = {length, weight};

        auto transform = glm::translate(glm::mat4(1.0f), {pos, start.z});
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {size, 1.f});

        std::vector<Gl::Vertex> vertices(4);
        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addCurveVertices(vertices);
    }

    int Renderer::calculateSegments(const glm::vec2 &p1, const glm::vec2 &p2) {
        // return (int)(glm::distance(p1 / UI::UIMain::state.viewportSize, p2 / UI::UIMain::state.viewportSize) / 0.0001f);
        float distance = glm::distance(p1 / UI::UIMain::state.viewportSize, p2 / UI::UIMain::state.viewportSize);
        float segments = distance * std::pow(10, 2);
        // segments = 100;
        return std::max(1, (int)segments);
    }

    void Renderer::curve(const glm::vec3 &start, const glm::vec3 &end, float weight, const glm::vec4 &color, const int id) {
        double dx = end.x - start.x;
        double offsetX = dx * 0.5;

        /*if (dx < 0.f) offsetX *= -1;
        if (offsetX < 100.f) offsetX = 100.0;*/

        glm::vec2 cp2 = {end.x - offsetX, end.y};
        glm::vec2 cp1 = {start.x + offsetX, start.y};
        cubicBezier(start, end, cp1, cp2, weight, color, id);
    }

    void Renderer::quadraticBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint, float weight,
                                   const glm::vec4 &color, const int id, bool pathMode) {
        int segments = calculateSegments(start, end);

        auto prev = start;
        for (int i = 1; i <= segments; i++) {
            glm::vec2 bP = bernstineQuadBezier(start, controlPoint, end, (float)i / (float)segments);
            glm::vec3 p = {bP.x, bP.y, start.z};
            if (pathMode)
                line(prev, p, weight, color, id);
            else
                createCurveVertices(prev, p, color, id, weight);
            prev = p;
        }
    }

    void Renderer2D::Renderer::cubicBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &cp1, const glm::vec2 &cp2, float weight, const glm::vec4 &color, const int id) {
        const int segments = (int)(calculateSegments(start, end));

        auto prev = start;
        for (int i = 1; i <= segments; i++) {
            glm::vec2 bP = bernstine(start, cp1, cp2, end, (float)i / (float)segments);
            glm::vec3 p = {bP.x, bP.y, start.z};
            createCurveVertices(prev, p, color, id, weight);
            prev = p;
        }
    }

    void Renderer::circle(const glm::vec3 &center, const float radius,
                          const glm::vec4 &color, const int id) {
        glm::vec2 size = {radius * 2, radius * 2};

        std::vector<Gl::Vertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), center);
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addCircleVertices(vertices);
    }

    void Renderer::text(const std::string &text, const glm::vec3 &pos, const size_t size, const glm::vec4 &color, const int id) {
        auto &shader = m_shaders[PrimitiveType::font];
        auto &vao = m_vaos[PrimitiveType::font];

        vao->bind();
        shader->bind();

        shader->setUniformVec4("textColor", color);
        shader->setUniformMat4("u_mvp", m_camera->getTransform());

        // auto selId = Simulator::ComponentsManager::compIdToRid(Pages::MainPageState::getInstance()->getSelectedId());
        shader->setUniform1i("u_SelectedObjId", -1);

        float scale = Font::getScale(size), x = pos.x, y = pos.y;

        for (auto &c : text) {
            auto &ch = m_Font->getCharacter(c);

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            std::vector<Gl::Vertex> vertices(4);

            auto transform = glm::translate(glm::mat4(1.0f), {xpos + w / 2, ypos + h / 2, pos.z});
            transform = glm::scale(transform, {w, h, 1.f});

            for (int i = 0; i < 4; i++) {
                auto &vertex = vertices[i];
                vertex.position = transform * m_StandardQuadVertices[i];
                vertex.id = id;
                vertex.color = color;
            }

            vertices[0].texCoord = {0.0f, 0.0f};
            vertices[1].texCoord = {0.0f, 1.0f};
            vertices[2].texCoord = {1.0f, 1.0f};
            vertices[3].texCoord = {1.0f, 0.0f};

            ch.Texture->bind();

            m_vaos[PrimitiveType::font]->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            x += (ch.Advance >> 6) * scale;
        }
    }

    void Renderer2D::Renderer::line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id) {
        glm::vec2 direction = end - start;
        float length = glm::length(direction);

        glm::vec2 pos = (start + end) * 0.5f;

        float angle = glm::atan(direction.y, direction.x);

        drawQuad(glm::vec3(pos, start.z), {length, size}, color, id, angle);
    }

    void Renderer2D::Renderer::drawPath(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed) {
        if (points.empty())
            return;
        auto newPoints = points;
        auto prev = newPoints[0];

        if (closed) {
            newPoints.emplace_back(prev);
        }

        for (int i = 1; i < (int)newPoints.size(); i++) {
            auto p1 = newPoints[i], p1_ = newPoints[i];
            if (i + 1 < newPoints.size()) {
                auto curve_ = generateQuadBezierPoints(newPoints[i - 1], newPoints[i], newPoints[i + 1], 8.f);
                Renderer2D::Renderer::quadraticBezier(glm::vec3(curve_.startPoint, prev.z), glm::vec3(curve_.endPoint, p1.z), curve_.controlPoint, weight, color, id, true);
                p1 = glm::vec3(curve_.startPoint, prev.z), p1_ = glm::vec3(curve_.endPoint, newPoints[i + 1].z);
            }
            Renderer2D::Renderer::line(prev, p1, 2.f, color, -1);
            prev = p1_;
        }
    }

    void Renderer2D::Renderer::triangle(const std::vector<glm::vec3> &points, const glm::vec4 &color, const int id) {
        std::vector<Gl::Vertex> vertices(3);

        for (int i = 0; i < vertices.size(); i++) {
            auto transform = glm::translate(glm::mat4(1.0f), points[i]);
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardTriVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 0.0f};
        vertices[1].texCoord = {0.0f, 0.5f};
        vertices[2].texCoord = {1.0f, 1.0f};

        addTriangleVertices(vertices);
    }

    void Renderer::addTriangleVertices(const std::vector<Gl::Vertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::circle];

        auto &primitive_vertices = m_RenderData.triangleVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::triangle);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

    void Renderer::addCircleVertices(const std::vector<Gl::Vertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::circle];

        auto &primitive_vertices = m_RenderData.circleVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::circle);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                                  vertices.end());
    }

    void Renderer::addQuadVertices(const std::vector<Gl::QuadVertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::quad];

        auto &primitive_vertices = m_RenderData.quadVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::quad);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

    void Renderer::addCurveVertices(const std::vector<Gl::Vertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::curve];

        auto &primitive_vertices = m_RenderData.curveVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::curve);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

    void Renderer::flush(PrimitiveType type) {
        auto &vao = m_vaos[type];
        // auto selId = Simulator::ComponentsManager::compIdToRid(Pages::MainPageState::getInstance()->getSelectedId());

        if (type == PrimitiveType::quad) {
            vao->bind();
            auto &shader = m_quadShadowShader;
            shader->bind();
            shader->setUniformMat4("u_mvp", m_camera->getTransform());
            shader->setUniform1i("u_SelectedObjId", -1);
            shader->setUniform1f("u_zoom", m_camera->getZoom());

            auto &vertices = m_RenderData.quadShadowVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            vertices.clear();

            vao->unbind();
            shader->unbind();
        }

        auto &shader = m_shaders[type];

        vao->bind();
        shader->bind();

        shader->setUniformMat4("u_mvp", m_camera->getTransform());
        shader->setUniform1i("u_SelectedObjId", -1);

        switch (type) {
        case PrimitiveType::quad: {
            shader->setUniform1f("u_zoom", m_camera->getZoom());
            auto &vertices = m_RenderData.quadVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            vertices.clear();
        } break;
        case PrimitiveType::curve: {
            shader->setUniform1f("u_zoom", m_camera->getZoom());
            auto &vertices = m_RenderData.curveVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            vertices.clear();
        } break;
        case PrimitiveType::circle: {
            auto &vertices = m_RenderData.circleVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            vertices.clear();
        } break;
        case PrimitiveType::triangle: {
            auto &vertices = m_RenderData.triangleVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, vertices.size());
            vertices.clear();
        } break;
        }

        vao->unbind();
        shader->unbind();
    }

    void Renderer::begin(std::shared_ptr<Camera> camera) {
        m_camera = camera;
        Gl::Api::clearStats();
    }

    QuadBezierCurvePoints Renderer::generateQuadBezierPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius) {
        glm::vec2 dir1 = glm::normalize(joinPoint - prevPoint);
        glm::vec2 dir2 = glm::normalize(nextPoint - joinPoint);
        glm::vec2 bisector = glm::normalize(dir1 + dir2);
        float offset = glm::dot(bisector, dir1);
        glm::vec2 controlPoint = joinPoint + bisector * offset;
        glm::vec2 startPoint = joinPoint - dir1 * curveRadius;
        glm::vec2 endPoint = joinPoint + dir2 * curveRadius;
        return {startPoint, controlPoint, endPoint};
    }

    void Renderer::end() {
        for (auto primitive : m_AvailablePrimitives) {
            flush(primitive);
        }
    }

    glm::vec2 Renderer2D::Renderer::getCharRenderSize(char ch, float renderSize) {
        auto ch_ = m_Font->getCharacter(ch);
        float scale = m_Font->getScale(renderSize);
        glm::vec2 size = {(ch_.Advance >> 6), ch_.Size.y};
        size = {size.x * scale, size.y * scale};
        return size;
    }
} // namespace Bess
