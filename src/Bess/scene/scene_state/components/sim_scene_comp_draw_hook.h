#pragma once
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
} // namespace Bess

namespace Bess::Canvas {
    class SimSceneCompDrawHook {
      public:
        virtual ~SimSceneCompDrawHook() = default;

        virtual void onDraw(SceneState &state,
                            std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                            std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) = 0;

        virtual void onSchematicDraw(SceneState &state,
                                     std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                     std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) = 0;

        bool isDrawEnabled() const { return m_drawEnabled; }
        void setDrawEnabled(bool enabled) { m_drawEnabled = enabled; }

        bool isSchematicDrawEnabled() const { return m_schematicDrawEnabled; }
        void setSchematicDrawEnabled(bool enabled) {
            m_schematicDrawEnabled = enabled;
        }

      protected:
        bool m_drawEnabled = true;
        bool m_schematicDrawEnabled = true;
    };
} // namespace Bess::Canvas
