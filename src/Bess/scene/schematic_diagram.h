#pragma once

#include "glm.hpp"
#include "scene/renderer/path.h"
#include <vector>

namespace Bess::Canvas {
    class SchematicDiagram {
      public:
        SchematicDiagram() = default;
        virtual ~SchematicDiagram() = default;

        const std::vector<Renderer::Path> &getPaths() const;
        void setPaths(const std::vector<Renderer::Path> &paths);

        const glm::vec2 &getSize() const;
        void setSize(const glm::vec2 &size);

        std::vector<Renderer::Path> &getPathsMut();

        bool showName() const;
        void setShowName(bool show);

        float getStrokeSize() const;
        void setStrokeSize(float size);

      private:
        std::vector<Renderer::Path> m_paths;
        glm::vec2 m_size; // bounding box size
        bool m_showName = true;
        float m_strokeSize = 0.f;
    };
}; // namespace Bess::Canvas
