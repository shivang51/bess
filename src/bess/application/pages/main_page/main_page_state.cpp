#include "pages/main_page/main_page_state.h"

#include "bverilog/sim_engine_importer.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/cmds/update_value_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/verilog_scene_import.h"
#include "simulation_engine.h"
#include <cstdint>
#include <filesystem>
#include <optional>

namespace Bess::Pages {
    namespace {
        std::string fallbackSlotName(size_t index, bool isInput) {
            const char base = isInput ? 'A' : 'a';
            return std::string(1, static_cast<char>(base + (index % 26)));
        }

        std::shared_ptr<Canvas::Scene> getTrackedScene(SceneDriver &sceneDriver, const UUID &sceneId) {
            if (sceneId == UUID::null) {
                return nullptr;
            }

            return sceneDriver.getSceneWithId(sceneId);
        }

        void syncSceneComponentSlots(Canvas::SceneState &sceneState,
                                     const std::shared_ptr<Canvas::SimulationSceneComponent> &comp,
                                     const SimEngine::SlotsGroupInfo &slotsInfo,
                                     bool isInput) {
            if (!comp) {
                return;
            }

            auto &slotIds = isInput ? comp->getInputSlots() : comp->getOutputSlots();
            const auto slotType = isInput ? Canvas::SlotType::digitalInput : Canvas::SlotType::digitalOutput;
            const auto resizeSlotType = isInput ? Canvas::SlotType::inputsResize : Canvas::SlotType::outputsResize;

            std::vector<UUID> realSlots;
            realSlots.reserve(slotIds.size());
            UUID resizeSlotId = UUID::null;

            for (const auto &slotId : slotIds) {
                const auto slot = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotId);
                if (!slot) {
                    continue;
                }

                if (slot->getSlotType() == resizeSlotType || slot->isResizeSlot()) {
                    resizeSlotId = slotId;
                    continue;
                }

                realSlots.push_back(slotId);
            }

            while (realSlots.size() > slotsInfo.count) {
                const auto slotId = realSlots.back();
                realSlots.pop_back();
                comp->removeChildComponent(slotId);
                sceneState.removeComponent(slotId, UUID::master);
            }

            while (realSlots.size() < slotsInfo.count) {
                auto newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setSlotType(slotType);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(comp->getUuid(), newSlot->getUuid(), false);
                realSlots.push_back(newSlot->getUuid());
            }

            if (slotsInfo.isResizeable) {
                if (resizeSlotId == UUID::null) {
                    auto resizeSlot = std::make_shared<Canvas::SlotSceneComponent>();
                    resizeSlot->setSlotType(resizeSlotType);
                    resizeSlot->setIndex(-1);
                    sceneState.addComponent<Canvas::SlotSceneComponent>(resizeSlot);
                    sceneState.attachChild(comp->getUuid(), resizeSlot->getUuid(), false);
                    resizeSlotId = resizeSlot->getUuid();
                }
            } else if (resizeSlotId != UUID::null) {
                comp->removeChildComponent(resizeSlotId);
                sceneState.removeComponent(resizeSlotId, UUID::master);
                resizeSlotId = UUID::null;
            }

            slotIds.clear();
            slotIds.insert(slotIds.end(), realSlots.begin(), realSlots.end());
            if (resizeSlotId != UUID::null) {
                slotIds.push_back(resizeSlotId);
            }

            for (size_t i = 0; i < realSlots.size(); ++i) {
                const auto slot = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(realSlots[i]);
                if (!slot) {
                    continue;
                }

                slot->setSlotType(slotType);
                slot->setIndex(static_cast<int>(i));
                if (i < slotsInfo.names.size()) {
                    slot->setName(slotsInfo.names[i]);
                } else {
                    slot->setName(fallbackSlotName(i, isInput));
                }
            }

            if (resizeSlotId != UUID::null) {
                const auto resizeSlot = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(resizeSlotId);
                if (resizeSlot) {
                    resizeSlot->setSlotType(resizeSlotType);
                    resizeSlot->setIndex(-1);
                    resizeSlot->setName("");
                }
            }

            comp->setScaleDirty();
            comp->setSchematicScaleDirty();
        }

        std::vector<std::filesystem::path> toFilesystemPaths(const std::vector<std::string> &paths) {
            std::vector<std::filesystem::path> result;
            result.reserve(paths.size());
            for (const auto &path : paths) {
                result.emplace_back(path);
            }
            return result;
        }

        std::string primaryImportPath(const std::vector<std::string> &paths) {
            if (paths.empty()) {
                return {};
            }
            return paths.front();
        }
    } // namespace

    struct MainPageState::VerilogImportSession {
        enum class Phase : uint8_t {
            resetProject,
            importSimulation,
            createScene,
            updateNets,
            completed,
            failed,
        };

        std::vector<std::string> paths;
        float progress = 0.f;
        std::string stageMessage = "Select a Verilog file";
        bool importing = false;
        bool finished = false;
        bool failed = false;
        Phase phase = Phase::resetProject;
        std::optional<Verilog::SimEngineImportResult> stagedResult;
    };

    MainPageState::MainPageState() = default;
    MainPageState::~MainPageState() = default;

    void MainPageState::resetProjectState() const {
        m_sceneDriver.getActiveScene()->clear();
        SimEngine::SimulationEngine::instance().clear();
    }

    void MainPageState::createNewProject(bool updateWindowName) {
        resetProjectState();
        m_currentProjectFile = std::make_shared<ProjectFile>();
        if (!updateWindowName)
            return;
        const auto win = MainPage::getInstance()->getParentWindow();
        win->setName(m_currentProjectFile->getName());
    }

    void MainPageState::loadProject(const std::string &path) {
        resetProjectState();
        const auto project = std::make_shared<ProjectFile>(path);
        updateCurrentProject(project);
    }

    void MainPageState::saveCurrentProject() const {
        m_currentProjectFile->save();
    }

    void MainPageState::updateCurrentProject(const std::shared_ptr<ProjectFile> &project) {
        if (project == nullptr)
            return;
        m_currentProjectFile = project;
        const auto win = MainPage::getInstance()->getParentWindow();
        win->setName(m_currentProjectFile->getName() + " - BESS");
    }

    bool MainPageState::importVerilogFile(const std::string &path, std::string *errorMessage) {
        return importVerilogFiles(std::vector<std::string>{path}, errorMessage);
    }

    bool MainPageState::importVerilogFiles(const std::vector<std::string> &paths, std::string *errorMessage) {
        try {
            if (paths.empty()) {
                if (errorMessage) {
                    *errorMessage = "No Verilog files were selected";
                }
                return false;
            }

            auto scene = m_sceneDriver.getActiveScene();
            if (!scene) {
                if (errorMessage) {
                    *errorMessage = "No active scene available";
                }
                return false;
            }

            resetProjectState();
            auto &simEngine = SimEngine::SimulationEngine::instance();
            const auto result = Verilog::importVerilogFilesIntoSimulationEngine(toFilesystemPaths(paths), simEngine);
            populateSceneFromVerilogImportResult(result, simEngine, *scene);
            m_sceneDriver.updateNets(scene);
            return true;
        } catch (const std::exception &ex) {
            if (errorMessage) {
                *errorMessage = ex.what();
            }
            BESS_ERROR("[MainPageState] Failed to import Verilog files (primary {}): {}",
                       primaryImportPath(paths),
                       ex.what());
            return false;
        }
    }

    HierarchicalSceneLayoutResult MainPageState::applyHierarchicalLayoutToActiveScene() {
        HierarchicalSceneLayoutResult result;
        const auto activeScene = m_sceneDriver.getActiveScene();
        if (!activeScene) {
            return result;
        }

        return applyHierarchicalSceneLayout(*activeScene, SimEngine::SimulationEngine::instance());
    }

    void MainPageState::startVerilogImport(const std::string &path) {
        startVerilogImport(std::vector<std::string>{path});
    }

    void MainPageState::startVerilogImport(const std::vector<std::string> &paths) {
        m_verilogImportSession = std::make_unique<VerilogImportSession>();
        m_verilogImportSession->paths = paths;
        m_verilogImportSession->progress = 0.05f;
        m_verilogImportSession->stageMessage = "Clearing current project";
        m_verilogImportSession->importing = true;
        m_verilogImportSession->phase = VerilogImportSession::Phase::resetProject;
    }

    VerilogImportStatus MainPageState::advanceVerilogImport(std::string *errorMessage) {
        VerilogImportStatus status;
        if (!m_verilogImportSession) {
            status.stageMessage = "No active import";
            status.finished = true;
            status.failed = true;
            return status;
        }

        auto &session = *m_verilogImportSession;
        status.progress = session.progress;
        status.stageMessage = session.stageMessage;
        status.importing = session.importing;
        status.finished = session.finished;
        status.failed = session.failed;

        if (!session.importing || session.finished) {
            return status;
        }

        try {
            auto &simEngine = SimEngine::SimulationEngine::instance();
            auto scene = m_sceneDriver.getActiveScene();

            switch (session.phase) {
            case VerilogImportSession::Phase::resetProject:
                resetProjectState();
                session.progress = 0.2f;
                session.stageMessage = "Converting Verilog to simulation graph";
                session.phase = VerilogImportSession::Phase::importSimulation;
                break;
            case VerilogImportSession::Phase::importSimulation:
                session.stagedResult = Verilog::importVerilogFilesIntoSimulationEngine(
                    toFilesystemPaths(session.paths),
                    simEngine);
                session.progress = 0.7f;
                session.stageMessage = "Creating scene components";
                session.phase = VerilogImportSession::Phase::createScene;
                break;
            case VerilogImportSession::Phase::createScene:
                if (!scene) {
                    throw std::runtime_error("No active scene available");
                }
                populateSceneFromVerilogImportResult(*session.stagedResult, simEngine, *scene);
                session.progress = 0.92f;
                session.stageMessage = "Updating nets";
                session.phase = VerilogImportSession::Phase::updateNets;
                break;
            case VerilogImportSession::Phase::updateNets:
                if (!scene) {
                    throw std::runtime_error("No active scene available");
                }
                m_sceneDriver.updateNets(scene);
                session.progress = 1.f;
                session.stageMessage = "Import complete";
                session.phase = VerilogImportSession::Phase::completed;
                session.importing = false;
                session.finished = true;
                session.failed = false;
                session.stagedResult.reset();
                break;
            case VerilogImportSession::Phase::completed:
            case VerilogImportSession::Phase::failed:
                break;
            }
        } catch (const std::exception &ex) {
            session.importing = false;
            session.finished = true;
            session.failed = true;
            session.phase = VerilogImportSession::Phase::failed;
            session.progress = 1.f;
            session.stageMessage = std::format("Import failed: {}", ex.what());
            session.stagedResult.reset();
            if (errorMessage) {
                *errorMessage = ex.what();
            }
        }

        status.progress = session.progress;
        status.stageMessage = session.stageMessage;
        status.importing = session.importing;
        status.finished = session.finished;
        status.failed = session.failed;
        return status;
    }

    void MainPageState::cancelVerilogImport() {
        m_verilogImportSession.reset();
    }

    std::shared_ptr<ProjectFile> MainPageState::getCurrentProjectFile() const {
        return m_currentProjectFile;
    }

    bool MainPageState::isKeyPressed(int key) const {
        return m_pressedKeysFrame.contains(key) && m_pressedKeysFrame.at(key);
    }

    void MainPageState::setKeyPressed(int key) {
        m_pressedKeysFrame[key] = true;
    }

    bool MainPageState::isKeyReleased(int key) const {
        return m_releasedKeysFrame.contains(key) && m_releasedKeysFrame.at(key);
    }

    void MainPageState::setKeyReleased(int key) {
        m_releasedKeysFrame[key] = true;
    }

    bool MainPageState::isKeyDown(int key) const {
        return m_downKeys.contains(key) && m_downKeys.at(key);
    }

    void MainPageState::setKeyDown(int key, bool isDown) {
        m_downKeys[key] = isDown;
    }

    void MainPageState::initCmdSystem() {
        m_commandSystem.init();
        auto &dispatcher = EventSystem::EventDispatcher::instance();
        dispatcher.sink<Canvas::Events::EntityMovedEvent>().connect<&MainPageState::onEntityMoved>(this);
        dispatcher.sink<Canvas::Events::EntityReparentedEvent>().connect<&MainPageState::onEntityReparented>(this);
        dispatcher.sink<Canvas::Events::ComponentAddedEvent>().connect<&MainPageState::onEntityAdded>(this);
        dispatcher.sink<Canvas::Events::ComponentRemovedEvent>().connect<&MainPageState::onEntityRemoved>(this);
        dispatcher.sink<SimEngine::Events::CompDefOutputsResizedEvent>().connect<&MainPageState::onCompDefOutputsResized>(this);
        dispatcher.sink<SimEngine::Events::CompDefInputsResizedEvent>().connect<&MainPageState::onCompDefInputsResized>(this);
    }

    void MainPageState::onEntityMoved(const Canvas::Events::EntityMovedEvent &e) {
        auto entity = m_sceneDriver->getState().getComponentByUuid(e.entityUuid);
        if (!entity) {
            return;
        }

        glm::vec3 *posPtr = &entity->getTransform().position;
        auto cmd = std::make_unique<Cmd::UpdateValCommand<glm::vec3>>(posPtr, e.newPos, e.oldPos);
        m_commandSystem.push(std::move(cmd));
    }

    void MainPageState::onEntityReparented(const Canvas::Events::EntityReparentedEvent &e) {
        const auto scene = getTrackedScene(m_sceneDriver, e.sceneId);
        if (!scene) {
            return;
        }

        auto &sceneState = scene->getState();
        auto entity = sceneState.getComponentByUuid(e.entityUuid);
        if (!entity) {
            return;
        }

        if (entity->getType() == Canvas::SceneComponentType::slot) {
            return;
        }

        UUID *parentPtr = &entity->getParentComponent();

        bool parentIsWasGroup = false;

        if (e.newParentUuid != UUID::null) {
            const auto &parentComp = sceneState.getComponentByUuid(e.newParentUuid);
            if (!parentComp) {
                return;
            }
            parentIsWasGroup = parentComp->getType() == Canvas::SceneComponentType::group;
        }

        if (!parentIsWasGroup && e.prevParent != UUID::null) {
            const auto &prevParentComp = sceneState.getComponentByUuid(e.prevParent);
            if (!prevParentComp)
                return;
            if (prevParentComp->getType() == Canvas::SceneComponentType::group) {
                parentIsWasGroup = true;
            }
        }

        // ignore if parent is/was a group
        // group handels this shit it self
        if (parentIsWasGroup) {
            return;
        }

        /// undo redo callback
        const auto callback = [this, entityUuid = e.entityUuid, sceneId = e.sceneId](bool isUndo, const UUID &newParent) {
            (void)isUndo;
            if (newParent == UUID::null)
                return;

            const auto scene = getTrackedScene(m_sceneDriver, sceneId);
            if (!scene) {
                return;
            }

            const auto &parent = scene->getState().getComponentByUuid(newParent);
            if (!parent) {
                return;
            }
            parent->addChildComponent(entityUuid);
        };

        if (entity) {
            auto cmd = std::make_unique<Cmd::UpdateValCommand<UUID>>(parentPtr,
                                                                     e.newParentUuid,
                                                                     e.prevParent,
                                                                     callback);
            m_commandSystem.push(std::move(cmd));
        }
    }

    const SceneDriver &MainPageState::getSceneDriver() const {
        return m_sceneDriver;
    }

    SceneDriver &MainPageState::getSceneDriver() {
        return m_sceneDriver;
    }

    Cmd::CommandSystem &MainPageState::getCommandSystem() {
        return m_commandSystem;
    }

    void MainPageState::update() {
        m_releasedKeysFrame.clear();
        m_pressedKeysFrame.clear();
    }

    void MainPageState::onCompDefOutputsResized(const SimEngine::Events::CompDefOutputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_TRACE("Ignoring CompDefOutputsResizedEvent for unknown componentId: {}", (uint64_t)e.componentId);
            return;
        }

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        const auto scene = m_sceneDriver.getSceneWithId(compData.sceneId);
        if (!scene) {
            m_simIdToSceneCompId.erase(e.componentId);
            return;
        }

        auto &sceneState = scene->getState();
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);

        // FIXME
        // const auto &outSlotsInfo = digitalComp->definition->getOutputSlotsInfo();
        // syncSceneComponentSlots(sceneState, comp, outSlotsInfo, false);
    }

    void MainPageState::onCompDefInputsResized(const SimEngine::Events::CompDefInputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_TRACE("Ignoring CompDefInputsResizedEvent for unknown componentId: {}", (uint64_t)e.componentId);
            return;
        }

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        const auto scene = m_sceneDriver.getSceneWithId(compData.sceneId);
        if (!scene) {
            m_simIdToSceneCompId.erase(e.componentId);
            return;
        }

        auto &sceneState = scene->getState();
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);

        // FIXME
        // const auto &inSlotsInfo = digitalComp->definition->getInputSlotsInfo();
        // syncSceneComponentSlots(sceneState, comp, inSlotsInfo, true);
    }

    void MainPageState::onEntityAdded(const Canvas::Events::ComponentAddedEvent &e) {
        const auto scene = getTrackedScene(m_sceneDriver, e.sceneId);
        if (!scene) {
            return;
        }

        const auto &sceneState = scene->getState();
        const auto &comp = sceneState.getComponentByUuid(e.uuid);
        if (!comp) {
            return;
        }

        if (e.type == Canvas::SceneComponentType::simulation ||
            e.type == Canvas::SceneComponentType::module) {
            const auto simComp = std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(comp);
            if (!simComp) {
                return;
            }
            const auto &simEngineId = simComp->getSimEngineId();
            m_simIdToSceneCompId[simEngineId] = {e.uuid, e.sceneId};
        }
    }

    void MainPageState::onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e) {
        if (e.type != Canvas::SceneComponentType::simulation &&
            e.type != Canvas::SceneComponentType::module)
            return;

        auto it = std::ranges::find_if(m_simIdToSceneCompId,
                                       [&e](const auto &pair) {
                                           return pair.second.sceneCompId == e.uuid &&
                                                  pair.second.sceneId == e.sceneId;
                                       });

        if (it != m_simIdToSceneCompId.end()) {
            m_simIdToSceneCompId.erase(it);
        }
    }

    MainPageState::TNetIdToCompMap &MainPageState::getNetIdToCompMap(UUID sceneId) {
        if (!m_netIdToCompMap.contains(sceneId)) {
            m_netIdToCompMap[sceneId] = TNetIdToCompMap{};
        }

        return m_netIdToCompMap.at(sceneId);
    }
} // namespace Bess::Pages
