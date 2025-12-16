#pragma once

#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"

namespace Bess::Canvas {

    class ConnSegSceneComponent : public SceneComponent {
    };

    class ConnectionSceneComponent : public SceneComponent {
      public:
        ConnectionSceneComponent() = default;
        ConnectionSceneComponent(const ConnectionSceneComponent &other) = default;
        ConnectionSceneComponent(UUID uuid);

        ConnectionSceneComponent(UUID uuid, const Transform &transform);
        ~ConnectionSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        REG_SCENE_COMP(SceneComponentType::connection)

        MAKE_GETTER_SETTER(UUID, FromSlotId, m_fromSlotId)
        MAKE_GETTER_SETTER(UUID, ToSlotId, m_toSlotId)

      private:
        UUID m_fromSlotId = UUID::null;
        UUID m_toSlotId = UUID::null;
        std::shared_ptr<SlotSceneComponent> m_startSlot;
        std::shared_ptr<SlotSceneComponent> m_endSlot;
    };
} // namespace Bess::Canvas
