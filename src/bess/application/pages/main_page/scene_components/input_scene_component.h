#pragma once

#include "common/bess_uuid.h"
#include "scene/scene_state/scene_state.h"
#include "scene_draw_context.h"
#include "sim_scene_component.h"

#define INPSC_SER_PROPS ("isModuleInput", getIsModuleInput, setIsModuleInput)

namespace Bess::Canvas {
    class InputSceneComponent : public SimulationSceneComponent {
      public:
        InputSceneComponent() = default;

        ~InputSceneComponent() override = default;

        REG_SCENE_COMP_TYPE("InputSceneComponent", SceneComponentType::simulation)
        SCENE_COMP_SER(Bess::Canvas::InputSceneComponent,
                       Bess::Canvas::SimulationSceneComponent,
                       INPSC_SER_PROPS)

        void draw(SceneDrawContext &context) override;

        void drawPropertiesUI() override;

        void calculateSchematicScale(SceneState &state) override;

        void drawToggleButton(SceneDrawContext &context,
                              UUID slotUuid,
                              int buttonIndex);

        void onMouseHovered(const Events::MouseHoveredEvent &e) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;

        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        MAKE_GETTER_SETTER(bool, IsModuleInput, m_isModuleInput)

      private:
        bool m_isModuleInput = false;
    };
} // namespace Bess::Canvas

// REFLECT_DERIVED_EMPTY(Bess::Canvas::InputSceneComponent, Bess::Canvas::SimulationSceneComponent)

REG_SCENE_COMP(Bess::Canvas::InputSceneComponent,
               Bess::Canvas::SimulationSceneComponent,
               INPSC_SER_PROPS)
