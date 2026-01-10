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
    };
} // namespace Bess::Canvas
