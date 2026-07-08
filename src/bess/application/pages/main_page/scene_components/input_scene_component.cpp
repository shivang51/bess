#include "input_scene_component.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/styles/sim_comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/settings/viewport_theme.h"
#include "bess_core/style/bess_theme.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"

namespace Bess::Canvas {
    namespace {
        bool makeAllLow = false;

        bool setOutputSlotState(const SceneState &state,
                                const SlotSceneComponent *slotComp,
                                bool isHigh) {
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
            simEngine.setOutputSlotState(slotParentComp->getSimEngineId(),
                                         slotComp->getIndex(),
                                         isHigh ? SimEngine::LogicState::high
                                                : SimEngine::LogicState::low);
            return true;
        }
    } // namespace

    InputSceneComponent::InputSceneComponent() {
        m_icon = Bess::UI::Icons::FontAwesomeIcons::FA_TOGGLE_OFF;
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

                if (!slotComp->isResizeSlot()) {
                    const auto slotParentComp =
                        state.getComponentByUuid<SimulationSceneComponent>(
                            slotComp->getParentComponent());
                    if (!slotParentComp) {
                        continue;
                    }

                    simEngine.setOutputSlotState(
                        slotParentComp->getSimEngineId(),
                        slotComp->getIndex(),
                        SimEngine::LogicState::low);
                }
            }
        }
    }

    void InputSceneComponent::prepareUI(SceneUIPrepareCtx &ctx) {
        SimulationSceneComponent::prepareUI(ctx);

        if (m_toggleButtons.size() > m_outputSlots.size() - 1) {
            size_t diff = m_toggleButtons.size() - (m_outputSlots.size() - 1);

            for (size_t i = 0; i < diff; i++) {
                auto btn = m_toggleButtons.back();
                ctx.sceneState->removeComponent(btn->getUuid());
                m_toggleButtons.pop_back();
            }

            m_setBtnCbs = true;
        } else if (m_toggleButtons.size() < m_outputSlots.size() - 1) {

            const auto n = m_outputSlots.size() - 1;
            while (m_toggleButtons.size() < n) {
                auto btn = std::make_shared<Bess::Canvas::UI::ToggleBtnComp>();
                m_toggleButtons.push_back(btn);

                auto &size = btn->getTrackSize();
                size.y = Styles::simCompStyles.slotLabelSize;

                auto &thumbSize = btn->getThumbSize();
                thumbSize.y = size.y;
                thumbSize.x = size.y;

                btn->setShowLabel(false);
                btn->getStyle().margin = Core::Style::Margin::fromVertical(
                    Canvas::Styles::simCompStyles.rowMargin);
                btn->getStyle().padding = 0;
                ctx.sceneState->addComponent(btn);
            }
        }

        auto prevParentNode = ctx.parentNode;
        ctx.parentNode = m_inpSlotsContainer->getUINode();

        for (const auto &btn : m_toggleButtons) {
            btn->prepareUI(ctx);
        }

        ctx.parentNode = prevParentNode;
        m_setBtnCbs = true;
        m_isUIDirty = false;
    }

    std::vector<UUID>
    InputSceneComponent::getDependants(const SceneState &state) const {
        auto deps = SimulationSceneComponent::getDependants(state);
        for (auto &btn : m_toggleButtons) {
            const auto &childDeps = btn->getDependants(state);
            deps.insert(deps.end(), childDeps.begin(), childDeps.end());
            deps.push_back(btn->getUuid());
        }
        return deps;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    InputSceneComponent::clone(const SceneState &sceneState) const {
        auto clonedComponent = std::make_shared<InputSceneComponent>(*this);
        clonedComponent->m_toggleButtons.clear();
        clonedComponent->m_isUIDirty = true;
        return cloneSimulationComponent(sceneState, clonedComponent);
    }

    void InputSceneComponent::draw(SceneDrawContext &context) {

        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        if (m_setBtnCbs) {
            BESS_ASSERT(m_toggleButtons.size() == m_outputSlots.size() - 1,
                        "Toggle buttons size does not match output slots size "
                        "| ToggleBtns Size = {}",
                        m_toggleButtons.size());
            for (size_t i = 0; i < m_toggleButtons.size(); i++) {
                auto btn = m_toggleButtons[i];
                const auto &slotUuid = m_outputSlots[i];

                const auto state = context.sceneState;
                btn->setCallback([this, state, slotUuid](bool toggled) {
                    const auto slotComp =
                        state->getComponentByUuid<SlotSceneComponent>(slotUuid);
                    BESS_ASSERT(slotComp,
                                "Slot component not found for UUID: {}",
                                (uint64_t)slotUuid);

                    if (!slotComp) {
                        return;
                    }

                    setOutputSlotState(*state, slotComp, toggled);
                });

                const auto slotComp =
                    state->getComponentByUuid<SlotSceneComponent>(slotUuid);
                auto isHigh = false;
                if (slotComp) {
                    isHigh = slotComp->getSlotState(*state).getLogicState() ==
                             SimEngine::LogicState::high;
                }
                btn->setToggled(isHigh);
            }

            m_setBtnCbs = false;
        }

        SimulationSceneComponent::draw(context);
    }

    void InputSceneComponent::drawToggleButton(SceneDrawContext &context,
                                               UUID slotUuid,
                                               int buttonIndex) {
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

        BESS_ASSERT(!slotComp->isInputSlot(),
                    "Unexpected input port for input component");

        const auto slotPosY =
            slotComp->getAbsolutePosition(state, context.isSchematicMode).y;

        const float buttonPosX =
            m_transform.position.x - (m_transform.scale.x / 2.f) +
            Styles::simCompStyles.paddingX + (buttonSize.x / 2.f);

        const glm::vec3 buttonPos =
            glm::vec3(buttonPosX, slotPosY, m_transform.position.z + 0.0001f);

        const auto pickingId =
            PickingId{m_runtimeId, static_cast<uint32_t>(buttonIndex + 1)};

        if (slotComp->isResizeSlot()) {
            if (SceneWidgets::button(
                    pickingId,
                    "All Low",
                    buttonPos,
                    context,
                    {
                        .textSize = Styles::simCompStyles.slotLabelSize - 2.f,
                        .buttonSize = buttonSize,
                    })) {
                makeAllLow = true;
            }
            return;
        }

        const bool isHigh = slotComp->getSlotState(state).getLogicState() ==
                            SimEngine::LogicState::high;

        bool nextValue = isHigh;
        if (SceneWidgets::toggleButton(
                pickingId, &nextValue, buttonPos, buttonSize, context)) {
            setOutputSlotState(state, slotComp, nextValue);
        }

        const std::string label = nextValue ? "1" : "0";
        const auto textSize = context.renderer->measureText(
            label, {.fontSize = Styles::simCompStyles.slotLabelSize});

        const float textPosX = buttonPos.x + (buttonSize.x / 2.f) + 8.f;
        const float textOffY = context.renderer->textCenterOffsetY(
            label, {.fontSize = Styles::simCompStyles.slotLabelSize});
        const glm::vec3 textPos = glm::vec3(
            textPosX, slotPosY + textOffY, m_transform.position.z + 0.0001f);

        SceneDraw::drawText(context,
                            label,
                            textPos,
                            Styles::simCompStyles.slotLabelSize,
                            ViewportTheme::colors.text,
                            PickingId{m_runtimeId, 0});
    }

    void InputSceneComponent::calculateSchematicScale(const SceneState &state) {
        SimulationSceneComponent::calculateSchematicScale(state);
        m_schematicTransform.scale.x = 50.f;
    }

} // namespace Bess::Canvas
