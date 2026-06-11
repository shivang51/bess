#include "scene/schematic_diagram.h"
#include "application/settings/viewport_theme.h"
#include "bess_core/renderer/renderer_types.h"
#include "common/bess_assert.h"

namespace Bess::Canvas {
    const std::vector<Path> &SchematicDiagram::getPaths() const {
        return m_paths;
    }

    void SchematicDiagram::setPaths(const std::vector<Path> &paths) {
        m_paths = paths;
    }

    const glm::vec2 &SchematicDiagram::getSize() const { return m_size; }

    void SchematicDiagram::setSize(const glm::vec2 &size) { m_size = size; }

    std::vector<Path> &SchematicDiagram::getPathsMut() { return m_paths; }

    bool SchematicDiagram::getShowName() const { return m_showName; }

    void SchematicDiagram::setShowName(const bool show) { m_showName = show; }

    float SchematicDiagram::getStrokeSize() const { return m_strokeSize; }

    void SchematicDiagram::setStrokeSize(const float size) {
        m_strokeSize = size;
    }

    void SchematicDiagram::addPath(const Path &path) {
        m_paths.emplace_back(path);
    }

    void SchematicDiagram::normalizePaths() {}

    glm::vec2 SchematicDiagram::draw(
        const Bess::Canvas::Transform &transform,
        const Bess::PickingId &pickingId,
        const std::shared_ptr<Bess::Core::Renderer::IRenderer2D> &renderer) {
        if (!renderer) {
            return transform.scale;
        }

        const auto &pos = transform.position;
        float dAr = getSize().x / getSize().y;
        float tAr = transform.scale.x / transform.scale.y;
        float adAr = dAr / tAr;

        auto digScale = transform.scale;
        digScale.x *= adAr;

        auto mid = digScale * 0.5f;

        for (auto &path : getPathsMut()) {
            const auto pathPos = path.bounds().min;
            const glm::vec2 translation = {
                pos.x + pathPos.x - mid.x,
                pos.y + pathPos.y - mid.y,
            };

            Core::Renderer::PathProps props;
            props.strokeColor =
                Bess::ViewportTheme::schematicViewColors.componentStroke;
            props.fillColor =
                Bess::ViewportTheme::schematicViewColors.componentFill;
            props.strokeSize = 2.f;
            props.renderFill = true;
            props.lineJoin = Core::Renderer::PathLineJoin::Round;
            props.zIndex = transform.position.z;
            props.id = pickingId;

            renderer->drawPath(path, props);
        }

        return digScale;
    }
} // namespace Bess::Canvas
