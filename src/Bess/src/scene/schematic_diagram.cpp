#include "scene/schematic_diagram.h"

namespace Bess::Canvas {
    const std::vector<Renderer::Path> &SchematicDiagram::getPaths() const { return m_paths; }

    void SchematicDiagram::setPaths(const std::vector<Renderer::Path> &paths) { m_paths = paths; }

    const glm::vec2 &SchematicDiagram::getSize() const { return m_size; }

    void SchematicDiagram::setSize(const glm::vec2 &size) { m_size = size; }

    std::vector<Renderer::Path> &SchematicDiagram::getPathsMut() { return m_paths; }

    bool SchematicDiagram::showName() const { return m_showName; }

    void SchematicDiagram::setShowName(const bool show) { m_showName = show; }

    float SchematicDiagram::getStrokeSize() const { return m_strokeSize; }

    void SchematicDiagram::setStrokeSize(const float size) { m_strokeSize = size; }
} // namespace Bess::Canvas
