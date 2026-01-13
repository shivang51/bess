#pragma once

#include "bess_uuid.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/scene_state.h"

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

        void calculateSchematicScale(SceneState &state) override;

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

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::InputSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::InputSceneComponent &component);
} // namespace Bess::JsonConvert
