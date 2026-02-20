#pragma once

#include "common/bess_uuid.h"
#include "command.h"
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

        DeleteCompCmd(const std::vector<UUID> &compUuids) : m_compUuids(compUuids) {
            m_name = "DeleteComponentCmd";
        }

        [[nodiscard]] size_t exploreChildren(const Canvas::SceneState &sceneState, const UUID &compUuid) {
            size_t count = 0;
            auto children = sceneState.getComponentByUuid(compUuid)->getChildComponents();
            for (const auto &childUuid : children) {
                count++;
                m_deletedComponents.push_back(sceneState.getComponentByUuid(childUuid));
                count += exploreChildren(sceneState, childUuid);
            }
            return count;
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {

            m_deletedComponents.clear();

            auto &sceneState = scene->getState();

            std::vector<UUID> notConnectionComps;

            // Handeling connections first
            for (const auto &compUuid : m_compUuids) {
                auto comp = sceneState.getComponentByUuid(compUuid);
                if (comp->getType() == Canvas::SceneComponentType::connection) {
                    const auto count = exploreChildren(sceneState, compUuid);
                    auto compRef = comp;
                    if (!sceneState.removeComponent(compUuid, UUID::master).empty()) {
                        m_deletedComponents.push_back(std::move(compRef));
                    } else {
                        m_deletedComponents.resize(m_deletedComponents.size() - count);
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

                    const auto count = exploreChildren(sceneState, compUuid);
                    auto compRef = comp;
                    if (!sceneState.removeComponent(compUuid, UUID::master).empty()) {
                        m_deletedComponents.push_back(std::move(compRef));
                    } else {
                        m_deletedComponents.resize(m_deletedComponents.size() - count);
                    }
                }
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
        std::vector<UUID> m_compUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_deletedComponents;
    };
} // namespace Bess::Cmd
