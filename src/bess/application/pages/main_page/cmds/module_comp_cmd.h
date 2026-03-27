#pragma once

#include "command.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "module_def.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

namespace Bess::Cmd {
    namespace Detail {
        inline void transferComponents(Canvas::Scene *fromScene,
                                       Canvas::Scene *toScene,
                                       const std::vector<std::shared_ptr<Canvas::SceneComponent>> &components) {
            BESS_ASSERT(fromScene, "[ModuleCmd] Source scene must be valid");
            BESS_ASSERT(toScene, "[ModuleCmd] Destination scene must be valid");

            auto &fromState = fromScene->getState();
            auto &toState = toScene->getState();

            for (const auto &component : components) {
                if (!component) {
                    continue;
                }

                fromState.removeFromMap(component->getUuid());
                toState.addComponent(component, false, false);
            }
        }

        inline std::vector<std::shared_ptr<Canvas::SceneComponent>>
        collectComponentsForModule(const std::shared_ptr<Canvas::Scene> &scene,
                                   const std::vector<UUID> &componentIds) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid while collecting components");

            const auto &sceneState = scene->getState();
            std::unordered_set<UUID> visited;
            std::vector<std::shared_ptr<Canvas::SceneComponent>> components;

            std::function<void(const UUID &)> collect = [&](const UUID &uuid) {
                if (visited.contains(uuid)) {
                    return;
                }

                const auto component = sceneState.getComponentByUuid(uuid);
                if (!component) {
                    return;
                }

                visited.insert(uuid);
                for (const auto &dependantUuid : component->getDependants(sceneState)) {
                    collect(dependantUuid);
                }
                components.push_back(component);
            };

            for (const auto &componentId : componentIds) {
                collect(componentId);
            }

            return components;
        }

        inline std::vector<UUID> collectRootComponentIds(const std::shared_ptr<Canvas::Scene> &scene) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid while collecting root ids");
            const auto &rootComponents = scene->getState().getRootComponents();
            return {rootComponents.begin(), rootComponents.end()};
        }

        inline std::shared_ptr<Canvas::ModuleSceneComponent>
        getOwningModuleComponent(const std::shared_ptr<Canvas::Scene> &scene) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid while resolving owning module");

            const auto &sceneState = scene->getState();
            if (sceneState.getIsRootScene() ||
                sceneState.getModuleId() == UUID::null ||
                sceneState.getParentSceneId() == UUID::null) {
                return nullptr;
            }

            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            const auto parentScene = sceneDriver.getSceneWithId(sceneState.getParentSceneId());
            if (!parentScene) {
                return nullptr;
            }

            return parentScene->getState().getComponentByUuid<Canvas::ModuleSceneComponent>(
                sceneState.getModuleId());
        }

        inline std::unordered_set<UUID>
        collectConnectedConnectionIds(const std::shared_ptr<Canvas::Scene> &scene,
                                     const UUID &componentId) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid while collecting boundary connections");

            std::unordered_set<UUID> connectionIds;
            const auto simComponent = scene->getState().getComponentByUuid<Canvas::SimulationSceneComponent>(
                componentId);
            if (!simComponent) {
                return connectionIds;
            }

            const auto collectSlotConnections = [&](const std::vector<UUID> &slotIds) {
                for (const auto &slotId : slotIds) {
                    const auto slotComponent = scene->getState().getComponentByUuid<Canvas::SlotSceneComponent>(
                        slotId);
                    if (!slotComponent || slotComponent->isResizeSlot()) {
                        continue;
                    }

                    for (const auto &connectionId : slotComponent->getConnectedConnections()) {
                        connectionIds.insert(connectionId);
                    }
                }
            };

            collectSlotConnections(simComponent->getInputSlots());
            collectSlotConnections(simComponent->getOutputSlots());
            return connectionIds;
        }

        inline std::unordered_set<UUID>
        collectBoundaryExclusionIds(const std::shared_ptr<Canvas::Scene> &scene,
                                    const UUID &netId,
                                    std::vector<UUID> &connectionIdsToDisconnect) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid while collecting module boundary exclusions");

            std::unordered_set<UUID> excludedIds;
            const auto owningModule = getOwningModuleComponent(scene);
            if (!owningModule) {
                return excludedIds;
            }

            const auto &sceneState = scene->getState();
            for (const auto &boundaryComponentId :
                 {owningModule->getAssociatedInp(), owningModule->getAssociatedOut()}) {
                const auto boundaryComponent = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                    boundaryComponentId);
                if (!boundaryComponent || boundaryComponent->getNetId() != netId) {
                    continue;
                }

                const auto boundaryComponents = collectComponentsForModule(scene, {boundaryComponentId});
                for (const auto &excludedComponent : boundaryComponents) {
                    if (excludedComponent) {
                        excludedIds.insert(excludedComponent->getUuid());
                    }
                }

                const auto connectionIds = collectConnectedConnectionIds(scene, boundaryComponentId);
                for (const auto &connectionId : connectionIds) {
                    if (!sceneState.isComponentValid(connectionId)) {
                        continue;
                    }

                    connectionIdsToDisconnect.push_back(connectionId);
                    const auto excludedComponents = collectComponentsForModule(scene, {connectionId});
                    for (const auto &excludedComponent : excludedComponents) {
                        if (excludedComponent) {
                            excludedIds.insert(excludedComponent->getUuid());
                        }
                    }
                }
            }

            return excludedIds;
        }

        inline void rewireModuleIoIds(const std::shared_ptr<Canvas::ModuleSceneComponent> &moduleComponent,
                                      const std::shared_ptr<Canvas::Scene> &moduleScene) {
            BESS_ASSERT(moduleComponent, "[ModuleCmd] Module component must be valid");
            BESS_ASSERT(moduleScene, "[ModuleCmd] Module scene must be valid");

            const auto moduleDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(
                moduleComponent->getCompDef());
            BESS_ASSERT(moduleDef, "[ModuleCmd] Module definition must be valid");

            const auto &moduleState = moduleScene->getState();
            const auto inputComponent = moduleState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                moduleComponent->getAssociatedInp());
            const auto outputComponent = moduleState.getComponentByUuid<Canvas::SimulationSceneComponent>(
                moduleComponent->getAssociatedOut());

            BESS_ASSERT(inputComponent, "[ModuleCmd] Associated module input component not found");
            BESS_ASSERT(outputComponent, "[ModuleCmd] Associated module output component not found");

            moduleDef->setInputId(inputComponent->getSimEngineId());
            moduleDef->setOutputId(outputComponent->getSimEngineId());
        }

        inline void ensureSceneRegistered(const std::shared_ptr<Canvas::Scene> &scene) {
            BESS_ASSERT(scene, "[ModuleCmd] Scene must be valid before registration");

            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            if (!sceneDriver.getSceneWithId(scene->getSceneId())) {
                sceneDriver.addScene(scene);
            }

            if (scene->getState().getParentSceneId() == UUID::null) {
                scene->getState().setParentSceneId(
                    sceneDriver.getActiveScene()->getSceneId());
            }
        }

        inline void unregisterScene(const UUID &sceneId) {
            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            sceneDriver.removeScene(sceneId);
        }
    } // namespace Detail

    class CreateModuleCmd : public Command {
      public:
        CreateModuleCmd(const std::shared_ptr<Canvas::Scene> &sourceScene,
                        const UUID &netId,
                        const std::string &name = "New Module")
            : m_sourceScene(sourceScene), m_netId(netId), m_nameOverride(name) {
            m_name = "CreateModuleCmd";
        }

        bool execute(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            if (!initialize(simEngine)) {
                return false;
            }

            Detail::ensureSceneRegistered(m_moduleScene);

            BESS_ASSERT(m_addModuleCmd, "[ModuleCmd] Add module command was not initialized");
            if (!m_addModuleCmd->execute(m_sourceScene.get(), simEngine)) {
                return false;
            }

            if (m_boundaryDisconnectCmd) {
                m_boundaryDisconnectCmd->execute(m_sourceScene.get(), simEngine);
            }

            Detail::transferComponents(m_sourceScene.get(), m_moduleScene.get(), m_movedComponents);
            m_executed = true;
            return true;
        }

        void undo(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            BESS_ASSERT(m_executed, "[ModuleCmd] Cannot undo a module command that never executed");

            Detail::transferComponents(m_moduleScene.get(), m_sourceScene.get(), m_movedComponents);
            m_addModuleCmd->undo(m_sourceScene.get(), simEngine);
            if (m_boundaryDisconnectCmd) {
                m_boundaryDisconnectCmd->undo(m_sourceScene.get(), simEngine);
            }

            if (!m_moduleSceneCleanupCmd) {
                m_moduleSceneCleanupCmd = std::make_unique<DeleteCompCmd>(
                    Detail::collectRootComponentIds(m_moduleScene));
            }

            m_moduleSceneCleanupCmd->execute(m_moduleScene.get(), simEngine);
            Detail::unregisterScene(m_moduleScene->getSceneId());
        }

        void redo(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            BESS_ASSERT(m_executed, "[ModuleCmd] Cannot redo a module command that never executed");

            Detail::ensureSceneRegistered(m_moduleScene);

            if (m_moduleSceneCleanupCmd) {
                m_moduleSceneCleanupCmd->undo(m_moduleScene.get(), simEngine);
                Detail::rewireModuleIoIds(m_moduleComponent, m_moduleScene);
            }

            if (m_boundaryDisconnectCmd) {
                m_boundaryDisconnectCmd->redo(m_sourceScene.get(), simEngine);
            }

            m_addModuleCmd->redo(m_sourceScene.get(), simEngine);
            Detail::transferComponents(m_sourceScene.get(), m_moduleScene.get(), m_movedComponents);
        }

        std::shared_ptr<Canvas::ModuleSceneComponent> getModuleComponent() const {
            return m_moduleComponent;
        }

      private:
        bool initialize(SimEngine::SimulationEngine *simEngine) {
            (void)simEngine;
            if (m_initialized) {
                return true;
            }

            BESS_ASSERT(m_sourceScene, "[ModuleCmd] Source scene must be valid");

            auto &mainPageState = Pages::MainPage::getInstance()->getState();
            auto &netCompMap = mainPageState.getNetIdToCompMap(m_sourceScene->getSceneId());
            if (!netCompMap.contains(m_netId) || netCompMap.at(m_netId).empty()) {
                return false;
            }

            UUID moduleInputId = UUID::null;
            UUID moduleOutputId = UUID::null;
            auto createdComponents = Canvas::ModuleSceneComponent::createNew(moduleInputId, moduleOutputId);
            BESS_ASSERT(!createdComponents.empty(), "[ModuleCmd] Failed to create module scene components");

            m_moduleComponent = createdComponents.front()->cast<Canvas::ModuleSceneComponent>();
            BESS_ASSERT(m_moduleComponent, "[ModuleCmd] Created component is not a module");

            m_moduleComponent->setName(m_nameOverride);
            createdComponents.erase(createdComponents.begin());
            m_moduleChildComponents = std::move(createdComponents);

            auto &sceneDriver = mainPageState.getSceneDriver();
            m_moduleScene = sceneDriver.getSceneWithId(m_moduleComponent->getSceneId());
            BESS_ASSERT(m_moduleScene, "[ModuleCmd] Failed to resolve newly created module scene");

            std::vector<UUID> boundaryConnectionIds;
            auto excludedIds = Detail::collectBoundaryExclusionIds(m_sourceScene,
                                                                   m_netId,
                                                                   boundaryConnectionIds);
            if (!boundaryConnectionIds.empty()) {
                m_boundaryDisconnectCmd = std::make_unique<DeleteCompCmd>(boundaryConnectionIds);
            }

            auto movedComponents = Detail::collectComponentsForModule(m_sourceScene,
                                                                      netCompMap.at(m_netId));
            m_movedComponents.reserve(movedComponents.size());
            for (const auto &component : movedComponents) {
                if (!component || excludedIds.contains(component->getUuid())) {
                    continue;
                }
                m_movedComponents.push_back(component);
            }

            m_addModuleCmd =
                std::make_unique<AddCompCmd<Canvas::SimulationSceneComponent>>(m_moduleComponent,
                                                                               m_moduleChildComponents);

            m_initialized = true;
            return true;
        }

      private:
        std::shared_ptr<Canvas::Scene> m_sourceScene;
        std::shared_ptr<Canvas::Scene> m_moduleScene;
        std::shared_ptr<Canvas::ModuleSceneComponent> m_moduleComponent;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_moduleChildComponents;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_movedComponents;
        std::unique_ptr<AddCompCmd<Canvas::SimulationSceneComponent>> m_addModuleCmd;
        std::unique_ptr<DeleteCompCmd> m_boundaryDisconnectCmd;
        std::unique_ptr<DeleteCompCmd> m_moduleSceneCleanupCmd;
        UUID m_netId = UUID::null;
        std::string m_nameOverride;
        bool m_initialized = false;
        bool m_executed = false;
    };

    class DeleteModuleCmd : public Command {
      public:
        DeleteModuleCmd(const std::shared_ptr<Canvas::Scene> &rootScene,
                        const UUID &moduleComponentId)
            : m_rootScene(rootScene), m_moduleComponentId(moduleComponentId) {
            m_name = "DeleteModuleCmd";
        }

        bool execute(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            if (!initialize()) {
                return false;
            }

            const bool deletedRoot = m_rootDeleteCmd->execute(m_rootScene.get(), simEngine);
            const bool deletedModuleScene = m_moduleSceneDeleteCmd->execute(m_moduleScene.get(), simEngine);
            Detail::unregisterScene(m_moduleScene->getSceneId());
            m_executed = deletedRoot || deletedModuleScene;
            return m_executed;
        }

        void undo(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            BESS_ASSERT(m_executed, "[ModuleCmd] Cannot undo a module delete command that never executed");

            Detail::ensureSceneRegistered(m_moduleScene);
            m_moduleSceneDeleteCmd->undo(m_moduleScene.get(), simEngine);
            Detail::rewireModuleIoIds(m_moduleComponent, m_moduleScene);
            m_rootDeleteCmd->undo(m_rootScene.get(), simEngine);
        }

        void redo(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine) override {
            (void)scene;
            BESS_ASSERT(m_executed, "[ModuleCmd] Cannot redo a module delete command that never executed");

            m_rootDeleteCmd->redo(m_rootScene.get(), simEngine);
            m_moduleSceneDeleteCmd->redo(m_moduleScene.get(), simEngine);
            Detail::unregisterScene(m_moduleScene->getSceneId());
        }

      private:
        bool initialize() {
            if (m_initialized) {
                return true;
            }

            BESS_ASSERT(m_rootScene, "[ModuleCmd] Root scene must be valid");

            m_moduleComponent = m_rootScene->getState().getComponentByUuid<Canvas::ModuleSceneComponent>(
                m_moduleComponentId);
            if (!m_moduleComponent) {
                return false;
            }

            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            m_moduleScene = sceneDriver.getSceneWithId(m_moduleComponent->getSceneId());
            BESS_ASSERT(m_moduleScene, "[ModuleCmd] Module scene not found for deletion");

            m_rootDeleteCmd = std::make_unique<DeleteCompCmd>(std::vector<UUID>{m_moduleComponentId});
            m_moduleSceneDeleteCmd = std::make_unique<DeleteCompCmd>(
                Detail::collectRootComponentIds(m_moduleScene));

            m_initialized = true;
            return true;
        }

      private:
        std::shared_ptr<Canvas::Scene> m_rootScene;
        std::shared_ptr<Canvas::Scene> m_moduleScene;
        std::shared_ptr<Canvas::ModuleSceneComponent> m_moduleComponent;
        std::unique_ptr<DeleteCompCmd> m_rootDeleteCmd;
        std::unique_ptr<DeleteCompCmd> m_moduleSceneDeleteCmd;
        UUID m_moduleComponentId = UUID::null;
        bool m_initialized = false;
        bool m_executed = false;
    };
} // namespace Bess::Cmd
