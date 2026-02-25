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

        bool parentIsWasGroup = false;

        if (e.newParentUuid != UUID::null) {
            const auto &parentComp = m_sceneDriver->getState().getComponentByUuid(e.newParentUuid);
            parentIsWasGroup = parentComp->getType() == Canvas::SceneComponentType::group;
        }

        if (!parentIsWasGroup && e.prevParent != UUID::null) {
            const auto &prevParentComp = m_sceneDriver->getState().getComponentByUuid(e.prevParent);
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
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(parent, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));

        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &outSlotsInfo = digitalComp->definition->getOutputSlotsInfo();

        if (parent->getOutputSlotsCount() > outSlotsInfo.count) {
            for (size_t i = outSlotsInfo.count; i < parent->getOutputSlotsCount(); i++) {
                const auto slotUuid = parent->getOutputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);
                if (!slotComp->isResizeSlot()) {
                    // sceneState.removeComponent(slotUuid, UUID::master);
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
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &parent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compId);
        BESS_ASSERT(parent, std::format("[Scene] Component with uuid {} not found in scene state", (uint64_t)compId));
        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &inSlotsInfo = digitalComp->definition->getInputSlotsInfo();

        if (parent->getInputSlotsCount() > inSlotsInfo.count) {
            for (size_t i = inSlotsInfo.count; i < parent->getInputSlotsCount(); i++) {
                const auto slotUuid = parent->getInputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);
                // if (!slotComp->isResizeSlot())
                // sceneState.removeComponent(slotUuid, UUID::master);
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
} // namespace Bess::Pages
