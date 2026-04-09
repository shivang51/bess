#include "pages/main_page/verilog_scene_import.h"

#include "common/bess_assert.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene.h"
#include "event_dispatcher.h"
#include "module_def.h"
#include "simulation_engine.h"
#include <algorithm>
#include <deque>
#include <functional>
#include <limits>
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

        struct ImportedModuleSceneComponent {
            std::shared_ptr<ModuleSceneComponent> component;
            std::vector<std::shared_ptr<SceneComponent>> children;
        };

        std::string leafInstanceName(std::string_view path) {
            const auto separator = path.rfind('/');
            if (separator == std::string_view::npos) {
                return std::string(path);
            }
            return std::string(path.substr(separator + 1));
        }

        size_t instanceDepth(std::string_view path) {
            return std::count(path.begin(), path.end(), '/');
        }

        ImportedSceneComponent createSceneComponentForImportedSimId(const UUID &simId,
                                                                   SimulationEngine &simEngine) {
            const auto &compDef = simEngine.getComponentDefinition(simId);
            auto created = SimulationSceneComponent::createNewAndRegister(compDef);
            BESS_ASSERT(!created.empty(), "Failed to create scene component for imported sim component");

            ImportedSceneComponent result;
            result.component = std::dynamic_pointer_cast<SimulationSceneComponent>(created.front());
            BESS_ASSERT(result.component, "Imported scene root is not a simulation scene component");
            result.component->setSimEngineId(simId);
            result.component->setName(compDef->getName());
            created.erase(created.begin());
            result.children = std::move(created);
            return result;
        }

        ImportedModuleSceneComponent createModuleSceneComponentForImportedInstance(const std::string &instancePath) {
            UUID moduleInputId = UUID::null;
            UUID moduleOutputId = UUID::null;
            auto created = ModuleSceneComponent::createNew(moduleInputId, moduleOutputId);
            BESS_ASSERT(!created.empty(), "Failed to create module scene component for imported instance");

            ImportedModuleSceneComponent result;
            result.component = std::dynamic_pointer_cast<ModuleSceneComponent>(created.front());
            BESS_ASSERT(result.component, "Imported module wrapper is not a module scene component");
            result.component->setName(leafInstanceName(instancePath));
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

        void addImportedModuleSceneComponent(SceneState &sceneState,
                                             ImportedModuleSceneComponent &moduleComp) {
            sceneState.addComponent(moduleComp.component);
            for (const auto &child : moduleComp.children) {
                sceneState.addComponent(child);
                sceneState.attachChild(moduleComp.component->getUuid(), child->getUuid(), false);
            }
        }

        void applyImportedPortNames(
            const SimEngineImportResult &result,
            const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            for (const auto &[portName, simId] : result.topInputComponents) {
                const auto it = sceneBySimId.find(simId);
                if (it != sceneBySimId.end()) {
                    it->second->setName(portName);
                }
            }

            for (const auto &[portName, simId] : result.topOutputComponents) {
                const auto it = sceneBySimId.find(simId);
                if (it != sceneBySimId.end()) {
                    it->second->setName(portName);
                }
            }
        }

        void resizeOutputs(const std::shared_ptr<DigitalComponent> &component, size_t count) {
            while (component->definition->getOutputSlotsInfo().count < count) {
                component->incrementOutputCount(true);
            }
            while (component->definition->getOutputSlotsInfo().count > count &&
                   component->definition->getOutputSlotsInfo().count > 1) {
                component->decrementOutputCount(true);
            }
        }

        void resizeInputs(const std::shared_ptr<DigitalComponent> &component, size_t count) {
            while (component->definition->getInputSlotsInfo().count < count) {
                component->incrementInputCount(true);
            }
            while (component->definition->getInputSlotsInfo().count > count &&
                   component->definition->getInputSlotsInfo().count > 1) {
                component->decrementInputCount(true);
            }
        }

        void applySlotNames(SceneState &sceneState,
                            const std::vector<UUID> &slotIds,
                            const std::vector<std::string> &slotNames) {
            size_t nameIdx = 0;
            for (const auto &slotId : slotIds) {
                const auto slot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);
                if (!slot || slot->isResizeSlot()) {
                    continue;
                }
                if (nameIdx < slotNames.size()) {
                    slot->setName(slotNames[nameIdx]);
                }
                ++nameIdx;
            }
        }

        std::unordered_map<UUID, int> computeImportedLevels(const std::vector<UUID> &componentIds,
                                                            SimulationEngine &simEngine) {
            std::unordered_set<UUID> importedIds(componentIds.begin(), componentIds.end());
            std::unordered_map<UUID, int> indegree;
            std::unordered_map<UUID, std::vector<UUID>> adjacency;

            for (const auto &id : componentIds) {
                indegree[id] = 0;
            }

            for (const auto &id : componentIds) {
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

            for (const auto &id : componentIds) {
                if (!levels.contains(id)) {
                    levels[id] = 0;
                }
            }

            return levels;
        }

        void layoutImportedComponents(const std::vector<UUID> &componentIds,
                                      SimulationEngine &simEngine,
                                      const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId,
                                      Scene &scene) {
            auto levels = computeImportedLevels(componentIds, simEngine);
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
                    const auto &defA = simEngine.getComponentDefinition(a);
                    const auto &defB = simEngine.getComponentDefinition(b);
                    return defA->getName() < defB->getName();
                });

                const float totalHeight = (static_cast<float>(ids.size() - 1) * ySpacing);
                for (size_t i = 0; i < ids.size(); ++i) {
                    const auto it = sceneBySimId.find(ids[i]);
                    if (it == sceneBySimId.end()) {
                        continue;
                    }
                    auto &transform = it->second->getTransform();
                    transform.position = glm::vec3{
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

        void stackTopOutputComponents(const SimEngineImportResult &result,
                                      const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            std::vector<std::pair<std::string, std::shared_ptr<SimulationSceneComponent>>> topOutputs;
            topOutputs.reserve(result.topOutputComponents.size());

            for (const auto &[portName, simId] : result.topOutputComponents) {
                const auto it = sceneBySimId.find(simId);
                if (it == sceneBySimId.end()) {
                    continue;
                }
                topOutputs.emplace_back(portName, it->second);
            }

            if (topOutputs.size() < 2) {
                return;
            }

            std::ranges::sort(topOutputs, [](const auto &lhs, const auto &rhs) {
                return lhs.first < rhs.first;
            });

            float maxX = std::numeric_limits<float>::lowest();
            float avgY = 0.f;
            for (const auto &[portName, component] : topOutputs) {
                (void)portName;
                const auto pos = component->getTransform().position;
                maxX = std::max(maxX, pos.x);
                avgY += pos.y;
            }
            avgY /= static_cast<float>(topOutputs.size());

            constexpr float ySpacing = 140.f;
            const float totalHeight = (static_cast<float>(topOutputs.size() - 1) * ySpacing);
            for (size_t i = 0; i < topOutputs.size(); ++i) {
                auto &component = topOutputs[i].second;
                auto transform = component->getTransform();
                transform.position.x = maxX;
                transform.position.y = (static_cast<float>(i) * ySpacing) - (totalHeight / 2.f) + avgY;
                component->setTransform(transform);

                auto schematicTransform = component->getSchematicTransform();
                schematicTransform.position = transform.position;
                component->setSchematicTransform(schematicTransform);
                component->setScaleDirty();
                component->setSchematicScaleDirty();
            }
        }

        void addImportedConnections(Scene &scene,
                                    const std::vector<UUID> &componentIds,
                                    SimulationEngine &simEngine,
                                    const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            std::unordered_set<UUID> importedIds(componentIds.begin(), componentIds.end());
            auto &sceneState = scene.getState();
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

            for (const auto &srcId : componentIds) {
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

        glm::vec3 getSceneComponentPosition(const std::shared_ptr<SceneComponent> &component) {
            return component->getTransform().position;
        }

        void setImportedChildRelativeTransform(const std::shared_ptr<SceneComponent> &component,
                                               const glm::vec3 &parentPos) {
            auto &transform = component->getTransform();
            transform.position -= parentPos;
            if (const auto simComp = std::dynamic_pointer_cast<SimulationSceneComponent>(component)) {
                auto schematicTransform = simComp->getSchematicTransform();
                schematicTransform.position -= parentPos;
                simComp->setSchematicTransform(schematicTransform);
            }
        }

        std::string endpointKey(UUID componentId, SimEngine::SlotType slotType, int slotIndex) {
            return std::to_string(static_cast<uint64_t>(componentId)) + ":" +
                   std::to_string(static_cast<int>(slotType)) + ":" +
                   std::to_string(slotIndex);
        }

        std::string endpointKey(const ImportedSlotEndpoint &endpoint) {
            return endpointKey(endpoint.componentId, endpoint.slotType, endpoint.slotIndex);
        }

        bool isOwnedByInstance(const SimEngineImportResult &result,
                               const UUID &componentId,
                               std::string_view instancePath) {
            const auto it = result.componentInstancePathById.find(componentId);
            return it != result.componentInstancePathById.end() && it->second == instancePath;
        }

        void configureImportedModuleInterface(const ImportedModuleSceneComponent &moduleComp,
                                              const ImportedModuleInstance &instance,
                                              SimulationEngine &simEngine) {
            auto moduleDef = std::dynamic_pointer_cast<ModuleDefinition>(moduleComp.component->getCompDef());
            BESS_ASSERT(moduleDef, "Imported module wrapper is missing its module definition");

            const auto inputCount = std::max<size_t>(1, instance.inputSlotNames.size());
            const auto outputCount = std::max<size_t>(1, instance.outputSlotNames.size());

            const auto inputComponent = simEngine.getDigitalComponent(moduleDef->getInputId());
            const auto outputComponent = simEngine.getDigitalComponent(moduleDef->getOutputId());
            BESS_ASSERT(inputComponent, "Imported module input bridge component was not found");
            BESS_ASSERT(outputComponent, "Imported module output bridge component was not found");

            resizeOutputs(inputComponent, inputCount);
            inputComponent->definition->getOutputSlotsInfo().names = instance.inputSlotNames;

            resizeInputs(outputComponent, outputCount);
            outputComponent->definition->getInputSlotsInfo().names = instance.outputSlotNames;

            moduleDef->getInputSlotsInfo().names = instance.inputSlotNames;
            moduleDef->getOutputSlotsInfo().names = instance.outputSlotNames;

            auto &sceneDriver = MainPage::getInstance()->getState().getSceneDriver();
            const auto moduleScene = sceneDriver.getSceneWithId(moduleComp.component->getSceneId());
            if (!moduleScene) {
                return;
            }

            auto &moduleSceneState = moduleScene->getState();
            const auto moduleInput = moduleSceneState.getComponentByUuid<SimulationSceneComponent>(moduleComp.component->getAssociatedInp());
            const auto moduleOutput = moduleSceneState.getComponentByUuid<SimulationSceneComponent>(moduleComp.component->getAssociatedOut());
            if (moduleInput) {
                moduleInput->setName("Module Input");
                applySlotNames(moduleSceneState, moduleInput->getOutputSlots(), instance.inputSlotNames);
            }
            if (moduleOutput) {
                moduleOutput->setName("Module Output");
                applySlotNames(moduleSceneState, moduleOutput->getInputSlots(), instance.outputSlotNames);
            }

            applySlotNames(sceneDriver.getRootSceneId() == UUID::null ? moduleSceneState : sceneDriver.getSceneWithId(sceneDriver.getRootSceneId())->getState(),
                           moduleComp.component->getInputSlots(),
                           instance.inputSlotNames);
            applySlotNames(sceneDriver.getRootSceneId() == UUID::null ? moduleSceneState : sceneDriver.getSceneWithId(sceneDriver.getRootSceneId())->getState(),
                           moduleComp.component->getOutputSlots(),
                           instance.outputSlotNames);
        }

        void bridgeImportedModuleBoundary(const SimEngineImportResult &result,
                                          const ImportedModuleInstance &instance,
                                          const ImportedModuleSceneComponent &moduleComp,
                                          SimulationEngine &simEngine) {
            auto moduleDef = std::dynamic_pointer_cast<ModuleDefinition>(moduleComp.component->getCompDef());
            BESS_ASSERT(moduleDef, "Imported module wrapper is missing its module definition");

            const auto wrapperId = moduleComp.component->getSimEngineId();
            const auto moduleInputId = moduleDef->getInputId();
            const auto moduleOutputId = moduleDef->getOutputId();

            for (size_t slotIndex = 0; slotIndex < instance.internalInputSinks.size(); ++slotIndex) {
                std::unordered_set<std::string> seenSources;
                for (const auto &sink : instance.internalInputSinks[slotIndex]) {
                    const auto connections = simEngine.getConnections(sink.componentId);
                    if (sink.slotIndex < 0 || sink.slotIndex >= static_cast<int>(connections.inputs.size())) {
                        continue;
                    }

                    const auto incomingConnections = connections.inputs[sink.slotIndex];
                    for (const auto &[srcId, srcSlotIndex] : incomingConnections) {
                        if (srcId == moduleInputId || srcId == wrapperId) {
                            continue;
                        }

                        if (isOwnedByInstance(result, srcId, instance.instancePath)) {
                            continue;
                        }

                        simEngine.deleteConnection(srcId,
                                                   SimEngine::SlotType::digitalOutput,
                                                   srcSlotIndex,
                                                   sink.componentId,
                                                   sink.slotType,
                                                   sink.slotIndex);

                        const auto key = endpointKey(srcId, SimEngine::SlotType::digitalOutput, srcSlotIndex);
                        if (!seenSources.insert(key).second) {
                            continue;
                        }

                        simEngine.connectComponent(srcId,
                                                   srcSlotIndex,
                                                   SimEngine::SlotType::digitalOutput,
                                                   wrapperId,
                                                   static_cast<int>(slotIndex),
                                                   SimEngine::SlotType::digitalInput);
                    }

                    simEngine.connectComponent(moduleInputId,
                                               static_cast<int>(slotIndex),
                                               SimEngine::SlotType::digitalOutput,
                                               sink.componentId,
                                               sink.slotIndex,
                                               sink.slotType);
                }
            }

            for (size_t slotIndex = 0; slotIndex < instance.internalOutputDrivers.size(); ++slotIndex) {
                std::unordered_set<std::string> seenTargets;
                for (const auto &driver : instance.internalOutputDrivers[slotIndex]) {
                    simEngine.connectComponent(driver.componentId,
                                               driver.slotIndex,
                                               driver.slotType,
                                               moduleOutputId,
                                               static_cast<int>(slotIndex),
                                               SimEngine::SlotType::digitalInput);

                    const auto connections = simEngine.getConnections(driver.componentId);
                    if (driver.slotIndex < 0 || driver.slotIndex >= static_cast<int>(connections.outputs.size())) {
                        continue;
                    }

                    const auto outgoingConnections = connections.outputs[driver.slotIndex];
                    for (const auto &[dstId, dstSlotIndex] : outgoingConnections) {
                        if (dstId == moduleOutputId || dstId == wrapperId) {
                            continue;
                        }

                        if (isOwnedByInstance(result, dstId, instance.instancePath)) {
                            continue;
                        }

                        simEngine.deleteConnection(driver.componentId,
                                                   driver.slotType,
                                                   driver.slotIndex,
                                                   dstId,
                                                   SimEngine::SlotType::digitalInput,
                                                   dstSlotIndex);

                        const auto key = endpointKey(dstId, SimEngine::SlotType::digitalInput, dstSlotIndex);
                        if (!seenTargets.insert(key).second) {
                            continue;
                        }

                        simEngine.connectComponent(wrapperId,
                                                   static_cast<int>(slotIndex),
                                                   SimEngine::SlotType::digitalOutput,
                                                   dstId,
                                                   dstSlotIndex,
                                                   SimEngine::SlotType::digitalInput);
                    }
                }
            }
        }

        void populateImportedModuleScene(const SimEngineImportResult &result,
                                         const ImportedModuleInstance &instance,
                                         SimulationEngine &simEngine,
                                         const ImportedModuleSceneComponent &moduleComp) {
            auto &sceneDriver = MainPage::getInstance()->getState().getSceneDriver();
            const auto moduleScene = sceneDriver.getSceneWithId(moduleComp.component->getSceneId());
            if (!moduleScene) {
                return;
            }

            auto &moduleSceneState = moduleScene->getState();
            std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> internalSceneBySimId;
            std::vector<UUID> internalSimIds;

            const auto moduleInput = moduleSceneState.getComponentByUuid<SimulationSceneComponent>(moduleComp.component->getAssociatedInp());
            const auto moduleOutput = moduleSceneState.getComponentByUuid<SimulationSceneComponent>(moduleComp.component->getAssociatedOut());
            if (moduleInput) {
                internalSceneBySimId[moduleInput->getSimEngineId()] = moduleInput;
                internalSimIds.push_back(moduleInput->getSimEngineId());
            }
            if (moduleOutput) {
                internalSceneBySimId[moduleOutput->getSimEngineId()] = moduleOutput;
                internalSimIds.push_back(moduleOutput->getSimEngineId());
            }

            for (const auto &[simId, ownerPath] : result.componentInstancePathById) {
                if (ownerPath != instance.instancePath) {
                    continue;
                }

                auto created = createSceneComponentForImportedSimId(simId, simEngine);
                internalSceneBySimId[simId] = created.component;
                internalSimIds.push_back(simId);
                addImportedSceneComponent(moduleSceneState, created);
            }

            if (moduleInput) {
                auto transform = moduleInput->getTransform();
                transform.position = glm::vec3{-220.f, 0.f, moduleScene->getNextZCoord()};
                moduleInput->setTransform(transform);
                auto schematicTransform = moduleInput->getSchematicTransform();
                schematicTransform.position = transform.position;
                moduleInput->setSchematicTransform(schematicTransform);
            }

            if (moduleOutput) {
                auto transform = moduleOutput->getTransform();
                transform.position = glm::vec3{220.f, 0.f, moduleScene->getNextZCoord()};
                moduleOutput->setTransform(transform);
                auto schematicTransform = moduleOutput->getSchematicTransform();
                schematicTransform.position = transform.position;
                moduleOutput->setSchematicTransform(schematicTransform);
            }

            if (!internalSimIds.empty()) {
                layoutImportedComponents(internalSimIds, simEngine, internalSceneBySimId, *moduleScene);
                addImportedConnections(*moduleScene, internalSimIds, simEngine, internalSceneBySimId);
            }
        }

        std::unordered_map<std::string, std::shared_ptr<ModuleSceneComponent>> buildImportedModuleHierarchy(
            const SimEngineImportResult &result,
            Scene &scene,
            SimulationEngine &simEngine,
            const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            auto &sceneState = scene.getState();

            std::vector<std::pair<std::string, ImportedModuleInstance>> modulePaths;
            for (const auto &[path, instance] : result.instancesByPath) {
                if (path == result.topModuleName) {
                    continue;
                }
                modulePaths.emplace_back(path, instance);
            }

            std::ranges::sort(modulePaths, [](const auto &lhs, const auto &rhs) {
                const auto lhsDepth = instanceDepth(lhs.first);
                const auto rhsDepth = instanceDepth(rhs.first);
                if (lhsDepth != rhsDepth) {
                    return lhsDepth > rhsDepth;
                }
                return lhs.first < rhs.first;
            });

            std::unordered_map<std::string, std::shared_ptr<ModuleSceneComponent>> moduleByPath;

            for (const auto &[path, instance] : modulePaths) {
                ImportedModuleSceneComponent created = createModuleSceneComponentForImportedInstance(path);
                const auto wrapper = created.component;
                addImportedModuleSceneComponent(sceneState, created);
                moduleByPath[path] = wrapper;
                configureImportedModuleInterface(created, instance, simEngine);
                EventSystem::EventDispatcher::instance().dispatchAll();
                bridgeImportedModuleBoundary(result, instance, created, simEngine);
                populateImportedModuleScene(result, instance, simEngine, created);

                std::vector<std::shared_ptr<SceneComponent>> layoutChildren;
                for (const auto &[simId, ownerPath] : result.componentInstancePathById) {
                    if (ownerPath != path || !sceneBySimId.contains(simId)) {
                        continue;
                    }
                    layoutChildren.push_back(sceneBySimId.at(simId));
                }

                for (const auto &[childPath, childModule] : moduleByPath) {
                    (void)childModule;
                    if (result.instancesByPath.contains(childPath) &&
                        result.instancesByPath.at(childPath).parentInstancePath == path) {
                        layoutChildren.push_back(moduleByPath.at(childPath));
                    }
                }

                if (layoutChildren.empty()) {
                    continue;
                }

                float minX = std::numeric_limits<float>::max();
                float minY = std::numeric_limits<float>::max();
                for (const auto &child : layoutChildren) {
                    const auto pos = getSceneComponentPosition(child);
                    minX = std::min(minX, pos.x);
                    minY = std::min(minY, pos.y);
                }

                wrapper->getTransform().position = glm::vec3{minX - 120.f, minY - 80.f, scene.getNextZCoord()};
                wrapper->setSchematicTransform(wrapper->getTransform());
            }

            return moduleByPath;
        }

        void removeNestedImportedComponentsFromRootScene(
            const SimEngineImportResult &result,
            Scene &scene,
            std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            auto collectSceneTree = [&](const std::shared_ptr<SceneComponent> &root) {
                std::vector<std::shared_ptr<SceneComponent>> ordered;
                std::unordered_set<UUID> visited;
                std::function<void(const std::shared_ptr<SceneComponent> &)> walk =
                    [&](const std::shared_ptr<SceneComponent> &component) {
                        if (!component || visited.contains(component->getUuid())) {
                            return;
                        }

                        visited.insert(component->getUuid());
                        const auto dependants = component->getDependants(scene.getState());
                        for (const auto &dependantId : dependants) {
                            walk(scene.getState().getComponentByUuid(dependantId));
                        }
                        ordered.push_back(component);
                    };

                walk(root);
                return ordered;
            };

            std::vector<UUID> toRemove;
            for (const auto &[simId, ownerPath] : result.componentInstancePathById) {
                if (ownerPath == result.topModuleName || !sceneBySimId.contains(simId)) {
                    continue;
                }
                toRemove.push_back(simId);
            }

            auto &sceneState = scene.getState();
            for (const auto &simId : toRemove) {
                if (!sceneBySimId.contains(simId)) {
                    continue;
                }

                const auto rootComponent = sceneBySimId.at(simId);
                const auto detachedComponents = collectSceneTree(rootComponent);
                for (const auto &component : detachedComponents) {
                    if (!component) {
                        continue;
                    }

                    if (component->getParentComponent() != UUID::null) {
                        const auto parent = sceneState.getComponentByUuid(component->getParentComponent());
                        if (parent) {
                            parent->removeChildComponent(component->getUuid());
                        }
                    }
                    sceneState.removeFromMap(component->getUuid());
                }
                sceneBySimId.erase(simId);
            }
        }

        void addRootModuleConnections(Scene &scene,
                                      SimulationEngine &simEngine,
                                      const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId,
                                      const std::unordered_map<std::string, std::shared_ptr<ModuleSceneComponent>> &moduleByPath) {
            auto &sceneState = scene.getState();
            std::unordered_map<UUID, std::unordered_map<int, UUID>> inputSlotBySimId;
            std::unordered_map<UUID, std::unordered_map<int, UUID>> outputSlotBySimId;
            std::unordered_set<UUID> visibleRootSimIds;

            auto registerVisibleComponent = [&](const std::shared_ptr<SimulationSceneComponent> &sceneComp) {
                if (!sceneComp) {
                    return;
                }

                const auto simId = sceneComp->getSimEngineId();
                visibleRootSimIds.insert(simId);

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
            };

            for (const auto &[simId, sceneComp] : sceneBySimId) {
                (void)simId;
                registerVisibleComponent(sceneComp);
            }

            for (const auto &[path, wrapper] : moduleByPath) {
                (void)path;
                registerVisibleComponent(wrapper);
            }

            std::unordered_set<std::string> createdConnections;
            for (const auto &srcId : visibleRootSimIds) {
                const auto connections = simEngine.getConnections(srcId);
                for (size_t srcSlot = 0; srcSlot < connections.outputs.size(); ++srcSlot) {
                    for (const auto &[dstId, dstSlot] : connections.outputs[srcSlot]) {
                        if (!visibleRootSimIds.contains(dstId)) {
                            continue;
                        }

                        if (!outputSlotBySimId.contains(srcId) ||
                            !outputSlotBySimId.at(srcId).contains(static_cast<int>(srcSlot)) ||
                            !inputSlotBySimId.contains(dstId) ||
                            !inputSlotBySimId.at(dstId).contains(dstSlot)) {
                            continue;
                        }

                        const auto startSlotId = outputSlotBySimId.at(srcId).at(static_cast<int>(srcSlot));
                        const auto endSlotId = inputSlotBySimId.at(dstId).at(dstSlot);
                        if (startSlotId == UUID::null || endSlotId == UUID::null || startSlotId == endSlotId) {
                            continue;
                        }

                        const auto dedupeKey = std::to_string(static_cast<uint64_t>(startSlotId)) + ":" +
                                               std::to_string(static_cast<uint64_t>(endSlotId));
                        if (!createdConnections.insert(dedupeKey).second) {
                            continue;
                        }

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

    void populateSceneFromVerilogImportResult(const Verilog::SimEngineImportResult &result,
                                              SimEngine::SimulationEngine &simEngine,
                                              Canvas::Scene &scene) {
        std::unordered_map<UUID, std::shared_ptr<Canvas::SimulationSceneComponent>> sceneBySimId;
        for (const auto &simId : result.createdComponentIds) {
            auto created = createSceneComponentForImportedSimId(simId, simEngine);
            sceneBySimId[simId] = created.component;
            addImportedSceneComponent(scene.getState(), created);
        }

        applyImportedPortNames(result, sceneBySimId);
        layoutImportedComponents(result.createdComponentIds, simEngine, sceneBySimId, scene);
        stackTopOutputComponents(result, sceneBySimId);
        const auto moduleByPath = buildImportedModuleHierarchy(result, scene, simEngine, sceneBySimId);
        removeNestedImportedComponentsFromRootScene(result, scene, sceneBySimId);
        addRootModuleConnections(scene, simEngine, sceneBySimId, moduleByPath);
        EventSystem::EventDispatcher::instance().dispatchAll();
    }
} // namespace Bess::Pages
