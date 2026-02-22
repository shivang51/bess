#pragma once

#include "command.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "scene/scene.h"
#include "scene/scene_state/components/scene_component.h"
#include <vector>

namespace Bess::Cmd {
    class DeleteCompCmd : public Bess::Cmd::Command {
      public:
        DeleteCompCmd() {
            m_name = "DeleteComponentCmd";
        }

        DeleteCompCmd(const std::vector<UUID> &compUuids) {
            m_compUuids = std::set<UUID>(compUuids.begin(), compUuids.end());
            m_name = "DeleteComponentCmd";
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {

            m_deletedComponents.clear();

            auto &sceneState = scene->getState();

            std::vector<UUID> notConnectionComps;
            std::set<UUID> dependantsToDelete;
            std::unordered_map<UUID, size_t> dependantsCount;
            std::vector<std::shared_ptr<Canvas::SceneComponent>> notConnDependants;

            for (const auto &compUuid : m_compUuids) {
                const auto &comp = sceneState.getComponentByUuid(compUuid);
                if (!comp) {
                    BESS_WARN("Component with uuid {} not found in scene state. Skipping deletion.",
                              (uint64_t)compUuid);
                    continue;
                }

                const auto &dependants = sceneState.getLifeDependants(compUuid);
                dependantsToDelete.insert(dependants.begin(), dependants.end());
                dependantsCount[compUuid] = dependants.size();
            }

            // Storing dependants
            for (const auto &dependantUuid : dependantsToDelete) {
                if (!sceneState.isComponentValid(dependantUuid)) {
                    BESS_WARN("Dependant component with uuid {} not found in scene state. Skipping deletion.",
                              (uint64_t)dependantUuid);
                    continue;
                }

                // deleting from passed comp list if it was also explictly passed
                if (m_compUuids.contains(dependantUuid)) {
                    m_compUuids.erase(dependantUuid);
                }

                auto comp = sceneState.getComponentByUuid(dependantUuid);
                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    m_deletedComponents.push_back(comp);
                } else {
                    notConnDependants.push_back(comp);
                }
            }

            for (const auto &comp : notConnDependants) {
                m_deletedComponents.push_back(comp);
            }

            notConnDependants.clear();

            size_t deletedCount = 0;

            // Handeling connections first
            for (const auto &compUuid : m_compUuids) {
                auto comp = sceneState.getComponentByUuid(compUuid);
                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    auto compRef = comp;
                    const auto removedUuids = sceneState.removeComponent(compUuid, UUID::master);
                    if (!removedUuids.empty()) {
                        deletedCount += removedUuids.size();
                        m_deletedComponents.push_back(std::move(compRef));
#ifdef DEBUG
                        if (dependantsCount.contains(compUuid)) {
                            const auto &count = dependantsCount[compUuid];
                            if (count != removedUuids.size() - 1) {
                                BESS_WARN("Deleted dependants count {} does not match expected count {} for component {}.",
                                          count, removedUuids.size() - 1, (uint64_t)compUuid);
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
                    bool shouldDelete = true;
                    if (comp->getParentComponent() != UUID::null) {
                        const auto parentComp = sceneState.getComponentByUuid(
                            comp->getParentComponent());
                        shouldDelete = parentComp->getType() == Canvas::SceneComponentType::group;
                    }

                    if (!shouldDelete)
                        continue;

                    auto compRef = comp;
                    const auto removedUuids = sceneState.removeComponent(compUuid, UUID::master);
                    if (!removedUuids.empty()) {
                        deletedCount += removedUuids.size();
                        m_deletedComponents.push_back(std::move(compRef));
#ifdef DEBUG
                        if (dependantsCount.contains(compUuid)) {
                            const auto &count = dependantsCount[compUuid];
                            if (count != removedUuids.size() - 1) {
                                BESS_WARN("Deleted dependants count {} does not match expected count {} for component {}.",
                                          count, removedUuids.size() - 1, (uint64_t)compUuid);
                            }
                        }
#endif
                    } else {
                        BESS_WARN("Failed to delete component {}", (uint64_t)compUuid);
                    }
                }
            }

            const auto expectedDeletedCount = dependantsToDelete.size() + m_compUuids.size();
            if (deletedCount != expectedDeletedCount) {
                BESS_WARN("Deleted components count {} does not match expected dependants count {}.",
                          deletedCount, expectedDeletedCount);
            }

            if (deletedCount != m_deletedComponents.size()) {
                BESS_WARN("Deleted components count {} does not match stored deleted components count {}.",
                          deletedCount, m_deletedComponents.size());
            }

            return !m_deletedComponents.empty();
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {

            auto &sceneState = scene->getState();
            for (auto &deletedComponent : std::ranges::reverse_view(m_deletedComponents)) {
                sceneState.addComponent(deletedComponent);

                const auto &parentUuid = deletedComponent->getParentComponent();
                const auto &parentComp = sceneState.getComponentByUuid(parentUuid);

                if (parentUuid == UUID::null || !parentComp)
                    continue;

                if (parentComp->getType() != Canvas::SceneComponentType::group)
                    continue;

                sceneState.attachChild(deletedComponent->getParentComponent(),
                                       deletedComponent->getUuid());
            }
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {

            for (const auto &comp : m_deletedComponents) {
                bool shouldDelete = true;

                if (comp->getParentComponent() != UUID::null) {
                    const auto parentComp = scene->getState().getComponentByUuid(
                        comp->getParentComponent());
                    shouldDelete = parentComp->getType() == Canvas::SceneComponentType::group;
                }

                if (!shouldDelete)
                    continue;

                scene->getState().removeComponent(comp->getUuid(), UUID::master);
            }
        }

      private:
        std::set<UUID> m_compUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_deletedComponents;
    };
} // namespace Bess::Cmd
