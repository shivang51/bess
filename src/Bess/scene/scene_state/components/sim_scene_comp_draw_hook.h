#pragma once
#include "scene/scene_state/components/scene_component_types.h"
#include <memory>

namespace Bess {
    namespace Canvas {
        class SceneState;
    }
    namespace Renderer {
        class MaterialRenderer;
    } // namespace Renderer

    namespace Renderer2D::Vulkan {
        class PathRenderer;
    } // namespace Renderer2D::Vulkan

    namespace SimEngine {
        class ComponentState;
    }
} // namespace Bess

namespace Bess::Canvas {
    struct DrawHookOnDrawResult {
        bool sizeChanged = false;
        glm::vec2 newSize = glm::vec2(0.0f);
        bool drawChildren = true;
        bool drawOriginal = true;
    };

    class SimSceneCompDrawHook {
      public:
        virtual ~SimSceneCompDrawHook() = default;

        virtual DrawHookOnDrawResult onDraw(const Transform &transform,
                                            const PickingId &pickingId,
                                            const SimEngine::ComponentState &compState,
                                            std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                            std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) = 0;

        virtual glm::vec2 onSchematicDraw(const Transform &transform,
                                          const PickingId &pickingId,
                                          std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                          std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) = 0;

        bool isDrawEnabled() const { return m_drawEnabled; }
        void setDrawEnabled(bool enabled) { m_drawEnabled = enabled; }

        bool isSchematicDrawEnabled() const { return m_schematicDrawEnabled; }
        void setSchematicDrawEnabled(bool enabled) {
            m_schematicDrawEnabled = enabled;
        }

      protected:
        bool m_drawEnabled = false;
        bool m_schematicDrawEnabled = false;
    };
} // namespace Bess::Canvas
