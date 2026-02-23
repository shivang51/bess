#include "pages/main_page/main_page_state.h"

#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/cmds/update_value_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include <algorithm>
#include <cstdint>

namespace Bess::Pages {

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

    void MainPageState::initCmdSystem(Canvas::Scene *scene,
                                      SimEngine::SimulationEngine *simEngine) {
        m_commandSystem.init(scene, simEngine);
        auto &dispatcher = EventSystem::EventDispatcher::instance();
        dispatcher.sink<Canvas::Events::EntityMovedEvent>().connect<&MainPageState::onEntityMoved>(this);
        dispatcher.sink<Canvas::Events::EntityReparentedEvent>().connect<&MainPageState::onEntityReparented>(this);
        dispatcher.sink<Canvas::Events::ComponentAddedEvent>().connect<&MainPageState::onEntityAdded>(this);
        dispatcher.sink<Canvas::Events::ComponentRemovedEvent>().connect<&MainPageState::onEntityRemoved>(this);
        dispatcher.sink<Canvas::Events::ConnectionRemovedEvent>().connect<&MainPageState::onConnectionRemoved>(this);
        dispatcher.sink<SimEngine::Events::CompDefOutputsResizedEvent>().connect<&MainPageState::onCompDefOutputsResized>(this);
        dispatcher.sink<SimEngine::Events::CompDefInputsResizedEvent>().connect<&MainPageState::onCompDefInputsResized>(this);
    }

    void MainPageState::onEntityMoved(const Canvas::Events::EntityMovedEvent &e) {
        auto entity = m_sceneDriver->getState().getComponentByUuid(e.entityUuid);
        glm::vec3 *posPtr = &entity->getTransform().position;

        if (entity) {
            auto cmd = std::make_unique<Cmd::UpdateValCommand<glm::vec3>>(posPtr, e.newPos, e.oldPos);
            m_commandSystem.push(std::move(cmd));
        }
    }

    void MainPageState::onEntityReparented(const Canvas::Events::EntityReparentedEvent &e) {
        auto entity = m_sceneDriver->getState().getComponentByUuid(e.entityUuid);

        if (entity->getType() == Canvas::SceneComponentType::slot) {
            return;
        }

        UUID *parentPtr = &entity->getParentComponent();

        /// undo redo callback
        const auto callback = [this, entityUuid = e.entityUuid](bool isUndo, const UUID &newParent) {
            if (newParent == UUID::null)
                return;

            const auto &parent = m_sceneDriver->getState().getComponentByUuid(newParent);
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
            BESS_ERROR("Received CompDefOutputsResizedEvent for unknown componentId: {}", (uint64_t)e.componentId);
            return;
        }

        auto &sceneState = m_sceneDriver->getState();
        const auto &compId = m_simIdToSceneCompId[e.componentId];
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(comp, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));
        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(parent, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));
        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &outSlotsInfo = digitalComp->definition->getOutputSlotsInfo();

        if (parent->getOutputSlotsCount() > outSlotsInfo.count) {
            for (size_t i = outSlotsInfo.count; i < parent->getOutputSlotsCount(); i++) {
                const auto slotUuid = parent->getOutputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);
                if (!slotComp->isResizeSlot()) {
                    sceneState.removeComponent(slotUuid, UUID::master);
                }
            }
            parent->setScaleDirty();
        } else if (parent->getOutputSlotsCount() < outSlotsInfo.count) {
            for (size_t i = parent->getOutputSlotsCount(); i < outSlotsInfo.count; i++) {
                std::shared_ptr<Canvas::SlotSceneComponent> newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setSlotType(Canvas::SlotType::digitalOutput);
                newSlot->setIndex((int)outSlotsInfo.count - 1);
                if (i < outSlotsInfo.names.size()) {
                    newSlot->setName(std::string(1, (char)('a' + i)));
                }
                parent->addOutputSlot(newSlot->getUuid(), outSlotsInfo.isResizeable);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(parent->getUuid(), newSlot->getUuid());
            }
        } else {
            BESS_WARN("CompDefOutputsResizedEvent received without any change");
        }
    }

    void MainPageState::onCompDefInputsResized(const SimEngine::Events::CompDefInputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_ERROR("Received CompDefInputsResizedEvent for unknown componentId: {}", (uint64_t)e.componentId);
            return;
        }

        auto &sceneState = m_sceneDriver->getState();
        const auto &compId = m_simIdToSceneCompId[e.componentId];
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(comp, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));
        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(parent, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));
        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &inSlotsInfo = digitalComp->definition->getInputSlotsInfo();

        if (parent->getInputSlotsCount() > inSlotsInfo.count) {
            for (size_t i = inSlotsInfo.count; i < parent->getInputSlotsCount(); i++) {
                const auto slotUuid = parent->getInputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);
                if (!slotComp->isResizeSlot())
                    sceneState.removeComponent(slotUuid, UUID::master);
            }
            parent->setScaleDirty();
        } else if (parent->getInputSlotsCount() < inSlotsInfo.count) {
            for (size_t i = parent->getInputSlotsCount(); i < inSlotsInfo.count; i++) {
                std::shared_ptr<Canvas::SlotSceneComponent> newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setSlotType(Canvas::SlotType::digitalInput);
                newSlot->setIndex((int)inSlotsInfo.count - 1);
                if (i < inSlotsInfo.names.size()) {
                    newSlot->setName(std::string(1, (char)('A' + i)));
                }
                parent->addInputSlot(newSlot->getUuid(), inSlotsInfo.isResizeable);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(parent->getUuid(), newSlot->getUuid());
            }
        } else {
            BESS_WARN("CompDefInputsResizedEvent received without any change");
        }
    }

    void MainPageState::onEntityAdded(const Canvas::Events::ComponentAddedEvent &e) {
        auto &sceneState = m_sceneDriver->getState();

        if (e.type != Canvas::SceneComponentType::simulation)
            return;

        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(e.uuid);
        const auto &simEngineId = comp->getSimEngineId();
        m_simIdToSceneCompId[simEngineId] = e.uuid;
    }

    void MainPageState::onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e) {
        if (e.type != Canvas::SceneComponentType::simulation)
            return;

        auto it = std::ranges::find_if(m_simIdToSceneCompId,
                                       [&e](const auto &pair) { return pair.second == e.uuid; });

        if (it != m_simIdToSceneCompId.end()) {
            m_simIdToSceneCompId.erase(it);
        }
    }

    void MainPageState::onConnectionRemoved(const Canvas::Events::ConnectionRemovedEvent &e) {

        auto &sceneState = m_sceneDriver->getState();
        const auto &slotCompA = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(e.slotAId);
        const auto &slotCompB = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(e.slotBId);

        auto processSlot = [&](const std::shared_ptr<Canvas::SlotSceneComponent> &slotComp) {
            const auto &parentComp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(slotComp->getParentComponent());
            BESS_ASSERT(parentComp, std::format("[Scene] Parent component with uuid {} not found for slot {}",
                                                (uint64_t)slotComp->getParentComponent(),
                                                (uint64_t)slotComp->getUuid()));
            const auto &simEngineId = parentComp->getSimEngineId();
            auto &simEngine = SimEngine::SimulationEngine::instance();

            const auto &digitalComp = simEngine.getDigitalComponent(simEngineId);
            BESS_ASSERT(digitalComp, std::format("[Scene] Digital component with simEngineId {} not found",
                                                 (uint64_t)simEngineId));

            const auto &def = digitalComp->definition;
            BESS_ASSERT(def, std::format("[Scene] Component definition not found for component with simEngineId {}",
                                         (uint64_t)simEngineId));

            const bool isInputSlot = (slotComp->getSlotType() == Canvas::SlotType::digitalInput);
            const auto &slotsInfo = isInputSlot
                                        ? def->getInputSlotsInfo()
                                        : def->getOutputSlotsInfo();

            if (slotsInfo.isResizeable) {
                const auto idx = slotComp->getIndex();
                if (idx == 0)
                    return; // do not remove first slot

                if (isInputSlot) {
                    const bool isConnected = !digitalComp->inputConnections[idx].empty();
                    // if not last slot or still connected, then abort !!! :)
                    if (isConnected ||
                        idx != def->getInputSlotsInfo().count - 1)
                        return;

                    digitalComp->decrementInputCount();
                } else {
                    const bool isConnected = !digitalComp->outputConnections[idx].empty();
                    // if not last slot or still connected, then return
                    if (isConnected || idx != def->getOutputSlotsInfo().count - 1)
                        return;

                    digitalComp->decrementOutputCount();
                }
            }
        };

        // can be a joint as well so checking for slot explicitly
        if (slotCompA && slotCompA->getType() == Canvas::SceneComponentType::slot)
            processSlot(slotCompA);
        if (slotCompB && slotCompB->getType() == Canvas::SceneComponentType::slot)
            processSlot(slotCompB);
    }
} // namespace Bess::Pages
