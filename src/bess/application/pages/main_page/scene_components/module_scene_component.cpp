#include "module_scene_component.h"
#include "common/bess_uuid.h"
#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    void ModuleSceneComponent::onAttach(SceneState &state) {
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state, UUID caller) {
        return {};
    }

    void ModuleSceneComponent::onSelect() {
    }
} // namespace Bess::Canvas
