#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_path.h"
#include "common/types.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include <vector>

namespace Bess::Canvas {

    using Path = Bess::Core::Renderer::Path2D;

    class BESS_API SchematicDiagram {
      public:
        SchematicDiagram() = default;
        virtual ~SchematicDiagram() = default;

        const std::vector<Path> &getPaths() const;
        void setPaths(const std::vector<Path> &paths);

        void addPath(const Path &path);

        const glm::vec2 &getSize() const;
        void setSize(const glm::vec2 &size);

        std::vector<Path> &getPathsMut();

        void normalizePaths();

        bool getShowName() const;
        void setShowName(bool show);

        float getStrokeSize() const;
        void setStrokeSize(float size);

        glm::vec2 draw(
            const Bess::Canvas::Transform &transform,
            const Bess::PickingId &pickingId,
            const std::shared_ptr<Bess::Core::Renderer::IRenderer2D> &renderer);

      private:
        std::vector<Path> m_paths;
        glm::vec2 m_size; // bounding box size
        bool m_showName = true;
        float m_strokeSize = 0.f;
    };
}; // namespace Bess::Canvas
