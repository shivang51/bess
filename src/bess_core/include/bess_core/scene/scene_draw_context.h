#pragma once

#include "bess_core/renderer/renderer_types.h"
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
        Core::Renderer::RenderTransformMode transformMode =
            Core::Renderer::RenderTransformMode::Camera;
        size_t viewportId;
        bool isSchematicMode = false;
    };

    struct SceneUIPrepareCtx {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer;
    };
} // namespace Bess
