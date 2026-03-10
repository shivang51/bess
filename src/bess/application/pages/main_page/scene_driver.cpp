#include "scene_driver.h"

#include "pages/main_page/main_page.h"
#include <algorithm>

namespace Bess {
    void SceneDriver::update(TimeMs deltaTime, const std::vector<ApplicationEvent> &events) {
        for (auto &scene : m_scenes) {
            scene->update(deltaTime, events);
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

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(UUID id, bool updateCmdSys) {
        if (UUID::null == id || !m_sceneIdToSceneMap.contains(id)) {
            return nullptr;
        }

        m_activeScene = m_sceneIdToSceneMap.at(id);
        m_activeSceneIdx = std::distance(m_scenes.begin(), std::ranges::find_if(m_scenes,
                                                                                [&](const std::shared_ptr<Canvas::Scene> &scene) {
                                                                                    return scene->getSceneId() == id;
                                                                                }));

        if (updateCmdSys) {
            auto &cmdSystem = Pages::MainPage::getInstance()->getState().getCommandSystem();
            cmdSystem.setScene(m_activeScene.get());
        }

        BESS_INFO("[SceneDriver] Active scene set to id {}.", (uint64_t)id);
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
        m_sceneIdToSceneMap[newScene->getSceneId()] = newScene;
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
        m_sceneIdToSceneMap[scene->getSceneId()] = scene;
    }

    size_t SceneDriver::getSceneCount() const {
        return m_scenes.size();
    }

    size_t SceneDriver::getActiveSceneIdx() const {
        return m_activeSceneIdx;
    }
} // namespace Bess
