#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_ui/layout.h"
#include <memory>

namespace Bess {

    class Camera;

    namespace Core::Renderer {
        class IRenderer2D;
    } // namespace Core::Renderer

    namespace Canvas {
        class SceneState;

        namespace SceneWidgets {
            struct SceneWidgetsState;
        } // namespace SceneWidgets
    } // namespace Canvas

    struct SceneDrawContext {
        Bess::Canvas::SceneState *sceneState = nullptr;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        Core::Renderer::RenderTransformMode transformMode =
            Core::Renderer::RenderTransformMode::Camera;
        size_t viewportId = 0;
        bool isSchematicMode = false;
        Canvas::SceneWidgets::SceneWidgetsState *sceneWidgetsState = nullptr;
    };

    struct SceneUIPrepareCtx {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer;
        Canvas::UI::UINode *parentNode = nullptr;
    };
} // namespace Bess
