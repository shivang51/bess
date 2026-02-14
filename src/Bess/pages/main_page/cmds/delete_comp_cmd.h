#pragma once

#include "bess_uuid.h"
#include "command.h"
#include "scene/scene.h"
#include "scene/scene_state/components/scene_component.h"
#include <ranges>
#include <vector>

namespace Bess::Cmd {

    class DeleteCompCmd : public Bess::Cmd::Command {
      public:
        DeleteCompCmd() = default;
        DeleteCompCmd(const std::vector<UUID> &compUuids) : m_compUuids(compUuids) {}

        void exploreChildren(const Canvas::SceneState &sceneState, const UUID &compUuid) {
            auto children = sceneState.getComponentByUuid(compUuid)->getChildComponents();
            for (const auto &childUuid : children) {
                m_deletedComponents.push_back(sceneState.getComponentByUuid(childUuid));
                exploreChildren(sceneState, childUuid);
            }
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {

            m_deletedComponents.clear();
            for (const auto &compUuid : m_compUuids) {
                auto comp = scene->getState().getComponentByUuid(compUuid);
                if (comp->getParentComponent() == UUID::null) {
                    m_deletedComponents.push_back(comp);
                    exploreChildren(scene->getState(), compUuid);
                    scene->deleteSceneEntity(compUuid);
                }
            }

            return true;
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            for (auto &deletedComponent : std::ranges::reverse_view(m_deletedComponents)) {
                scene->addComponent(deletedComponent, false);
            }
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {

            for (const auto &comp : m_deletedComponents) {
                if (comp->getParentComponent() == UUID::null) {
                    scene->deleteSceneEntity(comp->getUuid());
                }
            }
        }

      private:
        std::vector<UUID> m_compUuids;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_deletedComponents;
    };
} // namespace Bess::Cmd
