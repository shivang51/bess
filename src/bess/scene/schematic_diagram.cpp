#include "scene/schematic_diagram.h"
#include "application/settings/viewport_theme.h"

namespace Bess::Canvas {
    namespace {
        [[nodiscard]] Core::Renderer::Path2D
        toRendererPath(const Renderer::Path &path, const glm::vec2 &scale,
                       const glm::vec2 &translation) {
            auto renderPath = path.copy();
            renderPath.scale(scale);

            Core::Renderer::Path2D out;
            out.reserve(renderPath.getCmds().size());

            const auto transformPoint =
                [translation](const glm::vec2 &point) {
                    return point + translation;
                };

            for (const auto &command : renderPath.getCmds()) {
                using Kind = Renderer::Path::PathCommand::Kind;
                switch (command.kind) {
                case Kind::Move:
                    out.moveTo(transformPoint(command.move.p));
                    break;
                case Kind::Line:
                    out.lineTo(transformPoint(command.line.p));
                    break;
                case Kind::Quad:
                    out.quadTo(transformPoint(command.quad.c),
                               transformPoint(command.quad.p));
                    break;
                case Kind::Cubic:
                    out.cubicTo(transformPoint(command.cubic.c1),
                                transformPoint(command.cubic.c2),
                                transformPoint(command.cubic.p));
                    break;
                }
            }

            return out;
        }
    } // namespace

    const std::vector<Renderer::Path> &SchematicDiagram::getPaths() const {
        return m_paths;
    }

    void SchematicDiagram::setPaths(const std::vector<Renderer::Path> &paths) {
        m_paths = paths;
    }

    const glm::vec2 &SchematicDiagram::getSize() const { return m_size; }

    void SchematicDiagram::setSize(const glm::vec2 &size) { m_size = size; }

    std::vector<Renderer::Path> &SchematicDiagram::getPathsMut() {
        return m_paths;
    }

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
            const auto pathPos = path.getLowestPos();
            const glm::vec2 translation = {
                pos.x + pathPos.x - mid.x,
                pos.y + pathPos.y - mid.y,
            };

            auto rendererPath = toRendererPath(path, digScale, translation);
            if (rendererPath.empty()) {
                continue;
            }

            Core::Renderer::PathProps props;
            props.strokeColor =
                Bess::ViewportTheme::schematicViewColors.componentStroke;
            props.fillColor =
                Bess::ViewportTheme::schematicViewColors.componentFill;
            props.strokeSize = path.getStrokeWidth();
            props.renderFill = path.getProps().renderFill;
            props.closePath = path.getProps().isClosed;
            props.lineJoin = path.getProps().roundedJoints
                                 ? Core::Renderer::PathLineJoin::Round
                                 : Core::Renderer::PathLineJoin::Miter;
            props.zIndex = transform.position.z;
            props.id = pickingId;

            if (!path.getProps().renderStroke) {
                props.strokeColor.a = 0.f;
                props.strokeSize = 0.f;
            }

            renderer->drawPath(rendererPath, props);
        }

        return digScale;
    }
} // namespace Bess::Canvas
