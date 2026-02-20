#pragma once
#include "command.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "scene/scene.h"
#include "scene/scene_state/components/scene_component.h"
#include "simulation_engine.h"
#include <memory>

namespace Bess::Cmd {
    template <typename TComponent>
    class AddCompCmd : public Bess::Cmd::Command {
      public:
        AddCompCmd() {
            m_name = "AddComponentCmd";
        }

        AddCompCmd(std::shared_ptr<TComponent> comp) : m_comp(std::move(comp)) {
            m_name = "AddComponentCmd";
        }

        AddCompCmd(std::shared_ptr<TComponent> comp,
                   const std::vector<std::shared_ptr<Canvas::SceneComponent>> &childComponents)
            : m_comp(std::move(comp)), m_childComponents(childComponents) {
            m_name = "AddComponentCmd";
        }

        bool execute(Canvas::Scene *scene,
                     SimEngine::SimulationEngine *simEngine) override {
            if (!m_comp) {
                return false;
            }
            scene->addComponent(m_comp);

            auto &sceneState = scene->getState();
            for (const auto &childComp : m_childComponents) {
                sceneState.addComponent(childComp);
                sceneState.attachChild(m_comp->getUuid(), childComp->getUuid());
            }

            return true;
        }

        void undo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            BESS_ASSERT(m_comp, "Cannot undo AddCompCmd without a valid component");

            scene->deleteSceneEntity(m_comp->getUuid());
        }

        void redo(Canvas::Scene *scene,
                  SimEngine::SimulationEngine *simEngine) override {
            BESS_ASSERT(m_comp, "Cannot redo AddCompCmd without a valid component");

            scene->addComponent(m_comp, false);

            auto &sceneState = scene->getState();
            for (const auto &childComp : m_childComponents) {
                sceneState.addComponent(childComp);
                sceneState.attachChild(m_comp->getUuid(), childComp->getUuid());
            }
        }

      private:
        std::shared_ptr<TComponent> m_comp = nullptr;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_childComponents;
    };

} // namespace Bess::Cmd
