#include "group_scene_component.h"
#include "common/bess_uuid.h"
#include "pages/main_page/main_page.h"
#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    GroupSceneComponent::GroupSceneComponent() = default;

    std::vector<std::shared_ptr<SceneComponent>> GroupSceneComponent::clone(const SceneState &sceneState) const {
        auto clonedComponent = std::make_shared<GroupSceneComponent>(*this);
        prepareClone(*clonedComponent);

        std::vector<std::shared_ptr<SceneComponent>> clonedComponents;
        clonedComponents.push_back(clonedComponent);

        for (const auto &childId : m_childComponents) {
            const auto childComponent = sceneState.getComponentByUuid(childId);
            BESS_ASSERT(childComponent, "Group child component was not found during clone");

            const auto childClones = childComponent->clone(sceneState);
            BESS_ASSERT(!childClones.empty(), "Group child clone returned no components");

            clonedComponent->addChildComponent(childClones.front()->getUuid());
            childClones.front()->setParentComponent(clonedComponent->getUuid());
            clonedComponents.insert(clonedComponents.end(), childClones.begin(), childClones.end());
        }

        return clonedComponents;
    }

    void GroupSceneComponent::onAttach(SceneState &state) {
        m_transform.position = glm::vec3(0.0);
        for (const auto &childId : m_childComponents) {
            state.attachChild(m_uuid, childId);
        }
    }

    std::vector<UUID> GroupSceneComponent::cleanup(SceneState &state, UUID caller) {
        for (const auto &childId : m_childComponents) {
            auto childComp = state.getComponentByUuid(childId);
            if (childComp) {
                state.orphanComponent(childId);
            }
        }
        return {};
    }

    void GroupSceneComponent::onSelect() {
        if (!m_isSelected)
            return;

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneState = mainPageState.getSceneDriver()->getState();
        for (const auto &childId : m_childComponents) {
            auto childComp = sceneState.getComponentByUuid(childId);
            if (childComp) {
                sceneState.addSelectedComponent(childId);
            }
        }
    }
} // namespace Bess::Canvas
