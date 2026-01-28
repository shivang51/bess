#include "scene_driver.h"

namespace Bess {
    void SceneDriver::update(TFrameTime deltaTime, const std::vector<ApplicationEvent> &events) {
        if (m_activeScene) {
            m_activeScene->update(deltaTime, events);
        }
    }

    void SceneDriver::render() {
        if (m_activeScene) {
            m_activeScene->render();
        }
    }

    void SceneDriver::setActiveScene(const std::shared_ptr<Canvas::Scene> &scene) {
        m_activeScene = scene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getActiveScene() const {
        return m_activeScene;
    }
} // namespace Bess
