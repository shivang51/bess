#include "input_scene_component.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/renderer/colors.h"
#include "icons/FontAwesomeIcons.h"
#include "scene/scene_draw_helpers.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_widgets.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include <string_view>

namespace Bess::Canvas {
    namespace {
        bool makeAllLow = false;

        bool parseBinaryValue(std::string_view text, bool &value) {
            if (text == "0") {
                value = false;
                return true;
            }

            if (text == "1") {
                value = true;
                return true;
            }

            return false;
        }

        bool setOutputSlotState(
            const SceneState &state,
            const std::shared_ptr<SlotSceneComponent> &slotComp, bool isHigh) {
            const auto slotParentComp =
                state.getComponentByUuid<SimulationSceneComponent>(
                    slotComp->getParentComponent());
            if (!slotParentComp) {
                return false;
            }

            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            if (!projectCtx) {
                return false;
            }

            auto &simEngine = projectCtx->getSimEngine();
            simEngine.setOutputSlotState(
                slotParentComp->getSimEngineId(), slotComp->getIndex(),
                isHigh ? SimEngine::LogicState::high
                       : SimEngine::LogicState::low);
            return true;
        }
    }

    InputSceneComponent::InputSceneComponent() {
        m_icon = UI::Icons::FontAwesomeIcons::FA_TOGGLE_OFF;
    }

    void InputSceneComponent::update(TimeMs ts, SceneState &state) {
        SimulationSceneComponent::update(ts, state);
        if (makeAllLow) {
            makeAllLow = false;
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            BESS_ASSERT(projectCtx, "ProjectContext not found in AppContext");

            auto &simEngine = projectCtx->getSimEngine();

            for (const auto &slotUuid : m_outputSlots) {
                const auto slotComp =
                    state.getComponentByUuid<SlotSceneComponent>(slotUuid);
                if (!slotComp) {
                    continue;
                }

                const auto slotType = slotComp->getSlotType();
                if (slotType != SlotType::outputsResize) {
                    const auto slotParentComp =
                        state.getComponentByUuid<SimulationSceneComponent>(
                            slotComp->getParentComponent());
                    if (!slotParentComp) {
                        continue;
                    }

                    simEngine.setOutputSlotState(
                        slotParentComp->getSimEngineId(), slotComp->getIndex(),
                        SimEngine::LogicState::low);
                }
            }
        }
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

        BESS_ASSERT(slotType == SlotType::digitalOutput ||
                        slotType == SlotType::outputsResize,
                    "Unexpected slot type for input component: {}",
                    static_cast<int>(slotType));

        const auto slotPosY = slotComp->getAbsolutePosition(state).y;

        const float buttonPosX =
            m_transform.position.x - (m_transform.scale.x / 2.f) +
            Styles::simCompStyles.paddingX + (buttonSize.x / 2.f);

        const glm::vec3 buttonPos =
            glm::vec3(buttonPosX, slotPosY, m_transform.position.z + 0.001f);

        const auto pickingId =
            PickingId{m_runtimeId, static_cast<uint32_t>(buttonIndex + 1)};

        if (slotType == SlotType::outputsResize) {
            if (SceneWidgets::button(
                    pickingId, "All Low", buttonPos, {0.f, 0.f},
                    Core::Renderer::Colors::slate100, context)) {
                makeAllLow = true;
            }
            return;
        }

        const bool isHigh = slotComp->getSlotState(state).getLogicState() ==
                            SimEngine::LogicState::high;

        bool nextValue = isHigh;
        if (SceneWidgets::toggleButton(pickingId, &nextValue, buttonPos,
                                       buttonSize, context)) {
            setOutputSlotState(state, slotComp, nextValue);
        }

        std::string valueText = nextValue ? "1" : "0";
        constexpr float valueBoxWidth = 18.f;
        const glm::vec2 valueBoxSize = {valueBoxWidth, buttonSize.y};
        const glm::vec3 valueBoxPos = {
            buttonPos.x + (buttonSize.x / 2.f) + 5.f + (valueBoxWidth / 2.f),
            slotPosY,
            m_transform.position.z + 0.001f,
        };
        const auto textBoxId = PickingId{
            m_runtimeId,
            static_cast<uint32_t>(0x10000u + buttonIndex + 1),
        };

        const SceneWidgets::TextBoxOptions textBoxOptions{
            .maxLength = 1,
            .fontSize = Styles::simCompStyles.slotLabelSize,
            .padding = {5.f, 1.f},
            .backgroundColor = Core::Renderer::Colors::slate900,
            .hoverBackgroundColor = Core::Renderer::Colors::slate700,
            .focusedBackgroundColor = Core::Renderer::Colors::slate900,
            .borderColor = ViewportTheme::colors.componentBorder,
            .focusedBorderColor = ViewportTheme::colors.selectedComp,
            .textColor = ViewportTheme::colors.text,
            .placeholderColor = Core::Renderer::Colors::slate500,
            .cursorColor = ViewportTheme::colors.text,
        };

        const auto textResult =
            SceneWidgets::textBox(textBoxId, &valueText, valueBoxPos,
                                  valueBoxSize, context, textBoxOptions);
        bool parsedValue = nextValue;
        if (textResult.changed && parseBinaryValue(valueText, parsedValue) &&
            parsedValue != nextValue) {
            setOutputSlotState(state, slotComp, parsedValue);
        }
    }

    void InputSceneComponent::calculateSchematicScale(const SceneState &state) {
        SimulationSceneComponent::calculateSchematicScale(state);
        m_schematicTransform.scale.x = 50.f;
    }

} // namespace Bess::Canvas
