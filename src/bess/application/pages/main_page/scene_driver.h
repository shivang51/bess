#pragma once

#include "application/app_types.h"
#include "scene/scene.h"
#include <memory>

namespace Bess {
    class SceneDriver {
      public:
        SceneDriver() = default;
        ~SceneDriver() = default;

        void createDefaultScene();

        void update(TFrameTime deltaTime, const std::vector<class ApplicationEvent> &events);
        void render();

        void setActiveScene(const std::shared_ptr<Canvas::Scene> &scene);
        std::shared_ptr<Canvas::Scene> getActiveScene() const;

        // using pointer operator to directly access active scene
        std::shared_ptr<Canvas::Scene> operator->() {
            return m_activeScene;
        }

      private:
        std::shared_ptr<Canvas::Scene> m_activeScene;
        std::vector<std::shared_ptr<Canvas::Scene>> m_scenes;
    };
} // namespace Bess
