#include "scene/scene_state/components/non_sim_scene_component.h"
#include <unordered_map>

namespace Bess::Canvas {
    NonSimSceneComponent::NonSimSceneComponent() {
        initDragBehaviour();
    }

    std::unordered_map<std::type_index, std::string> NonSimSceneComponent::registry;
    std::unordered_map<std::type_index, std::function<std::shared_ptr<NonSimSceneComponent>()>> NonSimSceneComponent::m_contrRegistry;

    std::shared_ptr<NonSimSceneComponent> NonSimSceneComponent::getInstance(std::type_index tIdx) {
        if (m_contrRegistry.contains(tIdx)) {
            return m_contrRegistry[tIdx]();
        }
        return nullptr;
    }
} // namespace Bess::Canvas
