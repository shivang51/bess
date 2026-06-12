#pragma once

#include <memory>

namespace Bess {

    class Camera;

    namespace Core::Renderer {
        class IRenderer2D;
    } // namespace Core::Renderer

    namespace Canvas {
        class SceneState;
    } // namespace Canvas

    struct SceneDrawContext {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer;
        std::shared_ptr<Camera> camera;
    };
} // namespace Bess
