#pragma once

#include <memory>

namespace Bess {

    class Camera;

    namespace Renderer {
        class MaterialRenderer;
        class PathRenderer;
    } // namespace Renderer

    namespace Canvas {
        class SceneState;
    } // namespace Canvas

    struct SceneDrawContext {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer;
        std::shared_ptr<Renderer::PathRenderer> pathRenderer;
        std::shared_ptr<Camera> camera;
    };
} // namespace Bess
