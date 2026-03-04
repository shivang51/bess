#include "scene_driver.h"
#include "pages/main_page/main_page.h"

namespace Bess {
    void SceneDriver::update(TimeMs deltaTime, const std::vector<ApplicationEvent> &events) {
        if (m_activeScene) {
            m_activeScene->update(deltaTime, events);
        }
    }

    void SceneDriver::render() {
        if (m_activeScene) {
            m_activeScene->render();
        }
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getActiveScene() const {
        return m_activeScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(size_t index, bool updateCmdSys) {
        if (index < m_scenes.size()) {
            m_activeScene = m_scenes.at(index);
            m_activeSceneIdx = index;
            if (updateCmdSys) {
                auto &cmdSystem = Pages::MainPage::getInstance()->getState().getCommandSystem();
                cmdSystem.setScene(m_activeScene.get());
            }
            BESS_INFO("[SceneDriver] Active scene set to index {}.", index);
            return m_activeScene;
        }
        return nullptr;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::createNewScene() {
        auto newScene = std::make_shared<Canvas::Scene>();
        m_scenes.emplace_back(newScene);
        return newScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getSceneAtIdx(size_t index) const {
        if (index < m_scenes.size()) {
            return m_scenes.at(index);
        }
        return nullptr;
    }

    void SceneDriver::addScene(const std::shared_ptr<Canvas::Scene> &scene) {
        m_scenes.emplace_back(scene);
    }

    size_t SceneDriver::getSceneCount() const {
        return m_scenes.size();
    }

    size_t SceneDriver::getActiveSceneIdx() const {
        return m_activeSceneIdx;
    }
} // namespace Bess
