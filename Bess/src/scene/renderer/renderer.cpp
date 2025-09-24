#include "scene/renderer/renderer.h"
#include "asset_manager/asset_manager.h"
#include "assets.h"
#include "camera.h"
#include "ext/matrix_transform.hpp"
#include "ext/quaternion_geometric.hpp"
#include "geometric.hpp"
#include "glm.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/vector_angle.hpp"
#include "scene/renderer/asset_loaders.h"
#include "scene/renderer/gl/gl_wrapper.h"
#include "scene/renderer/gl/primitive_type.h"
#include "scene/renderer/gl/vertex.h"
#include "ui/ui_main/ui_main.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <ostream>

using namespace Bess::Renderer2D;

namespace Bess {

// disabled composite pass logic for now
#define BESS_RENDERER_DISABLE_RENDERPASS

    static constexpr uint32_t PRIMITIVE_RESTART = 0xFFFFFFFF;
    static constexpr float BEZIER_EPSILON = 0.0001f;

    std::vector<PrimitiveType> Renderer::m_AvailablePrimitives;

    std::vector<std::shared_ptr<Gl::Shader>> Renderer::m_shaders;
    std::vector<size_t> Renderer::m_MaxRenderLimit;

    std::shared_ptr<Camera> Renderer::m_camera;

    std::vector<glm::vec4> Renderer::m_StandardQuadVertices;
    std::vector<glm::vec4> Renderer::m_StandardTriVertices;

    RenderData Renderer::m_renderData;
    PathContext Renderer::m_pathData;

    std::shared_ptr<Gl::Shader> Renderer::m_gridShader;
    std::unique_ptr<Gl::GridVao> Renderer::m_gridVao;

    std::unique_ptr<Gl::Shader> Renderer::m_shadowPassShader;
    std::unique_ptr<Gl::Shader> Renderer::m_compositePassShader;
    std::unique_ptr<Gl::Vao> Renderer::m_renderPassVao;
    std::vector<Gl::Vertex> Renderer::m_pathStripVertices;
    std::vector<GLuint> Renderer::m_pathStripIndices;

    std::shared_ptr<Font> Renderer::m_Font;
    std::shared_ptr<MsdfFont> Renderer::m_msdfFont;
    std::unordered_map<std::shared_ptr<Gl::Texture>, std::vector<Gl::QuadVertex>> Renderer::m_textureQuadVertices;

    std::unique_ptr<Bess::Gl::QuadVao> Renderer::m_quadRendererVao;
    std::unique_ptr<Bess::Gl::CircleVao> Renderer::m_circleRendererVao;
    std::unique_ptr<Bess::Gl::TriangleVao> Renderer::m_triangleRendererVao;
    std::unique_ptr<Bess::Gl::InstancedVao<Gl::InstanceVertex>> Renderer::m_textRendererVao;
    std::unique_ptr<Bess::Gl::BatchVao<Gl::Vertex>> Renderer::m_pathRendererVao;
    std::unique_ptr<Bess::Gl::InstancedVao<Gl::InstanceVertex>> Renderer::m_lineRendererVao;

    std::array<int, 32> Renderer::m_texSlots;

    void Renderer::init() {
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

        m_AvailablePrimitives = {PrimitiveType::line, PrimitiveType::triangle,
                                 PrimitiveType::quad, PrimitiveType::circle, PrimitiveType::path, PrimitiveType::text};

        int maxPrimNum = 0;
        for (auto prim : m_AvailablePrimitives) {
            maxPrimNum = std::max(maxPrimNum, (int)prim);
        }

        maxPrimNum += 1;

        m_shaders.resize(maxPrimNum);
        m_MaxRenderLimit.resize(maxPrimNum);

        for (auto prim : m_AvailablePrimitives) {
            m_MaxRenderLimit[(int)prim] = 8000;
        }

        auto &assetManager = Assets::AssetManager::instance();
        m_quadRendererVao = std::make_unique<Bess::Gl::QuadVao>(m_MaxRenderLimit[(int)PrimitiveType::quad]);
        m_circleRendererVao = std::make_unique<Bess::Gl::CircleVao>(m_MaxRenderLimit[(int)PrimitiveType::circle]);
        m_triangleRendererVao = std::make_unique<Bess::Gl::TriangleVao>(m_MaxRenderLimit[(int)PrimitiveType::triangle]);
        m_textRendererVao = std::make_unique<Bess::Gl::InstancedVao<Gl::InstanceVertex>>(m_MaxRenderLimit[(int)PrimitiveType::text]);
        m_lineRendererVao = std::make_unique<Bess::Gl::InstancedVao<Gl::InstanceVertex>>(m_MaxRenderLimit[(int)PrimitiveType::line]);
        m_pathRendererVao = std::make_unique<Bess::Gl::BatchVao<Gl::Vertex>>(m_MaxRenderLimit[(int)PrimitiveType::path], 3, 3, true, false);

        m_gridVao = std::make_unique<Bess::Gl::GridVao>();
        m_gridShader = assetManager.get(Assets::Shaders::grid);

        for (auto primitive : m_AvailablePrimitives) {
            int primIdx = (int)primitive;
            switch (primitive) {
            case PrimitiveType::quad:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::quad);
                break;
            case PrimitiveType::path:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::path);
                break;
            case PrimitiveType::circle:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::circle);
                break;
            case PrimitiveType::triangle:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::triangle);
                break;
            case PrimitiveType::line:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::line);
                break;
            case PrimitiveType::text:
                m_shaders[primIdx] = assetManager.get(Assets::Shaders::text);
                break;
            }
        }

        m_StandardQuadVertices = {
            {-0.5f, 0.5f, 0.f, 1.f},
            {-0.5f, -0.5f, 0.f, 1.f},
            {0.5f, -0.5f, 0.f, 1.f},
            {0.5f, 0.5f, 0.f, 1.f},
        };

        m_StandardTriVertices = {
            {-0.5f, 0.0f, 0.f, 1.f},
            {0.5f, 0.0f, 0.f, 1.f},
            {0.0f, 0.5f, 0.f, 1.f}};

        m_Font = assetManager.get(Assets::Fonts::roboto);
        m_msdfFont = assetManager.get(Assets::Fonts::robotoMsdf);

        for (int i = 0; i < 32; i++) {
            m_texSlots[i] = i;
        }
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                        const glm::vec4 &color, int id,
                        QuadRenderProperties properties) {

        if (properties.hasShadow) {
            QuadRenderProperties props;
            props.borderRadius = properties.borderRadius;
            props.isMica = true;
            quad({pos.x, pos.y + 2.5f, pos.z - 0.001},
                 {size.x + 4.f, size.y + 1.f},
                 Assets::AssetManager::instance().get(Assets::Textures::shadowTexture),
                 glm::vec4(glm::vec3(0.25), 0.35f), id, props);
        }
        Gl::QuadVertex quadInstance{};
        quadInstance.position = pos;
        quadInstance.size = size;
        quadInstance.color = color;
        quadInstance.id = id;
        quadInstance.borderRadius = properties.borderRadius;
        quadInstance.borderColor = properties.borderColor;
        quadInstance.borderSize = properties.borderSize;
        quadInstance.isMica = properties.isMica ? 1 : 0;
        quadInstance.texSlotIdx = 0;
        quadInstance.texData = {0.f, 0.f, 1.f, 1.f};

        auto &vertices = m_renderData.quadVertices;
        if (vertices.size() == m_MaxRenderLimit[(int)PrimitiveType::quad]) {
            flush(PrimitiveType::quad);
        }
        vertices.emplace_back(quadInstance);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::Texture> texture,
                        const glm::vec4 &tintColor, int id, QuadRenderProperties properties) {
        int idx = 0;
        if (m_textureQuadVertices.find(texture) == m_textureQuadVertices.end()) {
            m_textureQuadVertices[texture] = std::vector<Gl::QuadVertex>();
            idx = m_textureQuadVertices.size();
        }

        auto &verticesStore = m_textureQuadVertices[texture];
        if (!verticesStore.empty())
            idx = verticesStore.front().texSlotIdx;

        Gl::QuadVertex quadInstance{};
        quadInstance.position = pos;
        quadInstance.size = size;
        quadInstance.color = tintColor;
        quadInstance.id = id;
        quadInstance.borderRadius = properties.borderRadius;
        quadInstance.borderColor = properties.borderColor;
        quadInstance.borderSize = properties.borderSize;
        quadInstance.isMica = properties.isMica ? 1 : 0;
        quadInstance.texSlotIdx = idx;
        quadInstance.texData = {0.f, 0.f, 1.f, 1.f};
        m_textureQuadVertices[texture].emplace_back(quadInstance);
    }

    void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size, std::shared_ptr<Gl::SubTexture> subTexture,
                        const glm::vec4 &tintColor, int id, QuadRenderProperties properties) {

        auto texture = subTexture->getTexture();

        int idx = 0;
        if (m_textureQuadVertices.find(texture) == m_textureQuadVertices.end()) {
            m_textureQuadVertices[texture] = std::vector<Gl::QuadVertex>();
            idx = m_textureQuadVertices.size();
        }

        auto &verticesStore = m_textureQuadVertices[texture];
        if (!verticesStore.empty())
            idx = verticesStore.front().texSlotIdx;

        Gl::QuadVertex quadInstance{};
        quadInstance.position = pos;
        quadInstance.size = size;
        quadInstance.color = tintColor;
        quadInstance.id = id;
        quadInstance.borderRadius = properties.borderRadius;
        quadInstance.borderColor = properties.borderColor;
        quadInstance.borderSize = properties.borderSize;
        quadInstance.isMica = properties.isMica ? 1 : 0;
        quadInstance.texSlotIdx = idx;
        quadInstance.texData = subTexture->getStartWH();
        verticesStore.emplace_back(quadInstance);
    }

    void Renderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id, const glm::vec4 &color) {
        std::vector<Gl::GridVertex> vertices(4);

        auto transform = glm::translate(glm::mat4(1.0f), pos);
        transform = glm::scale(transform, {size.x, size.y, 1.f});

        for (int i = 0; i < 4; i++) {
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardQuadVertices[i];
            vertex.id = id;
            vertex.ar = size.x / size.y;
            vertex.color = color;
        }

        vertices[0].texCoord = {0.0f, 1.0f};
        vertices[1].texCoord = {0.0f, 0.0f};
        vertices[2].texCoord = {1.0f, 0.0f};
        vertices[3].texCoord = {1.0f, 1.0f};

        m_gridShader->bind();
        m_gridVao->bind();

        m_gridShader->setUniformMat4("u_mvp", m_camera->getOrtho());
        m_gridShader->setUniform1f("u_zoom", m_camera->getZoom());

        auto camPos = m_camera->getPos();
        auto viewportPos = UI::UIMain::state.viewportPos / m_camera->getZoom();
        m_gridShader->setUniformVec2("u_cameraOffset", {-viewportPos.x - camPos.x, viewportPos.y + m_camera->getSpan().y + camPos.y});

        m_gridVao->setVertices(vertices);

        Gl::Api::drawElements(GL_TRIANGLES, 6);

        m_gridShader->unbind();
        m_gridVao->unbind();
    }

    void Renderer::circle(const glm::vec3 &center, const float radius,
                          const glm::vec4 &color, const int id, float innerRadius) {
        Gl::CircleVertex vertex{
            .position = center,
            .color = color,
            .radius = radius,
            .innerRadius = innerRadius,
            .id = id};

        auto &vertices = m_renderData.circleVertices;
        if (vertices.size() == m_MaxRenderLimit[(int)PrimitiveType::circle]) {
            flush(PrimitiveType::circle);
        }
        vertices.emplace_back(vertex);
    }

    void Renderer::msdfText(const std::string &text, const glm::vec3 &pos, const size_t size,
                            const glm::vec4 &color, const int id, float angle) {
        // Command to use to generate MSDF font texture atlas
        // https://github.com/Chlumsky/msdf-atlas-gen
        // msdf-atlas-gen -font Roboto-Regular.ttf -type mtsdf -size 64 -imageout roboto_mtsdf.png -json roboto.json -pxrange 4

        if (text.empty())
            return;

        float scale = size;
        float lineHeight = m_msdfFont->getLineHeight() * scale;

        MsdfCharacter yCharInfo = m_msdfFont->getCharacterData('y');
        MsdfCharacter wCharInfo = m_msdfFont->getCharacterData('W');

        float baseLineOff = yCharInfo.offset.y - wCharInfo.offset.y;

        glm::vec2 charPos = pos;
        for (auto &ch : text) {
            const MsdfCharacter &charInfo = m_msdfFont->getCharacterData(ch);
            if (ch == ' ') {
                charPos.x += charInfo.advance * scale;
                continue;
            }
            const auto &subTexture = charInfo.subTexture;
            glm::vec2 size_ = charInfo.size * scale;
            float xOff = (charInfo.offset.x + charInfo.size.x / 2.f) * scale;
            float yOff = (charInfo.offset.y + charInfo.size.y / 2.f) * scale;

            Gl::InstanceVertex vertex{};
            vertex.position = {charPos.x + xOff, charPos.y - yOff, pos.z};
            vertex.size = size_;
            vertex.color = color;
            vertex.id = id;
            vertex.texSlotIdx = 1;
            vertex.texData = subTexture->getStartWH();
            auto &vertices = m_renderData.fontVertices;
            if (vertices.size() == m_MaxRenderLimit[(int)PrimitiveType::text]) {
                flush(PrimitiveType::text);
            }
            vertices.emplace_back(vertex);
            charPos.x += charInfo.advance * scale;
        }
    }

    void Renderer::text(const std::string &text, const glm::vec3 &pos, const size_t size,
                        const glm::vec4 &color, const int id, float angle) {
        float scale = Font::getScale(size);

        float x = pos.x;
        float y = pos.y;
        for (const char &c : text) {
            auto &ch = m_Font->getCharacter(c);

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y + (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            quad({xpos + w / 2.f, ypos - h / 2.f, pos.z}, {w, h}, ch.Texture, color, id);

            x += (ch.Advance >> 6) * scale;
        }
    }

    void Renderer2D::Renderer::line(const glm::vec3 &start, const glm::vec3 &end, float size, const glm::vec4 &color, const int id) {
        glm::vec2 direction = end - start;
        float length = glm::length(direction);
        glm::vec2 pos = (start + end) * 0.5f;
        float angle = glm::atan(direction.y, direction.x);

        Gl::InstanceVertex vertex{
            .position = glm::vec3({pos, start.z}),
            .size = glm::vec2({length, size}),
            .angle = angle,
            .color = color,
            .id = id,
            .texSlotIdx = 0};

        auto &vertices = m_renderData.lineVertices;
        if (vertices.size() == m_MaxRenderLimit[(int)PrimitiveType::line]) {
            flush(PrimitiveType::line);
        }
        vertices.emplace_back(vertex);
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
        std::runtime_error("Triangle API is not implemented");
        std::vector<Gl::Vertex> vertices(3);

        for (int i = 0; i < vertices.size(); i++) {
            auto transform = glm::translate(glm::mat4(1.0f), points[i]);
            auto &vertex = vertices[i];
            vertex.position = transform * m_StandardTriVertices[i];
            vertex.id = id;
            vertex.color = color;
        }

        // vertices[0].texCoord = {0.0f, 0.0f};
        // vertices[1].texCoord = {0.0f, 0.5f};
        // vertices[2].texCoord = {1.0f, 1.0f};

        addTriangleVertices(vertices);
    }

    void Renderer::addTriangleVertices(const std::vector<Gl::Vertex> &vertices) {
        auto max_render_count = m_MaxRenderLimit[(int)PrimitiveType::circle];

        // auto &primitive_vertices = m_RenderData.triangleVertices;

        // if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
        //     flush(PrimitiveType::triangle);
        // }

        // primitive_vertices.insert(primitive_vertices.end(), vertices.begin(), vertices.end());
    }

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

    void Renderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, const int id) {
        m_pathData.ended = false;
        m_pathData.currentPos = startPos;
        m_pathData.points.emplace_back(startPos, weight, id);
        m_pathData.color = color;
    }

    void Renderer::endPathMode(bool closePath) {
        auto vertices = generateStrokeGeometry(m_pathData.points, m_pathData.color, 4.f, closePath);
        size_t idx = m_pathStripVertices.size();
        for (auto &v : vertices) {
            m_pathStripVertices.emplace_back(v);
            m_pathStripIndices.emplace_back((GLuint)idx++);
        }
        m_pathData = {};
        m_pathStripIndices.emplace_back(PRIMITIVE_RESTART);
    }

    void Renderer::pathLineTo(const glm::vec3 &pos, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        m_pathData.points.emplace_back(pos, weight, id);
        m_pathData.setCurrentPos(pos);
    }

    void Renderer2D::Renderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        auto positions = generateCubicBezierPoints(m_pathData.currentPos, controlPoint1, controlPoint2, end);
        PathPoint p{};
        for (auto pos : positions) {
            p.pos = pos;
            p.weight = weight;
            p.id = id;
            m_pathData.points.emplace_back(p);
        }
        m_pathData.setCurrentPos(end);
    }

    void Renderer2D::Renderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, const int id) {
        assert(!m_pathData.ended);
        auto positions = generateQuadBezierPoints(m_pathData.currentPos, controlPoint, end);
        PathPoint p{};
        for (auto pos : positions) {
            p.pos = pos;
            p.weight = weight;
            p.id = id;
            m_pathData.points.emplace_back(p);
        }
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
        auto &shader = m_shaders[(int)type];
        shader->bind();

        shader->setUniformMat4("u_mvp", m_camera->getTransform());

        switch (type) {
        case PrimitiveType::quad: {
            shader->setUniform1f("u_zoom", m_camera->getZoom());
            m_quadRendererVao->bind();
            m_quadRendererVao->updateInstanceData(m_renderData.quadVertices);
            Gl::Api::drawElementsInstanced(GL_TRIANGLES, m_quadRendererVao->getIndexCount(), m_renderData.quadVertices.size());
            m_renderData.quadVertices.clear();

            if (!m_textureQuadVertices.empty()) {
                shader->setUniform1iv("u_Textures", m_texSlots.data(), m_texSlots.size());
                for (auto &[tex, verticies] : m_textureQuadVertices) {
                    tex->bind(verticies.front().texSlotIdx);
                    m_quadRendererVao->updateInstanceData(verticies);
                    Gl::Api::drawElementsInstanced(GL_TRIANGLES, m_quadRendererVao->getIndexCount(), verticies.size());
                    verticies.clear();
                }
                m_textureQuadVertices.clear();
            }

            m_quadRendererVao->unbind();
        } break;
        case PrimitiveType::circle: {
            m_circleRendererVao->bind();
            m_circleRendererVao->updateInstanceData(m_renderData.circleVertices);
            Gl::Api::drawElementsInstanced(GL_TRIANGLES, m_circleRendererVao->getIndexCount(), m_renderData.circleVertices.size());
            m_circleRendererVao->unbind();
            m_renderData.circleVertices.clear();
        } break;
        case PrimitiveType::path: {
            shader->setUniform1f("u_zoom", m_camera->getZoom());
            m_pathRendererVao->bind();
            m_pathRendererVao->setVerticesAndIndicies(m_pathStripVertices, m_pathStripIndices);
            GL_CHECK(glEnable(GL_PRIMITIVE_RESTART));
            GL_CHECK(glPrimitiveRestartIndex(PRIMITIVE_RESTART));
            Gl::Api::drawElements(GL_TRIANGLE_STRIP, (GLsizei)(m_pathStripIndices.size()));
            m_pathRendererVao->unbind();
            m_pathStripVertices.clear();
            m_pathStripIndices.clear();
        } break; /*
        case PrimitiveType::triangle: {
            auto &vertices = m_renderData.triangleVertices;
            //vao->setVertices(vertices.data(), vertices.size());
            Gl::Api::drawElements(GL_TRIANGLES, vertices.size());
            vertices.clear();
        } break;*/
        case PrimitiveType::line: {
            auto &vertices = m_renderData.lineVertices;
            m_lineRendererVao->bind();
            m_lineRendererVao->updateInstanceData(vertices);
            Gl::Api::drawElementsInstanced(GL_TRIANGLES, m_lineRendererVao->getIndexCount(), vertices.size());
            m_lineRendererVao->unbind();
            vertices.clear();
        } break;
        case PrimitiveType::text: {
            auto &vertices = m_renderData.fontVertices;
            if (vertices.empty()) {
                shader->unbind();
                return;
            }
            shader->setUniform1f("u_pxRange", 4);
            shader->setUniform1iv("u_Textures", m_texSlots.data(), m_texSlots.size());
            m_msdfFont->getTextureAtlas()->bind(1);
            m_textRendererVao->bind();
            m_textRendererVao->updateInstanceData(vertices);
            Gl::Api::drawElementsInstanced(GL_TRIANGLES, m_textRendererVao->getIndexCount(), vertices.size());
            m_textRendererVao->unbind();
            vertices.clear();
        } break;
        }

        shader->unbind();
    }

    glm::vec2 Renderer2D::Renderer::getCharRenderSize(char ch, float renderSize) {
        std::string key = std::to_string(renderSize) + ch;
        auto ch_ = m_Font->getCharacter(ch);
        float scale = m_Font->getScale(renderSize);
        glm::vec2 size = {(ch_.Advance >> 6), ch_.Size.y};
        size = {size.x * scale, size.y * scale};
        return size;
    }

    glm::vec2 Renderer2D::Renderer::getTextRenderSize(const std::string &str, float renderSize) {
        float xSize = 0;
        float ySize = 0;
        for (auto &ch : str) {
            auto s = getCharRenderSize(ch, renderSize);
            xSize += s.x;
            ySize = std::max(xSize, s.y);
        }
        return {xSize, ySize};
    }

    glm::vec2 Renderer2D::Renderer::getMSDFTextRenderSize(const std::string &str, float renderSize) {
        float xSize = 0;
        float ySize = m_msdfFont->getLineHeight();

        for (auto &ch : str) {
            auto chInfo = m_msdfFont->getCharacterData(ch);
            xSize += chInfo.advance;
        }
        return glm::vec2({xSize, ySize}) * renderSize;
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

    std::vector<Gl::Vertex> Renderer::generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                             const glm::vec4 &color, float miterLimit, bool isClosed) {
        if (points.size() < (isClosed ? 3 : 2)) {
            return {};
        }

        auto makeVertex = [&](const glm::vec2 &pos, float z, uint64_t id, const glm::vec2 &uv) {
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
            totalLength += glm::distance(glm::vec2(points[i].pos), glm::vec2(points[i + 1].pos));
        }
        if (isClosed) {
            totalLength += glm::distance(glm::vec2(points.back().pos), glm::vec2(points.front().pos));
        }

        std::vector<Gl::Vertex> stripVertices;
        float cumulativeLength = 0.0f;
        const size_t pointCount = points.size();
        const size_t n = isClosed ? pointCount : pointCount;

        // --- 2. Generate Joins and Caps ---
        for (size_t i = 0; i < n; ++i) {
            // Determine the previous, current, and next points, handling wrapping for closed paths.
            const auto &pCurr = points[i];
            const auto &pPrev = isClosed ? points[(i + pointCount - 1) % pointCount] : points[std::max<long int>(0, i - 1)];
            const auto &pNext = isClosed ? points[(i + 1) % pointCount] : points[std::min(pointCount - 1, i + 1)];

            // Update cumulative length for UVs. For the first point, it's 0.
            if (i > 0) {
                cumulativeLength += glm::distance(glm::vec2(pCurr.pos), glm::vec2(points[i - 1].pos));
            }
            float u = (totalLength > 0) ? (cumulativeLength / totalLength) : 0;

            // --- Handle Caps for Open Paths ---
            if (!isClosed && i == 0) { // Start Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            if (!isClosed && i == n - 1) { // End Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            // --- Handle Interior and Closed Path Joins ---
            glm::vec2 dirIn = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
            glm::vec2 dirOut = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
            glm::vec2 dirOutToPrev = glm::normalize(glm::vec2(pPrev.pos - pCurr.pos));
            glm::vec2 normalIn(-dirIn.y, dirIn.x);
            glm::vec2 normalOut(-dirOut.y, dirOut.x);
            glm::vec2 miterVec = glm::normalize(normalIn + normalOut);
            float dotProduct = glm::dot(normalIn, miterVec);

            // Handle straight lines
            if (std::abs(dotProduct) < 1e-6f) {
                glm::vec2 normal = normalIn * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normal, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normal, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            // Joint
            {
                auto uVec = dirOutToPrev;
                auto vVec = dirOut;
                float angle = glm::angle(uVec, vVec);

                float uMag = pNext.weight / (2 * glm::sin(angle));
                float vMag = pCurr.weight / (2 * glm::sin(angle));

                uVec *= uMag;
                vVec *= vMag;

                auto disp = uVec + vVec;

                float crossProductZ = dirIn.x * dirOut.y - dirIn.y * dirOut.x;

                if (glm::length(disp) > miterLimit) {
                    glm::vec2 normalIn = glm::normalize(glm::vec2(-dirIn.y, dirIn.x));

                    glm::vec2 pos = pCurr.pos;
                    float halfWidth = pCurr.weight / 2.f; // Assuming constant width through the joint

                    glm::vec2 outerPrev = pos + normalIn * halfWidth;
                    glm::vec2 innerPrev = pos - normalIn * halfWidth;

                    glm::vec2 outerNext = pos + normalOut * halfWidth;
                    glm::vec2 innerNext = pos - normalOut * halfWidth;
                    stripVertices.push_back(makeVertex(innerPrev, pCurr.pos.z, pCurr.id, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerPrev, pCurr.pos.z, pCurr.id, {u, 0.f}));

                    stripVertices.push_back(makeVertex(innerNext, pCurr.pos.z, pCurr.id, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerNext, pCurr.pos.z, pCurr.id, {u, 0.f}));
                } else {
                    auto D = glm::vec2(pCurr.pos) + disp;
                    auto E = glm::vec2(pCurr.pos) - disp;

                    if (crossProductZ > 0) { // Left turn
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pCurr.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pCurr.id, {u, 0.f}));
                    } else { // Right turn
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pCurr.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pCurr.id, {u, 0.f}));
                    }
                }
            }
        }

        if (isClosed && !stripVertices.empty()) {
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
