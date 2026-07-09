#pragma once

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/toggle_btn_comp.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "sim_scene_component.h"

namespace Bess::Canvas {
    class InputSceneComponent : public SimulationSceneComponent {
      public:
        InputSceneComponent();

        ~InputSceneComponent() override = default;

        REG_SCENE_COMP_TYPE("InputSceneComponent",
                            SceneComponentType::simulation)
        SCENE_COMP_SER_NP(Bess::Canvas::InputSceneComponent,
                          Bess::Canvas::SimulationSceneComponent)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;

        void calculateSchematicScale(const SceneState &state) override;

        void update(TimeMs ts, SceneState &state) override;

        void prepareUI(SceneUIPrepareCtx &ctx) override;

        std::vector<UUID> getDependants(const SceneState &state) const override;

      private:
        std::vector<std::shared_ptr<Bess::Canvas::UI::UISceneComponent>>
            m_inputCtrls;
        std::vector<SimEngine::SignalKind> m_inpSignalKinds;

        bool m_setBtnCbs = false;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP_NP(Bess::Canvas::InputSceneComponent,
                  Bess::Canvas::SimulationSceneComponent)
