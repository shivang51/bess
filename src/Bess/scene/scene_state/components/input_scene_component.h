#pragma once

#include "bess_uuid.h"
#include "scene/scene_state/components/sim_scene_component.h"

namespace Bess::Canvas {
    class InputSceneComponent : public SimulationSceneComponent {
      public:
        InputSceneComponent() = default;
        ~InputSceneComponent() override = default;

        InputSceneComponent(UUID uuid);

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawToggleButton(SceneState &state,
                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                              std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer,
                              UUID slotUuid,
                              int buttonIndex);

        void onMouseHovered(const Events::MouseHoveredEvent &e) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;

        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;
    };
} // namespace Bess::Canvas
