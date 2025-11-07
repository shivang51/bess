#pragma once

#include "application/types.h"
#include "scene/scene.h"
#include <memory>

namespace Bess {
    class SceneDriver {
      public:
        SceneDriver();
        ~SceneDriver();

        void update(TFrameTime deltaTime, const std::vector<class ApplicationEvent> &events);
        void render();

        void setActiveScene(const std::shared_ptr<Canvas::Scene> &scene);
        std::shared_ptr<Canvas::Scene> getActiveScene() const;

      private:
        std::shared_ptr<Canvas::Scene> m_activeScene;
    };
} // namespace Bess
