#include "scene/schematic_diagram.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    const std::vector<Renderer::Path> &SchematicDiagram::getPaths() const { return m_paths; }

    void SchematicDiagram::setPaths(const std::vector<Renderer::Path> &paths) {
        m_paths = paths;
    }

    const glm::vec2 &SchematicDiagram::getSize() const { return m_size; }

    void SchematicDiagram::setSize(const glm::vec2 &size) { m_size = size; }

    std::vector<Renderer::Path> &SchematicDiagram::getPathsMut() { return m_paths; }

    bool SchematicDiagram::getShowName() const { return m_showName; }

    void SchematicDiagram::setShowName(const bool show) { m_showName = show; }

    float SchematicDiagram::getStrokeSize() const { return m_strokeSize; }

    void SchematicDiagram::setStrokeSize(const float size) {
        m_strokeSize = size;
        for (auto &path : m_paths) {
            path.setStrokeWidth(size);
        }
    }

    void SchematicDiagram::addPath(const Renderer::Path &path) {
        m_paths.emplace_back(path);
    }

    void SchematicDiagram::normalizePaths() {
        for (auto &path : m_paths) {
            path.normalize(m_size);
        }
    }

    glm::vec2 SchematicDiagram::draw(const Bess::Canvas::Transform &transform,
                                     const Bess::Canvas::PickingId &pickingId,
                                     const std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> &pathRenderer) {
        const auto &pos = transform.position;
        float dAr = getSize().x / getSize().y;
        float tAr = transform.scale.x / transform.scale.y;
        float adAr = dAr / tAr;

        auto digScale = transform.scale;
        digScale.x *= adAr;

        auto mid = digScale * 0.5f;

        auto drawInfo = Renderer2D::Vulkan::ContoursDrawInfo();
        for (auto &path : getPathsMut()) {
            const auto pathPos = path.getLowestPos();
            drawInfo.translate = glm::vec3(
                pos.x + pathPos.x - mid.x,
                pos.y + pathPos.y - mid.y,
                transform.position.z);
            drawInfo.scale = digScale;
            drawInfo.glyphId = pickingId;
            drawInfo.strokeColor = Bess::ViewportTheme::schematicViewColors.componentStroke;
            drawInfo.fillColor = Bess::ViewportTheme::schematicViewColors.componentFill;

            drawInfo.genFill = path.getProps().renderFill;
            drawInfo.genStroke = path.getProps().renderStroke;
            drawInfo.closePath = path.getProps().isClosed;
            drawInfo.rounedJoint = path.getProps().roundedJoints;
            pathRenderer->drawPath(path, drawInfo);
        }

        return digScale;
    }
} // namespace Bess::Canvas
