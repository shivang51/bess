#include "group_scene_component.h"
#include "bess_uuid.h"
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

} // namespace Bess::Canvas
