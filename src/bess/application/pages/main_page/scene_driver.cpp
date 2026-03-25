#include "scene_driver.h"

#include "common/bess_uuid.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"
#include "ui_main/ui_main.h"
#include <algorithm>

namespace Bess {
    std::shared_ptr<Canvas::Scene> SceneDriver::getActiveScene() const {
        return m_activeScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(UUID id, bool updateCmdSys) {
        if (UUID::null == id || !m_sceneIdToSceneMap.contains(id)) {
            return nullptr;
        }

        if (m_activeScene) {
            m_activeScene->getState().clearSelectedComponents();
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

        if (!m_activeScene->getState().getIsRootScene()) {
            const auto &modId = m_activeScene->getState().getModuleId();
            if (modId != UUID::null &&
                !m_modIdToSceneMap.contains(modId)) {
                m_modIdToSceneMap[modId] = m_activeScene;
            }
        }

        UI::UIMain::getScenePanels().front()->setAttachedScene(m_activeScene);
        BESS_INFO("[SceneDriver] Active scene set to id {}.", (uint64_t)id);
        return m_activeScene;
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::setActiveScene(size_t index, bool updateCmdSys) {
        if (index < m_scenes.size()) {
            if (m_activeScene) {
                m_activeScene->getState().clearSelectedComponents();
            }
            m_activeScene = m_scenes.at(index);
            m_activeSceneIdx = index;
            if (updateCmdSys) {
                auto &cmdSystem = Pages::MainPage::getInstance()->getState().getCommandSystem();
                cmdSystem.setScene(m_activeScene.get());
            }

            if (!m_activeScene->getState().getIsRootScene()) {
                const auto &modId = m_activeScene->getState().getModuleId();
                if (modId != UUID::null &&
                    !m_modIdToSceneMap.contains(modId)) {
                    m_modIdToSceneMap[modId] = m_activeScene;
                }
            }

            UI::UIMain::getScenePanels().front()->setAttachedScene(m_activeScene);
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
        if (!scene) {
            return;
        }

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
        if (!m_sceneIdToSceneMap.contains(id)) {
            return;
        }

        const bool removingActiveScene = m_activeScene && m_activeScene->getSceneId() == id;

        const auto &scene = m_sceneIdToSceneMap.at(id);
        m_scenes.erase(std::ranges::remove_if(m_scenes,
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
            if (m_rootSceneId != UUID::null && m_sceneIdToSceneMap.contains(m_rootSceneId)) {
                setActiveScene(m_rootSceneId);
            } else {
                setActiveScene((size_t)0);
            }
            return;
        }

        if (m_activeScene) {
            auto activeIt = std::ranges::find_if(m_scenes,
                                                 [this](const std::shared_ptr<Canvas::Scene> &scene) {
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

    void SceneDriver::updateNets(const std::shared_ptr<Canvas::Scene> &scene) {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        if (!simEngine.isNetUpdated())
            return;

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &netIdToNameMap = mainPageState.getNetIdToNameMap();
        auto &netIdCompMap = mainPageState.getNetIdToCompMap(scene->getSceneId());
        auto &sceneState = scene->getState();

        std::unordered_map<UUID,
                           std::shared_ptr<Canvas::SimulationSceneComponent>>
            simIdToComp;

        for (const auto &[compId, comp] : sceneState.getAllComponents()) {
            if (comp->getType() == Canvas::SceneComponentType::simulation) {
                const auto simComp = comp->cast<Canvas::SimulationSceneComponent>();
                simIdToComp[simComp->getSimEngineId()] = simComp;
            }
        }

        const auto &nets = simEngine.getNetsMap();
        for (const auto &[netId, net] : nets) {
            for (const auto &simId : net.getComponents()) {
                if (simIdToComp.contains(simId)) {
                    const auto &comp = simIdToComp[simId];
                    netIdCompMap[netId].emplace_back(comp->getUuid());
                    comp->setNetId(netId);
                }
            }
        }
    }

    void SceneDriver::updateNets() {
        updateNets(m_activeScene);
    }

    void SceneDriver::makeRootSceneActive() {
        if (m_rootSceneId != UUID::null) {
            setActiveScene(m_rootSceneId);
        }
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getSceneWithId(const UUID &id) const {
        if (!m_sceneIdToSceneMap.contains(id)) {
            return nullptr;
        }

        return m_sceneIdToSceneMap.at(id);
    }

    void SceneDriver::removeScenes() {
        m_scenes.clear();
        m_sceneIdToSceneMap.clear();
        m_rootSceneId = UUID::null;
        m_activeScene = nullptr;
        m_activeSceneIdx = 0;
        m_modIdToSceneMap.clear();
    }

    std::shared_ptr<Canvas::Scene> SceneDriver::getSceneForModule(const UUID &modId) const {
        if (!m_modIdToSceneMap.contains(modId)) {
            return nullptr;
        }

        return m_modIdToSceneMap.at(modId);
    }
} // namespace Bess
