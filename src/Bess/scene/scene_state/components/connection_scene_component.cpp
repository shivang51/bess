#include "scene/scene_state/components/connection_scene_component.h"

namespace Bess::Canvas {
    ConnectionSceneComponent::ConnectionSceneComponent(UUID uuid)
        : SceneComponent(uuid) {}

    ConnectionSceneComponent::ConnectionSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {}

    void ConnectionSceneComponent::draw(SceneState &state,
                                        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
    }
} // namespace Bess::Canvas
