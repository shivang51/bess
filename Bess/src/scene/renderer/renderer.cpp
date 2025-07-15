#include "scene/renderer/renderer.h"
#include "camera.h"
#include "fwd.hpp"
#include "geometric.hpp"
#include "glm.hpp"
#include "scene/renderer/gl/gl_wrapper.h"
#include "scene/renderer/gl/primitive_type.h"
#include "scene/renderer/gl/vertex.h"
#include "ui/ui_main/ui_main.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <ext/matrix_transform.hpp>
#include <iostream>
#include <ostream>

using namespace Bess::Renderer2D;

namespace Bess {

    #define BESS_RENDERER_DISABLE_RENDERPASS

    static constexpr uint32_t PRIMITIVE_RESTART = 0xFFFFFFFF;
    static constexpr float BEZIER_EPSILON = 0.0001f;

    bool Renderer::m_curveBroken = false;
    std::vector<PrimitiveType> Renderer::m_AvailablePrimitives;

    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>> Renderer::m_shaders;
    std::unique_ptr<Gl::Shader> Renderer::m_quadShadowShader;
    std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> Renderer::m_vaos;

    std::shared_ptr<Camera> Renderer::m_camera;

    std::vector<glm::vec4> Renderer::m_StandardQuadVertices;
    std::vector<glm::vec4> Renderer::m_StandardTriVertices;
    std::unordered_map<PrimitiveType, size_t> Renderer::m_MaxRenderLimit;

    RenderData Renderer::m_RenderData;
    PathContext Renderer::m_pathData;

    std::unique_ptr<Gl::Shader> Renderer::m_GridShader;
    std::unique_ptr<Gl::Shader> Renderer::m_shadowPassShader;
    std::unique_ptr<Gl::Shader> Renderer::m_compositePassShader;
    std::unique_ptr<Gl::Vao> Renderer::m_GridVao;
    std::unique_ptr<Gl::Vao> Renderer::m_renderPassVao;
    std::vector<Gl::Vertex> Renderer::m_curveStripVertices;
    std::vector<GLuint> Renderer::m_curveStripIndices;
    std::vector<Gl::Vertex> Renderer::m_pathStripVertices;
    std::vector<GLuint> Renderer::m_pathStripIndices;

    std::unique_ptr<Font> Renderer::m_Font;
    std::unordered_map<std::string, glm::vec2> Renderer::m_charSizeCache;

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

        #ifndef BESS_RENDERER_DISABLE_RENDERPASS
        {

           m_shadowPassShader = std::make_unique<Gl::Shader>("assets/shaders/shadow_vert.glsl", "assets/shaders/shadow_frag.glsl");
           m_compositePassShader = std::make_unique<Gl::Shader>("assets/shaders/composite_vert.glsl", "assets/shaders/composite_frag.glsl");
           std::vector<Gl::VaoAttribAttachment> attachments;
           attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::GridVertex, position)));
           attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::GridVertex, texCoord)));
           m_renderPassVao = std::make_unique<Gl::Vao>(8, 12, attachments, sizeof(Gl::RenderPassVertex));
        }
        #endif

        m_AvailablePrimitives = {PrimitiveType::curve,  PrimitiveType::line,
                                 PrimitiveType::font, PrimitiveType::triangle, 
                                 PrimitiveType::quad, PrimitiveType::circle,PrimitiveType::path};

        for (auto &prim : m_AvailablePrimitives) {
            m_MaxRenderLimit[prim] = 2000;
        }

        std::string vertexShader, fragmentShader;

        for (auto primitive : m_AvailablePrimitives) {
            switch (primitive) {
            case PrimitiveType::quad:
                vertexShader = "assets/shaders/quad_vert.glsl";
                fragmentShader = "assets/shaders/quad_frag.glsl";
                m_quadShadowShader = std::make_unique<Gl::Shader>("assets/shaders/quad_vert.glsl", "assets/shaders/shadow_frag.glsl");
                break;
            case PrimitiveType::curve:
            case PrimitiveType::path:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/curve_frag.glsl";
                break;
            case PrimitiveType::circle:
                vertexShader = "assets/shaders/circle_vert.glsl";
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
            case PrimitiveType::line:
                vertexShader = "assets/shaders/vert.glsl";
                fragmentShader = "assets/shaders/line_frag.glsl";
                break;
            }

            if (vertexShader.empty() || fragmentShader.empty()) {
                std::cerr << "[-] Primitive " << (int)primitive << "is not available" << std::endl;
                return;
            }

            auto max_render_count = m_MaxRenderLimit[primitive];

            m_shaders[primitive] = std::make_unique<Gl::Shader>(vertexShader, fragmentShader);

            if (primitive == PrimitiveType::quad) {
                std::vector<Gl::VaoAttribAttachment> attachments;
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::QuadVertex, position)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, color)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::QuadVertex, texCoord)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, borderRadius)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, borderSize)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::QuadVertex, borderColor)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::QuadVertex, size)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::QuadVertex, id)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::QuadVertex, isMica)));
                m_vaos[primitive] = std::make_unique<Gl::Vao>(max_render_count * 4, max_render_count * 6, attachments, sizeof(Gl::QuadVertex));
            } else if (primitive == PrimitiveType::circle) {
                std::vector<Gl::VaoAttribAttachment> attachments;
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::CircleVertex, position)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::CircleVertex, color)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::CircleVertex, texCoord)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::float_t, offsetof(Gl::CircleVertex, innerRadius)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::CircleVertex, id)));
                m_vaos[primitive] = std::make_unique<Gl::Vao>(max_render_count * 4, max_render_count * 6, attachments, sizeof(Gl::CircleVertex));
            }
            else {
                std::vector<Gl::VaoAttribAttachment> attachments;
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec3, offsetof(Gl::Vertex, position)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4, offsetof(Gl::Vertex, color)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::vec2, offsetof(Gl::Vertex, texCoord)));
                attachments.emplace_back(Gl::VaoAttribAttachment(Gl::VaoAttribType::int_t, offsetof(Gl::Vertex, id)));
                bool triangle = primitive == PrimitiveType::triangle || primitive == PrimitiveType::curve || primitive == PrimitiveType::path;
                Gl::VaoElementType elType = Gl::VaoElementType::quad;
                if (primitive == PrimitiveType::triangle) {
                    elType = Gl::VaoElementType::triangle;
                } else if (primitive == PrimitiveType::curve || primitive == PrimitiveType::path) {
                    elType = Gl::VaoElementType::triangleStrip;
                }
                size_t maxVertices = max_render_count * (triangle ? 3 : 4);
                size_t maxIndices = max_render_count * (triangle ? 3 : 6);
                m_vaos[primitive] = std::make_unique<Gl::Vao>(maxVertices, maxIndices, attachments, sizeof(Gl::Vertex), elType);
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
                         QuadRenderProperties properties) {

        std::vector<glm::vec2> texCoords = {
            {0.0f, 1.0f},
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f}
        };

        std::vector<Gl::QuadVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::rotate(transform, properties.angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
            vertex.borderRadius = properties.borderRadius;
            vertex.size = size;
            vertex.isMica = properties.isMica ? 1 : 0;
            vertex.borderColor = properties.borderColor;
            vertex.borderSize = properties.borderSize;
            vertex.texCoord = texCoords[i];
        }

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

        auto camPos = m_camera->getPos();
        auto viewportPos = UI::UIMain::state.viewportPos / m_camera->getZoom();
        m_GridShader->setUniformVec2("u_cameraOffset", {-viewportPos.x - camPos.x, viewportPos.y + m_camera->getSpan().y + camPos.y});

        m_GridVao->setVertices(vertices.data(), vertices.size());

        Gl::Api::drawElements(GL_TRIANGLES, 6);

        m_GridShader->unbind();
        m_GridVao->unbind();
    }

    void Renderer::curve(const glm::vec3 &start, const glm::vec3 &end, float weight, const glm::vec4 &color, const int id) {
        double dx = end.x - start.x;
        double offsetX = dx * 0.5;

        glm::vec2 cp2 = {end.x - offsetX, end.y};
        glm::vec2 cp1 = {start.x + offsetX, start.y};
        cubicBezier(start, end, cp1, cp2, weight, color, id);
    }

    void Renderer::quadraticBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &controlPoint, float weight,
                                   const glm::vec4 &color, const int id, bool breakCurve) {
        auto points = generateQuadBezierPoints(start, controlPoint, end);

        auto prev = start;
        for (size_t i = 1; i < points.size(); i++) {
            glm::vec3& p = points[i];
            addCurveSegmentStrip(prev, p, color, id, weight, i == 1);
            prev = p;
        }

        m_curveBroken = breakCurve;
        if (breakCurve)
            m_curveStripIndices.emplace_back(PRIMITIVE_RESTART);
    }

    void Renderer::cubicBezier(const glm::vec3 &start, const glm::vec3 &end, const glm::vec2 &cp1, const glm::vec2 &cp2, float weight,
                               const glm::vec4 &color, const int id, bool breakCurve) {

        auto points = generateCubicBezierPoints(start, cp1, cp2, end);

        auto prev = start;
        for (size_t i = 1; i < points.size(); i++) {
            glm::vec3& p = points[i];
            addCurveSegmentStrip(prev, p, color, id, weight, i == 1);
            prev = p;
        }

        m_curveBroken = breakCurve;
        if (breakCurve)
            m_curveStripIndices.emplace_back(PRIMITIVE_RESTART);
    }

    void Renderer::circle(const glm::vec3 &center, const float radius,
                          const glm::vec4 &color, const int id, float innerRadius) {
        glm::vec2 size = {radius * 2, radius * 2};

        std::vector<glm::vec2> texCoords = {
            {0.0f, 1.0f},
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f}
        };

        std::vector<Gl::CircleVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), center);
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
            vertex.innerRadius = innerRadius / radius; // Normalize inner radius to the outer radius
            vertex.texCoord = texCoords[i];
        }

        addCircleVertices(vertices);
    }

    void Renderer::text(const std::string &text, const glm::vec3 &pos, const size_t size,
                        const glm::vec4 &color, const int id, float angle) {
        auto &shader = m_shaders[PrimitiveType::font];
        auto &vao = m_vaos[PrimitiveType::font];

        vao->bind();
        shader->bind();

        shader->setUniformVec4("textColor", color);
        shader->setUniformMat4("u_mvp", m_camera->getTransform());
        shader->setUniform1i("u_SelectedObjId", -1);

        float scale = Font::getScale(size);

        // First, compute the bounding box of the text.
        float totalWidth = 0.0f;
        float maxHeight = 0.0f;
        for (const char &c : text) {
            auto &ch = m_Font->getCharacter(c);
            totalWidth += (ch.Advance >> 6) * scale;
            float h = ch.Size.y * scale;
            if (h > maxHeight) {
                maxHeight = h;
            }
        }
        // Assume that 'pos' is the top-left corner of the text block.
        // Compute the center of the text block.
        glm::vec2 pivot = {pos.x + totalWidth / 2.0f, pos.y - maxHeight / 2.0f};

        // Create a global transform that rotates the entire text block about the center pivot.
        glm::mat4 globalTransform(1.0f);
        // Translate so that the pivot is at the origin.
        globalTransform = glm::translate(globalTransform, {pivot.x, pivot.y, pos.z});
        // Rotate around the Z axis.
        globalTransform = glm::rotate(globalTransform, angle, {0.f, 0.f, 1.f});
        // Translate back.
        globalTransform = glm::translate(globalTransform, {-pivot.x, -pivot.y, -pos.z});

        // Now layout each glyph as if the text were unrotated (horizontally laid out).
        float x = pos.x;
        float y = pos.y;
        for (const char &c : text) {
            auto &ch = m_Font->getCharacter(c);

            // Compute the position of the glyph quad.
            float xpos = x + ch.Bearing.x * scale;
            // Note: We assume a coordinate system where y increases upward;
            // adjust this if your system is different.
            float ypos = y + (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            // Build the local transform for this glyph.
            glm::mat4 localTransform(1.0f);
            localTransform = glm::translate(localTransform, {xpos, ypos, pos.z});
            // Translate to center the quad before scaling.
            localTransform = glm::translate(localTransform, {w / 2.0f, -h / 2.0f, 0.f});
            localTransform = glm::scale(localTransform, {w, h, 1.f});

            // Apply the global transform to rotate the whole text block.
            glm::mat4 transform = globalTransform * localTransform;

            std::vector<Gl::Vertex> vertices(4);
            for (int i = 0; i < 4; i++) {
                auto &vertex = vertices[i];
                vertex.position = transform * m_StandardQuadVertices[i];
                vertex.id = id;
                vertex.color = color;
            }

            // Set texture coordinates.
            vertices[0].texCoord = {0.0f, 1.0f};
            vertices[1].texCoord = {0.0f, 0.0f};
            vertices[2].texCoord = {1.0f, 0.0f};
            vertices[3].texCoord = {1.0f, 1.0f};

            ch.Texture->bind();
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);

            // Advance horizontally as usual.
            x += (ch.Advance >> 6) * scale;
        }
    }

    void Renderer2D::Renderer::line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id) {
        glm::vec2 direction = end - start;
        float length = glm::length(direction);

        glm::vec2 pos = (start + end) * 0.5f;

        float angle = glm::atan(direction.y, direction.x);

        auto transform = glm::translate(glm::mat4(1.0f), {pos, start.z});
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, {length, size, 1.f});

        std::vector<Gl::Vertex> vertices(4);
        for (int i = 0; i < vertices.size(); i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        addLineVertices(vertices);
    }

    void Renderer2D::Renderer::drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const std::vector<int> &ids, bool closed) {
        if (points.empty())
            return;

        auto prev = points[0];

        for (int i = 1; i < (int)points.size(); i++) {
            Renderer2D::Renderer::line(prev, points[i], weight, color, ids[i - 1]);
            prev = points[i];
        }

        if (closed) {
            Renderer2D::Renderer::line(prev, points[0], weight, color, ids[0]);
        }
    }

    void Renderer2D::Renderer::drawLines(const std::vector<glm::vec3> &points, float weight, const glm::vec4 &color, const int id, bool closed) {
        if (points.empty())
            return;

        auto prev = points[0];

        for (int i = 1; i < (int)points.size(); i++) {
            auto p1 = points[i], p1_ = points[i];
            // if (i + 1 < points.size()) {
            //     auto curve_ = generateQuadBezierPoints(points[i - 1], points[i], points[i + 1], 4.f);
            //     Renderer2D::Renderer::quadraticBezier(glm::vec3(curve_.startPoint, prev.z), glm::vec3(curve_.endPoint, p1.z), curve_.controlPoint, weight, color, id);
            //     p1 = glm::vec3(curve_.startPoint, prev.z), p1_ = glm::vec3(curve_.endPoint, points[i + 1].z);
            // }
            Renderer2D::Renderer::line(prev, p1, weight, color, id);
            prev = p1_;
        }

        if (closed) {
            Renderer2D::Renderer::line(prev, points[0], weight, color, id);
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

    void Renderer::addCircleVertices(const std::vector<Gl::CircleVertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::circle];

        auto &primitive_vertices = m_RenderData.circleVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::circle);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                                  vertices.end());
    }

    void Renderer::addLineVertices(const std::vector<Gl::Vertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::line];

        auto &primitive_vertices = m_RenderData.lineVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::line);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

    void Renderer::addQuadVertices(const std::vector<Gl::QuadVertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[PrimitiveType::quad];

        auto &primitive_vertices = m_RenderData.quadVertices;

        if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
            flush(PrimitiveType::quad);
        }

        primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

    void Renderer::addCurveSegmentStrip(
        const glm::vec3 &prev_,
        const glm::vec3 &curr_,
        const glm::vec4 &color,
        int id,
        float weight,
        bool firstSegment) {
        glm::vec2 prev = {prev_.x, prev_.y};
        glm::vec2 curr = {curr_.x, curr_.y};

        glm::vec2 dir = glm::normalize(curr - prev);
        glm::vec2 normal = {-dir.y, dir.x};

        float halfW = weight * 0.5f;
        glm::vec2 offset = normal * halfW;

        glm::vec3 vPrevOut = {prev + offset, prev_.z};
        glm::vec3 vPrevIn = {prev - offset, prev_.z};
        glm::vec3 vCurrOut = {curr + offset, curr_.z};
        glm::vec3 vCurrIn = {curr - offset, curr_.z};

        auto makeV = [&](const glm::vec3 &p, const glm::vec2 &uv) {
            Gl::Vertex v;
            v.position = p;
            v.color = color;
            v.id = id;
            v.texCoord = uv;
            return v;
        };

        if (firstSegment) {
            uint32_t base = (uint32_t)m_curveStripVertices.size();
            m_curveStripVertices.emplace_back(makeV(vPrevOut, {0.0f, 1.0f})); // base + 0
            m_curveStripVertices.emplace_back(makeV(vPrevIn, {0.0f, 0.0f}));  // base + 1
            m_curveStripIndices.emplace_back(base + 0);
            m_curveStripIndices.emplace_back(base + 1);
        }

        uint32_t base = (uint32_t)m_curveStripVertices.size();
        m_curveStripVertices.emplace_back(makeV(vCurrOut, {1.0f, 1.0f})); // base + 0
        m_curveStripVertices.emplace_back(makeV(vCurrIn, {1.0f, 0.0f}));  // base + 1
        m_curveStripIndices.emplace_back(base + 0);
        m_curveStripIndices.emplace_back(base + 1);
    }

// disabled composite pass logic
#ifndef BESS_RENDERER_DISABLE_RENDERPASS
    std::vector<Gl::RenderPassVertex> Renderer::getRenderPassVertices(float width, float height) {
        std::vector<Gl::RenderPassVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), {0, 0, 0});
        transform = glm::scale(transform, {width, height, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        return vertices;
    }

    void Renderer::doShadowRenderPass(float width, float height) {
        m_shadowPassShader->bind();
        m_renderPassVao->bind();

        m_shadowPassShader->setUniformMat4("u_mvp", m_camera->getTransform());

        auto vertices = getRenderPassVertices(width, height);
        m_renderPassVao->setVertices(vertices.data(), vertices.size());
        Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
        m_renderPassVao->unbind();
        m_shadowPassShader->unbind();
    }

    void Renderer::doCompositeRenderPass(float width, float height) {
        m_compositePassShader->bind();
        m_renderPassVao->bind();

        m_compositePassShader->setUniformMat4("u_mvp", m_camera->getTransform());
        m_compositePassShader->setUniform1i("uBaseColTex", 0);
        m_compositePassShader->setUniform1i("uShadowTex", 1);

        auto vertices = getRenderPassVertices(width, height);
        m_renderPassVao->setVertices(vertices.data(), vertices.size());
        Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
        m_renderPassVao->unbind();
        m_compositePassShader->unbind();
    }
#endif

    void Renderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color) {
        m_pathData.ended = false;
        m_pathData.currentPos = startPos;
        m_pathData.points.emplace_back(startPos);
        m_pathData.color = color;
        m_pathData.weight = weight;
    }

    void Renderer::endPathMode(bool closePath) {
        auto vertices = generateStrokeGeometry(m_pathData.points, m_pathData.weight, m_pathData.color, -1, 4.f, closePath);
        size_t idx = m_pathStripVertices.size();
        for (auto &v : vertices) {
            m_pathStripVertices.emplace_back(v);
            m_pathStripIndices.emplace_back(idx++);
        }
        m_pathData = {};
        m_pathStripIndices.emplace_back(PRIMITIVE_RESTART);
    }

    void Renderer::pathLineTo(const glm::vec3 &pos, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        m_pathData.points.emplace_back(pos);
        m_pathData.setCurrentPos(pos);
    }

    void Renderer2D::Renderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        auto points = generateCubicBezierPoints(m_pathData.currentPos, controlPoint1, controlPoint2, end);
        m_pathData.points.insert(m_pathData.points.end(), points.begin(), points.end());
        m_pathData.setCurrentPos(end);
    }

    void Renderer2D::Renderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        auto points = generateQuadBezierPoints(m_pathData.currentPos, controlPoint, end);
        m_pathData.points.insert(m_pathData.points.end(), points.begin(), points.end());
        m_pathData.setCurrentPos(end);
    }

    void Renderer::begin(std::shared_ptr<Camera> camera) {
        m_camera = camera;
        Gl::Api::clearStats();
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    void Renderer::end() {
        for (auto primitive : m_AvailablePrimitives) {
            flush(primitive);
        }
    }

    void Renderer::flush(PrimitiveType type) {
        auto &vao = m_vaos[type];

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
            vao->setVerticesAndIndices(m_curveStripVertices.data(), m_curveStripVertices.size(), m_curveStripIndices.data(), m_curveStripIndices.size());
            GL_CHECK(glEnable(GL_PRIMITIVE_RESTART));
            GL_CHECK(glPrimitiveRestartIndex(PRIMITIVE_RESTART));
            Gl::Api::drawElements(GL_TRIANGLE_STRIP, (GLsizei)(m_curveStripIndices.size()));
            m_curveStripVertices.clear();
            m_curveStripIndices.clear();
        } break;
        case PrimitiveType::path: {
            shader->setUniform1f("u_zoom", m_camera->getZoom());
            vao->setVerticesAndIndices(m_pathStripVertices.data(), m_pathStripVertices.size(), m_pathStripIndices.data(), m_pathStripIndices.size());
            GL_CHECK(glEnable(GL_PRIMITIVE_RESTART));
            GL_CHECK(glPrimitiveRestartIndex(PRIMITIVE_RESTART));
            Gl::Api::drawElements(GL_TRIANGLE_STRIP, (GLsizei)(m_pathStripIndices.size()));
            m_pathStripVertices.clear();
            m_pathStripIndices.clear();
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
        case PrimitiveType::line: {
            auto &vertices = m_RenderData.lineVertices;
            vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, (GLsizei)(vertices.size() / 4) * 6);
            vertices.clear();
        } break;
        }

        vao->unbind();
        shader->unbind();
    }


    glm::vec2 Renderer2D::Renderer::getCharRenderSize(char ch, float renderSize) {
        std::string key = std::to_string(renderSize) + ch;
        if (m_charSizeCache.find(key) != m_charSizeCache.end()) {
            return m_charSizeCache.at(key);
        }
        auto ch_ = m_Font->getCharacter(ch);
        float scale = m_Font->getScale(renderSize);
        glm::vec2 size = {(ch_.Advance >> 6), ch_.Size.y};
        size = {size.x * scale, size.y * scale};
        m_charSizeCache[key] = size;
        return size;
    }

    glm::vec2 Renderer2D::Renderer::getStringRenderSize(const std::string &str, float renderSize) {
        float xSize = 0;
        float ySize = 0;
        for (auto &ch : str) {
            auto s = getCharRenderSize(ch, renderSize);
            xSize += s.x;
            ySize = std::max(xSize, s.y);
        }
        return {xSize, ySize};
    }

    glm::vec2 Renderer::nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                        const glm::vec2 &p2, const glm::vec2 &p3, const float t) {
        auto t_ = 1 - t;

        glm::vec2 B0 = (float)std::pow(t_, 3) * p0;
        glm::vec2 B1 = (float)(3 * t * std::pow(t_, 2)) * p1;
        glm::vec2 B2 = (float)(3 * t * t * std::pow(t_, 1)) * p2;
        glm::vec2 B3 = (float)(t * t * t) * p3;

        return B0 + B1 + B2 + B3;
    }

    std::vector<glm::vec3> Renderer2D::Renderer::generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end) {
        std::vector<glm::vec3> points;
        int segments = calculateCubicBezierSegments(m_pathData.currentPos, controlPoint1, controlPoint2, end);

        for (int i = 1; i <= segments; i++) {
            float t = (float)i / (float)segments;
            glm::vec2 bP = nextBernstinePointCubicBezier(start, controlPoint1, controlPoint2, end, t);
            glm::vec3 p = {bP.x, bP.y, glm::mix(start.z, end.z, t)};
            points.emplace_back(p);
        }

        return points;
    }

    glm::vec2 Renderer::nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const float t) {
        float t_ = 1.0f - t;
        glm::vec2 point = (t_ * t_) * p0 + 2.0f * (t_ * t) * p1 + (t * t) * p2;
        return point;
    }

    std::vector<glm::vec3> Renderer2D::Renderer::generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end) {
        std::vector<glm::vec3> points;
        int segments = calculateQuadBezierSegments(m_pathData.currentPos, controlPoint, end);

        for (int i = 1; i <= segments; i++) {
            float t = (float)i / (float)segments;
            glm::vec2 bP = nextBernstinePointQuadBezier(start, controlPoint, end, t);
            glm::vec3 p = {bP.x, bP.y, glm::mix(start.z, end.z, t)};
            points.emplace_back(p);
        }

        return points;
    }

    QuadBezierCurvePoints Renderer::generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius) {
        glm::vec2 dir1 = glm::normalize(joinPoint - prevPoint);
        glm::vec2 dir2 = glm::normalize(nextPoint - joinPoint);
        glm::vec2 bisector = glm::normalize(dir1 + dir2);
        float offset = glm::dot(bisector, dir1);
        glm::vec2 controlPoint = joinPoint + bisector * offset;
        glm::vec2 startPoint = joinPoint - dir1 * curveRadius;
        glm::vec2 endPoint = joinPoint + dir2 * curveRadius;
        return {startPoint, controlPoint, endPoint};
    }

    std::vector<Gl::Vertex> Renderer::generateStrokeGeometry(const std::vector<glm::vec3> &points,
                                                             float width,
                                                             const glm::vec4 &color, int id, float miterLimit, bool isClosed) {
        if (points.size() < (isClosed ? 3 : 2)) {
            return {};
        }

        auto makeVertex = [&](const glm::vec2 &pos, float z, const glm::vec2 &uv) {
            Gl::Vertex v;
            v.position = glm::vec3(pos, z);
            v.color = color;
            v.id = id;
            v.texCoord = uv;
            return v;
        };

        // --- 1. Pre-calculate total path length for correct UV mapping ---
        float totalLength = 0.0f;
        for (size_t i = 0; i < points.size() - 1; ++i) {
            totalLength += glm::distance(glm::vec2(points[i]), glm::vec2(points[i + 1]));
        }
        if (isClosed) {
            totalLength += glm::distance(glm::vec2(points.back()), glm::vec2(points.front()));
        }

        std::vector<Gl::Vertex> stripVertices;
        const float halfWidth = width / 2.0f;
        float cumulativeLength = 0.0f;

        // --- 2. Generate Joins and Caps ---
        for (size_t i = 0; i < points.size(); ++i) {
            // Determine the previous, current, and next points, handling wrapping for closed paths.
            const size_t pointCount = points.size();
            const auto &p_curr = points[i];
            const auto &p_prev = isClosed ? points[(i + pointCount - 1) % pointCount] : points[std::max<long int>(0, i - 1)];
            const auto &p_next = isClosed ? points[(i + 1) % pointCount] : points[std::min(pointCount - 1, i + 1)];

            // Update cumulative length for UVs. For the first point, it's 0.
            if (i > 0) {
                cumulativeLength += glm::distance(glm::vec2(p_curr), glm::vec2(points[i - 1]));
            }
            float u = (totalLength > 0) ? (cumulativeLength / totalLength) : 0;

            // --- Handle Caps for Open Paths ---
            if (!isClosed && i == 0) { // Start Cap
                glm::vec2 dir = glm::normalize(glm::vec2(p_next) - glm::vec2(p_curr));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * halfWidth;
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) - normalVec, p_curr.z, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) + normalVec, p_curr.z, {u, 0.f}));
                continue;
            }
            if (!isClosed && i == pointCount - 1) { // End Cap
                glm::vec2 dir = glm::normalize(glm::vec2(p_curr) - glm::vec2(p_prev));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * halfWidth;
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) - normalVec, p_curr.z, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) + normalVec, p_curr.z, {u, 0.f}));
                continue;
            }

            // --- Handle Interior and Closed Path Joins ---
            glm::vec2 dir_in = glm::normalize(glm::vec2(p_curr) - glm::vec2(p_prev));
            glm::vec2 dir_out = glm::normalize(glm::vec2(p_next) - glm::vec2(p_curr));
            glm::vec2 normal_in(-dir_in.y, dir_in.x);
            glm::vec2 normal_out(-dir_out.y, dir_out.x);
            glm::vec2 miterVec = glm::normalize(normal_in + normal_out);
            float dotProduct = glm::dot(normal_in, miterVec);

            if (std::abs(dotProduct) < 1e-6f) { // Handle straight lines
                glm::vec2 normal = normal_in * halfWidth;
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) - normal, p_curr.z, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(p_curr) + normal, p_curr.z, {u, 0.f}));
                continue;
            }

            float miterLength = halfWidth / dotProduct;
            float crossProductZ = dir_in.x * dir_out.y - dir_in.y * dir_out.x;

            // Note (Shivang): There is bug here if remove stripVertices.empty() it crashes and if keep it
            // it distorts the shape for some points. Keeping it felt like best bet
            if (!stripVertices.empty() && miterLength / halfWidth > miterLimit) {
                // --- Bevel Join ---
                const auto &lastLeft = stripVertices.back();
                if (crossProductZ > 0) { // Left turn
                    glm::vec2 innerMiterPoint = glm::vec2(p_curr) - miterVec * miterLength;
                    glm::vec2 bevelP2 = glm::vec2(p_curr) + normal_out * halfWidth;
                    stripVertices.push_back(makeVertex(innerMiterPoint, p_curr.z, {u, 1.f}));
                    stripVertices.push_back(lastLeft);
                    stripVertices.push_back(makeVertex(innerMiterPoint, p_curr.z, {u, 1.f}));
                    stripVertices.push_back(makeVertex(bevelP2, p_curr.z, {u, 0.f}));
                } else { // Right turn
                    glm::vec2 innerMiterPoint = glm::vec2(p_curr) + miterVec * miterLength;
                    glm::vec2 bevelP2 = glm::vec2(p_curr) - normal_out * halfWidth;
                    stripVertices.push_back(makeVertex(bevelP2, p_curr.z, {u, 1.f}));
                    stripVertices.push_back(makeVertex(innerMiterPoint, p_curr.z, {u, 0.f}));
                    stripVertices.push_back(lastLeft);
                    stripVertices.push_back(makeVertex(bevelP2, p_curr.z, {u, 0.f}));
                }
            } else {
                // --- Miter Join ---
                glm::vec2 outerMiterPoint = glm::vec2(p_curr) + miterVec * miterLength;
                glm::vec2 innerMiterPoint = glm::vec2(p_curr) - miterVec * miterLength;
                if (crossProductZ > 0) { // Left turn
                    stripVertices.push_back(makeVertex(innerMiterPoint, p_curr.z, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerMiterPoint, p_curr.z, {u, 0.f}));
                } else { // Right turn
                    stripVertices.push_back(makeVertex(innerMiterPoint, p_curr.z, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerMiterPoint, p_curr.z, {u, 0.f}));
                }
            }
        }

        // --- 3. For closed paths, connect the end of the strip to the beginning ---
        if (isClosed && !stripVertices.empty()) {
            // Create two new vertices with the position of the first two, but with u=1
            Gl::Vertex finalRight = stripVertices[0];
            finalRight.texCoord.x = 1.0f;
            Gl::Vertex finalLeft = stripVertices[1];
            finalLeft.texCoord.x = 1.0f;

            stripVertices.push_back(finalRight);
            stripVertices.push_back(finalLeft);
        }

        return stripVertices;
    }

    int Renderer::calculateCubicBezierSegments(const glm::vec2 &p0,
                                               const glm::vec2 &p1,
                                               const glm::vec2 &p2,
                                               const glm::vec2 &p3) {
        glm::vec2 n0 = p0 / UI::UIMain::state.viewportSize;
        glm::vec2 n1 = p1 / UI::UIMain::state.viewportSize;
        glm::vec2 n2 = p2 / UI::UIMain::state.viewportSize;
        glm::vec2 n3 = p3 / UI::UIMain::state.viewportSize;

        glm::vec2 d1 = n0 - 2.0f * n1 + n2;
        glm::vec2 d2 = n1 - 2.0f * n2 + n3;

        float m1 = glm::length(d1);
        float m2 = glm::length(d2);
        float M = 6.0f * std::max(m1, m2);

        float Nf = std::sqrt(M / (8.0f * BEZIER_EPSILON));

        return std::max(1, (int)std::ceil(Nf));
    }

    int Renderer::calculateQuadBezierSegments(const glm::vec2 &p0,
                                              const glm::vec2 &p1,
                                              const glm::vec2 &p2) {
        glm::vec2 n0 = p0 / UI::UIMain::state.viewportSize;
        glm::vec2 n1 = p1 / UI::UIMain::state.viewportSize;
        glm::vec2 n2 = p2 / UI::UIMain::state.viewportSize;

        glm::vec2 d = n0 - 2.0f * n1 + n2;

        float M = 2.0f * glm::length(d);

        float Nf = std::sqrt(M / (8.0f * BEZIER_EPSILON));

        return std::max(1, (int)std::ceil(Nf));
    }

} // namespace Bess
