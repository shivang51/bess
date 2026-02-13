#pragma once

#include "bess_uuid.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/scene_state.h"
#include <memory>

namespace Bess::Canvas {
    class InputSceneComponent : public SimulationSceneComponent {
      public:
        static constexpr const char *subType = "InputSceneComponent";
        InputSceneComponent() {
            m_subType = subType;
        }

        ~InputSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void calculateSchematicScale(SceneState &state,
                                     const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer) override;

        void drawToggleButton(SceneState &state,
                              const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer,
                              const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer,
                              UUID slotUuid,
                              int buttonIndex);

        void onMouseHovered(const Events::MouseHoveredEvent &e) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;

        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;
    };
} // namespace Bess::Canvas

REFLECT_DERIVED_EMPTY(Bess::Canvas::InputSceneComponent, Bess::Canvas::SimulationSceneComponent)
