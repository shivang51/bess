#pragma once

#include "command.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/services/connection_service.h"
#include "scene/scene.h"
#include "scene/scene_state/components/scene_component.h"
#include <vector>

namespace Bess::Cmd {
    // bool indicated if its undo/redo, true if its undo
    // vector is list of processed comps
    typedef std::function<void(bool,
                               const std::vector<std::shared_ptr<Canvas::SceneComponent>> &)>
        DeleteCompCmdCB;

    class DeleteCompCmd : public Bess::Cmd::Command {
      public:
        DeleteCompCmd() {
            m_name = "DeleteComponentCmd";
        }

        DeleteCompCmd(const std::vector<UUID> &compUuids,
                      const DeleteCompCmdCB &callback = nullptr) : m_callback(callback) {
            m_compUuids = std::set<UUID>(compUuids.begin(), compUuids.end());
            m_name = "DeleteComponentCmd";
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {

            m_deletedComponents.clear();

            auto &sceneState = scene->getState();

            std::vector<UUID> notConnectionComps;
            std::unordered_map<UUID, std::vector<UUID>> dependantsMap;
            std::unordered_set<UUID> unqDependants;

            for (const auto &compUuid : m_compUuids) {
                const auto &comp = sceneState.getComponentByUuid(compUuid);
                if (!comp) {
                    BESS_WARN("Component with uuid {} not found in scene state. Skipping deletion.",
                              (uint64_t)compUuid);
                    continue;
                }

                BESS_DEBUG("Collecting dependants for {} | {}", (uint64_t)compUuid, comp->getName());

                const auto &dependants = comp->getDependants(sceneState);

                std::vector<UUID> unqDependantsList;

                for (const auto &dependantUuid : dependants) {
                    if (!unqDependants.contains(dependantUuid)) {
                        unqDependantsList.push_back(dependantUuid);
                        unqDependants.insert(dependantUuid);
                    }
                }

                if (unqDependantsList.empty())
                    continue;

                dependantsMap[compUuid] = std::move(unqDependantsList);
            }

            for (const auto &[compId, dependants] : dependantsMap) {
                BESS_DEBUG("Component {} with uuid {} has {} dependants",
                           sceneState.getComponentByUuid(compId)->getName(),
                           (uint64_t)compId, dependants.size());
                for (const auto &depId : dependants) {
                    const auto &comp = sceneState.getComponentByUuid(depId);
                    BESS_DEBUG("Got Dependant {} | {}", comp->getName(), (uint64_t)depId);
                }
            }

            // Storing components in a order so that cleanup and restoration is correct.
            for (const auto &[compId, dependants] : dependantsMap) {
                for (const auto &dependantUuid : dependants) {
                    if (!sceneState.isComponentValid(dependantUuid)) {
                        BESS_WARN("Dependant component with uuid {} not found in scene state. Skipping deletion.",
                                  (uint64_t)dependantUuid);
                        continue;
                    }

                    // ignoring the dependant which was passed explicitly as well.
                    if (m_compUuids.contains(dependantUuid)) {
                        BESS_DEBUG("Ignoring dependant {} as it was also explicitly passed",
                                   (uint64_t)dependantUuid);
                        continue;
                    }

                    auto comp = sceneState.getComponentByUuid(dependantUuid);
                    m_deletedComponents.push_back(std::move(comp));
                }

                auto comp = sceneState.getComponentByUuid(compId);
                m_deletedComponents.push_back(std::move(comp));
            }

            for (const auto &compUuid : m_compUuids) {
                if (!dependantsMap.contains(compUuid)) {
                    auto comp = sceneState.getComponentByUuid(compUuid);
                    if (comp) {
                        m_deletedComponents.push_back(std::move(comp));
                    } else {
                        BESS_WARN("Component with uuid {} not found in scene state. Skipping deletion.",
                                  (uint64_t)compUuid);
                    }
                }
            }

            std::unordered_set<UUID> deletedUuids;

            auto &connectionsSvc = Svc::SvcConnection::instance();

            // Handeling connections first
            for (const auto &compUuid : m_compUuids) {
                auto comp = sceneState.getComponentByUuid(compUuid);
                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    const auto &removedUuids = connectionsSvc.removeConnection(
                        comp->cast<Canvas::ConnectionSceneComponent>());
                    if (!removedUuids.empty()) {
                        deletedUuids.insert_range(removedUuids);
#ifdef DEBUG
                        if (dependantsMap.contains(compUuid)) {
                            const auto &count = dependantsMap.at(compUuid).size();
                            if (count == removedUuids.size() - 1) {
                                BESS_DEBUG("Deleted dependants count {} does not match expected count {} for component {}.",
                                           removedUuids.size() - 1, count, (uint64_t)compUuid);
                            }
                        }
#endif
                    } else {
                        BESS_WARN("Failed to delete connection component {}", (uint64_t)compUuid);
                    }
                } else {
                    notConnectionComps.push_back(compUuid);
                }
            }

            for (const auto &compUuid : notConnectionComps) {
                auto comp = sceneState.getComponentByUuid(compUuid);
                if (comp) {
                    const auto removedUuids = sceneState.removeComponent(compUuid, UUID::master);
                    if (!removedUuids.empty()) {
                        deletedUuids.insert_range(removedUuids);
#ifdef DEBUG
                        if (dependantsMap.contains(compUuid)) {
                            const auto &count = dependantsMap.at(compUuid).size();
                            if (count != removedUuids.size() - 1) {
                                BESS_WARN("Deleted dependants count {} does not match expected count {} for component {}.",
                                          removedUuids.size() - 1, count, (uint64_t)compUuid);
                                for (const auto &id : removedUuids) {
                                    BESS_DEBUG("Removed id {}", (uint64_t)id);
                                }
                            }
                        }
#endif
                    } else {
                        BESS_WARN("Failed to delete component {}", (uint64_t)compUuid);
                    }
                }
            }

            const auto expectedDeletedCount = m_deletedComponents.size();
            const auto deletedCount = deletedUuids.size();

            if (deletedCount != expectedDeletedCount) {
                BESS_WARN("Deleted components count {} does not match expected count {}.",
                          deletedCount, expectedDeletedCount);
            }

            if (deletedCount != m_deletedComponents.size()) {
                BESS_WARN("Deleted components count {} does not match stored deleted components count {}.",
                          deletedCount, m_deletedComponents.size());
                if (deletedCount > m_deletedComponents.size()) {
                    for (const auto &uuid : deletedUuids) {
                        if (std::ranges::none_of(m_deletedComponents, [&uuid](const auto &comp) { return comp->getUuid() == uuid; })) {
                            BESS_DEBUG("Component with uuid {} was deleted but not stored in deleted components list.",
                                       (uint64_t)uuid);
                        }
                    }
                }
            }

            BESS_ASSERT(deletedCount == m_deletedComponents.size(),
                        "Deleted components count does not match stored deleted components count");

            for (const auto &comp : m_deletedComponents) {
                BESS_DEBUG("Deleted component: {} with uuid {}", comp->getName(), (uint64_t)comp->getUuid());
            }

            return !m_deletedComponents.empty();
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            auto &sceneState = scene->getState();

            auto &connectionsSvc = Svc::SvcConnection::instance();
            std::vector<std::shared_ptr<Canvas::SceneComponent>> groupComps;
            for (const auto &deletedComponent : std::ranges::reverse_view(m_deletedComponents)) {
                BESS_DEBUG("Restoring component: {} with uuid {}", deletedComponent->getName(),
                           (uint64_t)deletedComponent->getUuid());
                if (deletedComponent->getType() == Canvas::SceneComponentType::connection) {
                    connectionsSvc.addConnection(deletedComponent->cast<Canvas::ConnectionSceneComponent>());
                } else {
                    sceneState.addComponent(deletedComponent,
                                            deletedComponent->getType() != Canvas::SceneComponentType::group);
                }

                if (deletedComponent->getType() == Canvas::SceneComponentType::group) {
                    groupComps.push_back(deletedComponent);
                }

                const auto &parentUuid = deletedComponent->getParentComponent();

                if (parentUuid == UUID::null)
                    continue;

                const auto &parentComp = sceneState.getComponentByUuid(parentUuid);

                if (!parentComp) {
                    BESS_ERROR("Parent  {} not found for {} during undo of del cmd.",
                               (uint64_t)parentUuid, deletedComponent->getName());
                    BESS_ASSERT(false, "Parent component not found during undo of delete command");
                    continue;
                }

                sceneState.attachChild(deletedComponent->getParentComponent(),
                                       deletedComponent->getUuid());
            }

            for (const auto &groupComp : groupComps) {
                groupComp->onAttach(sceneState);
            }

            if (m_callback) {
                m_callback(true, m_deletedComponents);
            }
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            auto &connectionsSvc = Svc::SvcConnection::instance();
            for (const auto &comp : m_deletedComponents) {
                bool shouldDelete = true;

                if (comp->getParentComponent() != UUID::null) {
                    const auto parentComp = scene->getState().getComponentByUuid(
                        comp->getParentComponent());
                    if (parentComp) {
                        shouldDelete = parentComp->getType() == Canvas::SceneComponentType::group;
                    } else {
                        BESS_ERROR("Parent {} not found for component {} during redo of del cmd.",
                                   (uint64_t)comp->getParentComponent(), comp->getName());
                        BESS_ASSERT(false, "Parent component not found during redo of delete command");
                    }
                }

                if (!shouldDelete)
                    continue;

                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    connectionsSvc.removeConnection(comp->cast<Canvas::ConnectionSceneComponent>());
                } else {
                    scene->getState().removeComponent(comp->getUuid(), UUID::master);
                }
            }

            if (m_callback) {
                m_callback(false, m_deletedComponents);
            }
        }

      private:
        std::set<UUID> m_compUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_deletedComponents;
        DeleteCompCmdCB m_callback = nullptr;
    };
} // namespace Bess::Cmd
