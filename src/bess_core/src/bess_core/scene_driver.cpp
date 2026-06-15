#include "bess_core/scene_driver.h"

#include "common/bess_uuid.h"
#include "ui_main/ui_main.h"

#include <algorithm>
#include <mutex>

namespace Bess {
    void SceneDriver::onInit() {
    }

    void SceneDriver::onShutdown() {
        removeScenes();
    }

    void SceneDriver::onDestroy() {
    }

    void SceneDriver::onUpdate(TimeMs ts) {
        for (auto &scene : m_scenes) {
            if (scene) {
                scene->update(ts, scene == m_activeScene);
            }
        }
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getActiveScene() const {
        return m_activeScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(UUID id) {

        if (UUID::null == id || !m_sceneIdToSceneMap.contains(id)) {
            return nullptr;
        }

        std::unique_lock lock(m_scenesMutex);
        if (m_activeScene) {
            m_activeScene->getState().clearSelectedComponents();
        }

        m_activeScene = m_sceneIdToSceneMap.at(id);
        m_activeSceneIdx = std::distance(
            m_scenes.begin(),
            std::ranges::find_if(
                m_scenes, [&](const std::shared_ptr<Canvas::Scene> &scene) {
                    return scene->getSceneId() == id;
                }));
        lock.unlock();

        if (!m_activeScene->getState().getIsRootScene()) {
            const auto &modId = m_activeScene->getState().getModuleId();
            if (modId != UUID::null && !m_modIdToSceneMap.contains(modId)) {
                lock.lock();
                m_modIdToSceneMap[modId] = m_activeScene;
                lock.unlock();
            }
        }

        if (!UI::UIMain::getScenePanels().empty() &&
            UI::UIMain::getScenePanels().front()) {
            UI::UIMain::getScenePanels().front()->setAttachedScene(
                m_activeScene);
        }
#ifdef DEBUG
        else {
            BESS_WARN(
                "[SceneDriver] No scene panel available to attach the active "
                "scene to.");
        }
#endif
        BESS_INFO("[SceneDriver] Active scene set to id {}.", (uint64_t)id);
        return m_activeScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(size_t index) {
        std::unique_lock lock(m_scenesMutex);
        if (index < m_scenes.size()) {
            if (m_activeScene) {
                m_activeScene->getState().clearSelectedComponents();
            }
            m_activeScene = m_scenes.at(index);
            m_activeSceneIdx = index;

            if (!m_activeScene->getState().getIsRootScene()) {
                const auto &modId = m_activeScene->getState().getModuleId();
                if (modId != UUID::null && !m_modIdToSceneMap.contains(modId)) {
                    m_modIdToSceneMap[modId] = m_activeScene;
                }
            }

            lock.unlock();
            if (!UI::UIMain::getScenePanels().empty() &&
                UI::UIMain::getScenePanels().front()) {
                UI::UIMain::getScenePanels().front()->setAttachedScene(
                    m_activeScene);
            }
            BESS_INFO("[SceneDriver] Active scene set to index {}.", index);
            return m_activeScene;
        }
        return nullptr;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::createNewScene() {
        std::lock_guard lock(m_scenesMutex);
        auto newScene = std::make_shared<Canvas::Scene>();
        m_scenes.emplace_back(newScene);
        m_sceneIdToSceneMap[newScene->getSceneId()] = newScene;
        return newScene;
    }

    std::shared_ptr<Canvas::Scene>
    SceneDriver::getSceneAtIdx(size_t index) const {
        std::lock_guard lock(m_scenesMutex);
        if (index < m_scenes.size()) {
            return m_scenes.at(index);
        }
        return nullptr;
    }

    void SceneDriver::addScene(const std::shared_ptr<Canvas::Scene> &scene) {
        if (!scene) {
            return;
        }

        std::lock_guard lock(m_scenesMutex);
        if (m_sceneIdToSceneMap.contains(scene->getSceneId())) {
            return;
        }

        m_scenes.emplace_back(scene);
        m_sceneIdToSceneMap[scene->getSceneId()] = scene;

        if (!scene->getState().getIsRootScene()) {
            const auto &modId = scene->getState().getModuleId();
            m_modIdToSceneMap[modId] = scene;
        }
    }

    void SceneDriver::removeScene(const UUID &id) {
        std::unique_lock lock(m_scenesMutex);
        if (!m_sceneIdToSceneMap.contains(id)) {
            return;
        }

        const bool removingActiveScene =
            m_activeScene && m_activeScene->getSceneId() == id;

        const auto &scene = m_sceneIdToSceneMap.at(id);
        m_scenes.erase(std::ranges::remove_if(
                           m_scenes,
                           [&id](const std::shared_ptr<Canvas::Scene> &scene) {
                               return scene && scene->getSceneId() == id;
                           })
                           .begin(),
                       m_scenes.end());
        m_sceneIdToSceneMap.erase(id);

        if (!scene->getState().getIsRootScene()) {
            const auto &modId = scene->getState().getModuleId();
            if (m_modIdToSceneMap.contains(modId)) {
                m_modIdToSceneMap.erase(modId);
            }
        }

        if (m_scenes.empty()) {
            m_activeScene = nullptr;
            m_activeSceneIdx = 0;
            return;
        }

        if (removingActiveScene) {
            lock.unlock();
            if (m_rootSceneId != UUID::null &&
                m_sceneIdToSceneMap.contains(m_rootSceneId)) {
                setActiveScene(m_rootSceneId);
            } else {
                setActiveScene((size_t)0);
            }
            return;
        }

        if (m_activeScene) {
            auto activeIt = std::ranges::find_if(
                m_scenes, [this](const std::shared_ptr<Canvas::Scene> &scene) {
                    return scene && m_activeScene &&
                           scene->getSceneId() == m_activeScene->getSceneId();
                });
            if (activeIt != m_scenes.end()) {
                m_activeSceneIdx = std::distance(m_scenes.begin(), activeIt);
            }
        }
    }

    size_t SceneDriver::getSceneCount() const {
        return m_scenes.size();
    }

    size_t SceneDriver::getActiveSceneIdx() const {
        return m_activeSceneIdx;
    }

    void SceneDriver::makeRootSceneActive() {
        if (m_rootSceneId != UUID::null) {
            setActiveScene(m_rootSceneId);
        }
    }

    std::shared_ptr<Canvas::Scene>
    SceneDriver::getSceneWithId(const UUID &id) const {
        std::lock_guard lock(m_scenesMutex);
        if (!m_sceneIdToSceneMap.contains(id)) {
            return nullptr;
        }

        return m_sceneIdToSceneMap.at(id);
    }

    void SceneDriver::removeScenes() {
        std::lock_guard lock(m_scenesMutex);
        for (const auto &scene : m_scenes) {
            scene->destroy();
        }
        m_sceneIdToSceneMap.clear();
        m_rootSceneId = UUID::null;
        m_activeScene = nullptr;
        m_activeSceneIdx = 0;
        m_modIdToSceneMap.clear();
    }

    void SceneDriver::reset() {
        removeScenes();
        auto scene = createNewScene();
        setRootSceneId(scene->getSceneId());
        setActiveScene(scene->getSceneId());
    }

    std::shared_ptr<Canvas::Scene>
    SceneDriver::getSceneForModule(const UUID &modId) const {
        std::lock_guard lock(m_scenesMutex);
        if (!m_modIdToSceneMap.contains(modId)) {
            return nullptr;
        }

        return m_modIdToSceneMap.at(modId);
    }
} // namespace Bess
