#include "scene/schematic_diagram.h"

namespace Bess::Canvas {
    const std::vector<Renderer::Path> &SchematicDiagram::getPaths() const { return m_paths; }

    void SchematicDiagram::setPaths(const std::vector<Renderer::Path> &paths) { m_paths = paths; }

    const glm::vec2 &SchematicDiagram::getSize() const { return m_size; }

    void SchematicDiagram::setSize(const glm::vec2 &size) { m_size = size; }

    std::vector<Renderer::Path> &SchematicDiagram::getPathsMut() { return m_paths; }
} // namespace Bess::Canvas
