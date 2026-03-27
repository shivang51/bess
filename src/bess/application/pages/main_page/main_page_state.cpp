#include "pages/main_page/main_page_state.h"

#include "bverilog/sim_engine_importer.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/cmds/update_value_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include <algorithm>
#include <deque>
#include <cstdint>
#include <unordered_set>

namespace Bess::Pages {
    namespace {
        using namespace Bess::Canvas;
        using namespace Bess::SimEngine;
        using namespace Bess::Verilog;

        struct ImportedSceneComponent {
            std::shared_ptr<SimulationSceneComponent> component;
            std::vector<std::shared_ptr<SceneComponent>> children;
        };

        ImportedSceneComponent createSceneComponentForImportedSimId(const UUID &simId) {
            auto &simEngine = SimulationEngine::instance();
            const auto &compDef = simEngine.getComponentDefinition(simId);
            auto created = SimulationSceneComponent::createNewAndRegister(compDef);
            BESS_ASSERT(!created.empty(), "Failed to create scene component for imported sim component");

            ImportedSceneComponent result;
            result.component = created.front()->cast<SimulationSceneComponent>();
            BESS_ASSERT(result.component, "Imported scene root is not a simulation scene component");
            result.component->setSimEngineId(simId);
            result.component->setName(compDef->getName());
            created.erase(created.begin());
            result.children = std::move(created);
            return result;
        }

        void addImportedSceneComponent(SceneState &sceneState,
                                       ImportedSceneComponent &importedComp) {
            sceneState.addComponent(importedComp.component);
            for (const auto &child : importedComp.children) {
                sceneState.addComponent(child);
                sceneState.attachChild(importedComp.component->getUuid(), child->getUuid(), false);
            }
        }

        std::unordered_map<UUID, int> computeImportedLevels(const SimEngineImportResult &result) {
            std::unordered_set<UUID> importedIds(result.createdComponentIds.begin(),
                                                 result.createdComponentIds.end());
            std::unordered_map<UUID, int> indegree;
            std::unordered_map<UUID, std::vector<UUID>> adjacency;

            for (const auto &id : result.createdComponentIds) {
                indegree[id] = 0;
            }

            auto &simEngine = SimulationEngine::instance();
            for (const auto &id : result.createdComponentIds) {
                const auto connections = simEngine.getConnections(id);
                for (const auto &slotConnections : connections.outputs) {
                    for (const auto &[dstId, dstSlot] : slotConnections) {
                        (void)dstSlot;
                        if (!importedIds.contains(dstId)) {
                            continue;
                        }
                        adjacency[id].push_back(dstId);
                        indegree[dstId] += 1;
                    }
                }
            }

            std::deque<UUID> queue;
            std::unordered_map<UUID, int> levels;
            for (const auto &[id, deg] : indegree) {
                if (deg == 0) {
                    queue.push_back(id);
                    levels[id] = 0;
                }
            }

            while (!queue.empty()) {
                const auto current = queue.front();
                queue.pop_front();
                for (const auto &next : adjacency[current]) {
                    levels[next] = std::max(levels[next], levels[current] + 1);
                    indegree[next] -= 1;
                    if (indegree[next] == 0) {
                        queue.push_back(next);
                    }
                }
            }

            for (const auto &id : result.createdComponentIds) {
                if (!levels.contains(id)) {
                    levels[id] = 0;
                }
            }

            return levels;
        }

        void layoutImportedComponents(const SimEngineImportResult &result,
                                      const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId,
                                      Scene &scene) {
            auto levels = computeImportedLevels(result);
            std::unordered_map<int, std::vector<UUID>> levelBuckets;
            int maxLevel = 0;
            for (const auto &[id, level] : levels) {
                levelBuckets[level].push_back(id);
                maxLevel = std::max(maxLevel, level);
            }

            const float xSpacing = 220.f;
            const float ySpacing = 140.f;
            const float centerX = (static_cast<float>(maxLevel) * xSpacing) / 2.f;
            for (auto &[level, ids] : levelBuckets) {
                std::ranges::sort(ids, [&](const UUID &a, const UUID &b) {
                    const auto &defA = SimulationEngine::instance().getComponentDefinition(a);
                    const auto &defB = SimulationEngine::instance().getComponentDefinition(b);
                    return defA->getName() < defB->getName();
                });

                const float totalHeight = (static_cast<float>(ids.size() - 1) * ySpacing);
                for (size_t i = 0; i < ids.size(); ++i) {
                    const auto it = sceneBySimId.find(ids[i]);
                    if (it == sceneBySimId.end()) {
                        continue;
                    }
                    auto &transform = it->second->getTransform();
                    transform.position = {
                        static_cast<float>(level) * xSpacing - centerX,
                        static_cast<float>(i) * ySpacing - (totalHeight / 2.f),
                        scene.getNextZCoord(),
                    };
                    it->second->setSchematicTransform(transform);
                    it->second->setScaleDirty();
                    it->second->setSchematicScaleDirty();
                }
            }
        }

        void addImportedConnections(Scene &scene,
                                    const SimEngineImportResult &result,
                                    const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            std::unordered_set<UUID> importedIds(result.createdComponentIds.begin(),
                                                 result.createdComponentIds.end());
            auto &sceneState = scene.getState();
            auto &simEngine = SimulationEngine::instance();

            std::unordered_map<UUID, std::unordered_map<int, UUID>> inputSlotBySimId;
            std::unordered_map<UUID, std::unordered_map<int, UUID>> outputSlotBySimId;

            for (const auto &[simId, sceneComp] : sceneBySimId) {
                for (const auto &slotId : sceneComp->getInputSlots()) {
                    const auto slot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
                    if (!slot || slot->isResizeSlot()) {
                        continue;
                    }
                    inputSlotBySimId[simId][slot->getIndex()] = slotId;
                }

                for (const auto &slotId : sceneComp->getOutputSlots()) {
                    const auto slot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
                    if (!slot || slot->isResizeSlot()) {
                        continue;
                    }
                    outputSlotBySimId[simId][slot->getIndex()] = slotId;
                }
            }

            for (const auto &srcId : result.createdComponentIds) {
                const auto connections = simEngine.getConnections(srcId);
                for (size_t srcSlot = 0; srcSlot < connections.outputs.size(); ++srcSlot) {
                    for (const auto &[dstId, dstSlot] : connections.outputs[srcSlot]) {
                        if (!importedIds.contains(dstId)) {
                            continue;
                        }
                        if (!outputSlotBySimId.contains(srcId) ||
                            !outputSlotBySimId[srcId].contains(static_cast<int>(srcSlot)) ||
                            !inputSlotBySimId.contains(dstId) ||
                            !inputSlotBySimId[dstId].contains(dstSlot)) {
                            continue;
                        }

                        const auto startSlotId = outputSlotBySimId[srcId][static_cast<int>(srcSlot)];
                        const auto endSlotId = inputSlotBySimId[dstId][dstSlot];

                        auto startSlot = sceneState.getComponentByUuid<SlotSceneComponent>(startSlotId);
                        auto endSlot = sceneState.getComponentByUuid<SlotSceneComponent>(endSlotId);
                        if (!startSlot || !endSlot) {
                            continue;
                        }

                        auto conn = std::make_shared<ConnectionSceneComponent>();
                        conn->setStartEndSlots(startSlotId, endSlotId);
                        startSlot->addConnection(conn->getUuid());
                        endSlot->addConnection(conn->getUuid());
                        sceneState.addComponent(conn, false);
                    }
                }
            }
        }
    } // namespace


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
        try {
            auto scene = m_sceneDriver.getActiveScene();
            if (!scene) {
                if (errorMessage) {
                    *errorMessage = "No active scene available";
                }
                return false;
            }

            auto &simEngine = SimEngine::SimulationEngine::instance();
            const auto result = Verilog::importVerilogFileIntoSimulationEngine(path, simEngine);

            std::unordered_map<UUID, std::shared_ptr<Canvas::SimulationSceneComponent>> sceneBySimId;
            for (const auto &simId : result.createdComponentIds) {
                auto created = createSceneComponentForImportedSimId(simId);
                sceneBySimId[simId] = created.component;
                addImportedSceneComponent(scene->getState(), created);
            }

            layoutImportedComponents(result, sceneBySimId, *scene);
            addImportedConnections(*scene, result, sceneBySimId);
            m_sceneDriver.updateNets(scene);
            return true;
        } catch (const std::exception &ex) {
            if (errorMessage) {
                *errorMessage = ex.what();
            }
            BESS_ERROR("[MainPageState] Failed to import Verilog file {}: {}", path, ex.what());
            return false;
        }
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
        auto entity = e.state->getComponentByUuid(e.entityUuid);

        if (entity->getType() == Canvas::SceneComponentType::slot) {
            return;
        }

        UUID *parentPtr = &entity->getParentComponent();

        bool parentIsWasGroup = false;

        if (e.newParentUuid != UUID::null) {
            const auto &parentComp = e.state->getComponentByUuid(e.newParentUuid);
            parentIsWasGroup = parentComp->getType() == Canvas::SceneComponentType::group;
        }

        if (!parentIsWasGroup && e.prevParent != UUID::null) {
            const auto &prevParentComp = e.state->getComponentByUuid(e.prevParent);
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
        const auto callback = [this, entityUuid = e.entityUuid, state = e.state](bool isUndo, const UUID &newParent) {
            if (newParent == UUID::null)
                return;

            const auto &parent = state->getComponentByUuid(newParent);
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

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        auto &sceneState = m_sceneDriver.getSceneWithId(compData.sceneId)->getState();
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &outSlotsInfo = digitalComp->definition->getOutputSlotsInfo();

        size_t realCmpSize = outSlotsInfo.count;
        if (outSlotsInfo.isResizeable)
            realCmpSize = realCmpSize + 1; // doing this because scene comp will also contain a resize slot

        if (comp->getOutputSlotsCount() > realCmpSize) {
            for (size_t i = outSlotsInfo.count; i < comp->getOutputSlotsCount(); i++) {
                const auto slotUuid = comp->getOutputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);

                if (!slotComp) {
                    comp->getOutputSlots().erase(std::ranges::find(comp->getOutputSlots(), slotUuid));
                    continue;
                }

                if (!slotComp->isResizeSlot()) {
                    comp->getOutputSlots().erase(std::ranges::find(comp->getOutputSlots(), slotUuid));
                    sceneState.removeComponent(slotUuid, UUID::master);
                }
            }
            comp->setScaleDirty();
        } else if (comp->getOutputSlotsCount() < realCmpSize) {
            for (size_t i = comp->getOutputSlotsCount(); i < outSlotsInfo.count; i++) {
                std::shared_ptr<Canvas::SlotSceneComponent> newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setSlotType(Canvas::SlotType::digitalOutput);
                newSlot->setIndex((int)outSlotsInfo.count - 1);
                if (i < outSlotsInfo.names.size()) {
                    newSlot->setName(std::string(1, (char)('a' + i)));
                }
                comp->addOutputSlot(newSlot->getUuid(), outSlotsInfo.isResizeable);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(comp->getUuid(),
                                       newSlot->getUuid());
            }
        }
    }

    void MainPageState::onCompDefInputsResized(const SimEngine::Events::CompDefInputsResizedEvent &e) {
        if (!m_simIdToSceneCompId.contains(e.componentId)) {
            BESS_ERROR("Received CompDefInputsResizedEvent for unknown componentId: {}", (uint64_t)e.componentId);
            return;
        }

        const auto &compData = m_simIdToSceneCompId[e.componentId];

        auto &sceneState = m_sceneDriver.getSceneWithId(compData.sceneId)->getState();
        const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(compData.sceneCompId);
        if (!comp)
            return; // most likely the component was deleted, so we can ignore this event

        const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(e.componentId);
        const auto &inSlotsInfo = digitalComp->definition->getInputSlotsInfo();

        size_t realCmpSize = inSlotsInfo.count;
        if (inSlotsInfo.isResizeable)
            realCmpSize = realCmpSize + 1; // doing this because scene comp will also contain a resize slot

        if (comp->getInputSlotsCount() > realCmpSize) {
            for (size_t i = inSlotsInfo.count; i < comp->getInputSlotsCount(); i++) {
                const auto slotUuid = comp->getInputSlots()[i];
                const auto &slotComp = sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(slotUuid);
                if (!slotComp) {
                    comp->getInputSlots().erase(std::ranges::find(comp->getInputSlots(), slotUuid));
                    continue;
                }

                if (!slotComp->isResizeSlot()) {
                    comp->getInputSlots().erase(std::ranges::find(comp->getInputSlots(), slotUuid));
                    sceneState.removeComponent(slotUuid, UUID::master);
                }
            }
            comp->setScaleDirty();
        } else if (comp->getInputSlotsCount() < realCmpSize) {
            for (size_t i = comp->getInputSlotsCount(); i < inSlotsInfo.count; i++) {
                std::shared_ptr<Canvas::SlotSceneComponent> newSlot = std::make_shared<Canvas::SlotSceneComponent>();
                newSlot->setSlotType(Canvas::SlotType::digitalInput);
                newSlot->setIndex((int)inSlotsInfo.count - 1);
                if (i < inSlotsInfo.names.size()) {
                    newSlot->setName(std::string(1, (char)('A' + i)));
                }
                comp->addInputSlot(newSlot->getUuid(), inSlotsInfo.isResizeable);
                sceneState.addComponent<Canvas::SlotSceneComponent>(newSlot);
                sceneState.attachChild(comp->getUuid(), newSlot->getUuid());
            }
        }
    }

    void MainPageState::onEntityAdded(const Canvas::Events::ComponentAddedEvent &e) {
        const auto &comp = e.state->getComponentByUuid(e.uuid);

        if (e.type == Canvas::SceneComponentType::simulation ||
            e.type == Canvas::SceneComponentType::module) {
            const auto &simComp = comp->cast<Canvas::SimulationSceneComponent>();
            const auto &simEngineId = simComp->getSimEngineId();
            m_simIdToSceneCompId[simEngineId] = {e.uuid, e.state->getSceneId()};
        }
    }

    void MainPageState::onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e) {
        if (e.type != Canvas::SceneComponentType::simulation)
            return;

        auto it = std::ranges::find_if(m_simIdToSceneCompId,
                                       [&e](const auto &pair) { return pair.second.sceneCompId == e.uuid; });

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
