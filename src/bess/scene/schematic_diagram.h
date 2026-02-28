#pragma once

#include "scene/renderer/path.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/scene_component_types.h"
#include <vector>

namespace Bess::Canvas {
    class SchematicDiagram {
      public:
        SchematicDiagram() = default;
        virtual ~SchematicDiagram() = default;

        const std::vector<Renderer::Path> &getPaths() const;
        void setPaths(const std::vector<Renderer::Path> &paths);

        void addPath(const Renderer::Path &path);

        const glm::vec2 &getSize() const;
        void setSize(const glm::vec2 &size);

        std::vector<Renderer::Path> &getPathsMut();

        void normalizePaths();

        bool getShowName() const;
        void setShowName(bool show);

        float getStrokeSize() const;
        void setStrokeSize(float size);

        glm::vec2 draw(const Bess::Canvas::Transform &transform,
                       const Bess::Canvas::PickingId &pickingId,
                       const std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> &pathRenderer);

      private:
        std::vector<Renderer::Path> m_paths;
        glm::vec2 m_size; // bounding box size
        bool m_showName = true;
        float m_strokeSize = 0.f;
    };
}; // namespace Bess::Canvas
