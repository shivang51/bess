#include "input_scene_component.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "icons/FontAwesomeIcons.h"
#include "scene/scene_draw_helpers.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_widgets.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"

namespace Bess::Canvas {
    InputSceneComponent::InputSceneComponent() {
        m_icon = UI::Icons::FontAwesomeIcons::FA_TOGGLE_OFF;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    InputSceneComponent::clone(const SceneState &sceneState) const {
        auto clonedComponent = std::make_shared<InputSceneComponent>(*this);
        return cloneSimulationComponent(sceneState, clonedComponent);
    }

    void InputSceneComponent::draw(SceneDrawContext &context) {

        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        SimulationSceneComponent::draw(context);

        int i = 0;
        for (const auto &slotUuid : m_outputSlots) {
            drawToggleButton(context, slotUuid, i++);
        }
    }

    void InputSceneComponent::drawToggleButton(SceneDrawContext &context,
                                               UUID slotUuid, int buttonIndex) {
        constexpr float buttonWidth = 30.f;
        constexpr float buttonHeight = Styles::SIM_COMP_SLOT_ROW_SIZE -
                                       (Styles::simCompStyles.rowMargin * 2.f);
        constexpr glm::vec2 buttonSize = glm::vec2(buttonWidth, buttonHeight);

        const auto &state = *context.sceneState;
        const auto slotComp =
            state.getComponentByUuid<SlotSceneComponent>(slotUuid);
        if (!slotComp) {
            return;
        }

        const auto slotType = slotComp->getSlotType();

        if (slotType == SlotType::inputsResize ||
            slotType == SlotType::outputsResize) {
            return;
        }

        const auto slotPosY = slotComp->getAbsolutePosition(state).y;
        const bool isHigh = slotComp->getSlotState(state).getLogicState() ==
                            SimEngine::LogicState::high;

        const float buttonPosX =
            m_transform.position.x - (m_transform.scale.x / 2.f) +
            Styles::simCompStyles.paddingX + (buttonSize.x / 2.f);
        const glm::vec3 buttonPos =
            glm::vec3(buttonPosX, slotPosY, m_transform.position.z + 0.001f);

        const auto pickingId =
            PickingId{m_runtimeId, static_cast<uint32_t>(buttonIndex + 1)};

        bool nextValue = isHigh;
        if (SceneWidgets::toggleButton(pickingId, &nextValue, buttonPos,
                                       buttonSize, context)) {
            const auto slotParentComp =
                state.getComponentByUuid<SimulationSceneComponent>(
                    slotComp->getParentComponent());
            if (!slotParentComp) {
                return;
            }

            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            if (!projectCtx) {
                return;
            }

            auto &simEngine = projectCtx->getSimEngine();

            simEngine.setOutputSlotState(
                slotParentComp->getSimEngineId(), slotComp->getIndex(),
                nextValue ? SimEngine::LogicState::high
                          : SimEngine::LogicState::low);
        }

        // Button label
        const std::string label = nextValue ? "1" : "0";
        const auto textSize = context.renderer->measureText(
            label, {.fontSize = Styles::simCompStyles.slotLabelSize});

        const float textPosX = buttonPos.x + (buttonSize.x / 2.f) + 8.f;
        const glm::vec3 textPos =
            glm::vec3(textPosX,
                      slotPosY + (textSize.y / 2.f) -
                          1.f, // FIXME: why -2.f, maybe the baseline?
                      m_transform.position.z + 0.001f);

        SceneDraw::drawText(
            context, label, textPos, Styles::simCompStyles.slotLabelSize,
            ViewportTheme::colors.text, PickingId{m_runtimeId, 0});
    }

    void InputSceneComponent::calculateSchematicScale(const SceneState &state) {
        SimulationSceneComponent::calculateSchematicScale(state);
        m_schematicTransform.scale.x = 50.f;
    }
} // namespace Bess::Canvas
