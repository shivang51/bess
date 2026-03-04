#pragma once

#include "common/types.h"
#include "scene/scene.h"
#include <memory>

namespace Bess {
    class SceneDriver {
      public:
        SceneDriver() = default;
        ~SceneDriver() = default;

        void update(TimeMs deltaTime, const std::vector<class ApplicationEvent> &events);
        void render();

        std::shared_ptr<Canvas::Scene> getActiveScene() const;

        void addScene(const std::shared_ptr<Canvas::Scene> &scene);

        std::shared_ptr<Canvas::Scene> getSceneAtIdx(size_t index) const;

        std::shared_ptr<Canvas::Scene> createNewScene();

        std::shared_ptr<Canvas::Scene> setActiveScene(size_t index, bool updateCmdSys = true);

        size_t getActiveSceneIdx() const;

        size_t getSceneCount() const;

        // using pointer operator to directly access active scene
        std::shared_ptr<Canvas::Scene> operator->() {
            return m_activeScene;
        }

        const std::shared_ptr<Canvas::Scene> &operator->() const {
            return m_activeScene;
        }

      private:
        std::shared_ptr<Canvas::Scene> m_activeScene;
        std::vector<std::shared_ptr<Canvas::Scene>> m_scenes;
        size_t m_activeSceneIdx{0};
    };
} // namespace Bess
