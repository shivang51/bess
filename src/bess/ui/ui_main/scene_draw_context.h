#pragma once

#include "camera.h"
#include "renderer/material_renderer.h"
#include "scene_state/scene_state.h"

namespace Bess {
    struct SceneDrawContext {
        Bess::Canvas::SceneState *sceneState;
        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer;
        std::shared_ptr<Canvas::PathRenderer> pathRenderer;
        std::shared_ptr<Camera> camera;
    };
} // namespace Bess
