#include "group_scene_component.h"
#include "bess_uuid.h"
#include "pages/main_page/main_page.h"
#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    void GroupSceneComponent::onAttach(SceneState &state) {
        for (const auto &childId : m_childComponents) {
            state.attachChild(m_uuid, childId);
        }
    }

    std::vector<UUID> GroupSceneComponent::cleanup(SceneState &state, UUID caller) {
        for (const auto &childId : m_childComponents) {
            state.orphanComponent(childId);
        }
        return {};
    }

    void GroupSceneComponent::onSelect() {
        if (!m_isSelected)
            return;

        auto &mainPageState = Pages::MainPage::getTypedInstance()->getState();
        auto &sceneState = mainPageState.getSceneDriver()->getState();
        for (const auto &childId : m_childComponents) {
            auto childComp = sceneState.getComponentByUuid(childId);
            if (childComp) {
                sceneState.addSelectedComponent(childId);
            }
        }
    }
} // namespace Bess::Canvas
