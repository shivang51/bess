#pragma once

#include "entt/entity/fwd.hpp"
#include "scene_new/scene.h"

namespace Bess::Canvas {
    class Artist {
      public:
        static Scene *sceneRef;
        static void drawSimEntity(entt::entity entity);
    };
} // namespace Bess::Canvas
