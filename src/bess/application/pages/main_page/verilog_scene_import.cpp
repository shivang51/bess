#include "pages/main_page/verilog_scene_import.h"

#include "common/bess_assert.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include <algorithm>
#include <deque>
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
            result.component = created.front()->cast<SimulationSceneComponent>();
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
            result.component = created.front()->cast<ModuleSceneComponent>();
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

        std::unordered_map<UUID, int> computeImportedLevels(const SimEngineImportResult &result,
                                                            SimulationEngine &simEngine) {
            std::unordered_set<UUID> importedIds(result.createdComponentIds.begin(),
                                                 result.createdComponentIds.end());
            std::unordered_map<UUID, int> indegree;
            std::unordered_map<UUID, std::vector<UUID>> adjacency;

            for (const auto &id : result.createdComponentIds) {
                indegree[id] = 0;
            }

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
                                      SimulationEngine &simEngine,
                                      const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId,
                                      Scene &scene) {
            auto levels = computeImportedLevels(result, simEngine);
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
                                    const SimEngineImportResult &result,
                                    SimulationEngine &simEngine,
                                    const std::unordered_map<UUID, std::shared_ptr<SimulationSceneComponent>> &sceneBySimId) {
            std::unordered_set<UUID> importedIds(result.createdComponentIds.begin(),
                                                 result.createdComponentIds.end());
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

        glm::vec3 getSceneComponentPosition(const std::shared_ptr<SceneComponent> &component) {
            return component->getTransform().position;
        }

        void setImportedChildRelativeTransform(const std::shared_ptr<SceneComponent> &component,
                                               const glm::vec3 &parentPos) {
            auto &transform = component->getTransform();
            transform.position -= parentPos;
            if (const auto simComp = component->cast<SimulationSceneComponent>()) {
                auto schematicTransform = simComp->getSchematicTransform();
                schematicTransform.position -= parentPos;
                simComp->setSchematicTransform(schematicTransform);
            }
        }

        void buildImportedModuleHierarchy(
            const SimEngineImportResult &result,
            Scene &scene,
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

                std::vector<std::shared_ptr<SceneComponent>> ownedChildren;
                for (const auto &[simId, ownerPath] : result.componentInstancePathById) {
                    if (ownerPath != path || !sceneBySimId.contains(simId)) {
                        continue;
                    }
                    ownedChildren.push_back(sceneBySimId.at(simId));
                }

                for (const auto &[childPath, childModule] : moduleByPath) {
                    (void)childModule;
                    if (result.instancesByPath.contains(childPath) &&
                        result.instancesByPath.at(childPath).parentInstancePath == path) {
                        ownedChildren.push_back(moduleByPath.at(childPath));
                    }
                }

                if (ownedChildren.empty()) {
                    continue;
                }

                float minX = std::numeric_limits<float>::max();
                float minY = std::numeric_limits<float>::max();
                for (const auto &child : ownedChildren) {
                    const auto pos = getSceneComponentPosition(child);
                    minX = std::min(minX, pos.x);
                    minY = std::min(minY, pos.y);
                }

                wrapper->getTransform().position = glm::vec3{minX - 120.f, minY - 80.f, scene.getNextZCoord()};
                wrapper->setSchematicTransform(wrapper->getTransform());

                for (const auto &child : ownedChildren) {
                    setImportedChildRelativeTransform(child, wrapper->getTransform().position);
                    sceneState.attachChild(wrapper->getUuid(), child->getUuid(), false);
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
        layoutImportedComponents(result, simEngine, sceneBySimId, scene);
        stackTopOutputComponents(result, sceneBySimId);
        buildImportedModuleHierarchy(result, scene, sceneBySimId);
        addImportedConnections(scene, result, simEngine, sceneBySimId);
    }
} // namespace Bess::Pages
