#include "input_scene_component.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/styles/sim_comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/settings/viewport_theme.h"
#include "bess_core/style/bess_theme.h"
#include "common/types.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"

namespace Bess::Canvas {
    namespace {
        bool makeAllLow = false;

        bool setOutputPortState(const SceneState &state,
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
            simEngine.setOutputPortState(slotParentComp->getSimEngineId(),
                                         slotComp->getIndex(),
                                         isHigh ? SimEngine::LogicState::high
                                                : SimEngine::LogicState::low);
            return true;
        }

        bool setScalarPortState(const SceneState &state,
                                const SlotSceneComponent *slotComp,
                                double value) {
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
            simEngine.setOutputPortState(slotParentComp->getSimEngineId(),
                                         slotComp->getIndex(),
                                         SimEngine::PortState::scalar(value));
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

                    simEngine.setOutputPortState(
                        slotParentComp->getSimEngineId(),
                        slotComp->getIndex(),
                        SimEngine::LogicState::low);
                }
            }
        }
    }

    namespace {
        std::shared_ptr<UI::UISceneComponent>
        addToggleBtn(SceneUIPrepareCtx &ctx) {
            auto btn = std::make_shared<Bess::Canvas::UI::ToggleBtnComp>();
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
            return btn;
        }

        std::shared_ptr<UI::UISceneComponent>
        addTextBox(SceneUIPrepareCtx &ctx) {
            auto textBox = std::make_shared<Bess::Canvas::UI::TextBoxComp>();
            textBox->getStyle().margin = Core::Style::Margin::fromVertical(
                Canvas::Styles::simCompStyles.rowMargin);
            textBox->getStyle().padding = 0;
            ctx.sceneState->addComponent(textBox);
            return textBox;
        }
    } // namespace

    void InputSceneComponent::prepareUI(SceneUIPrepareCtx &ctx) {
        SimulationSceneComponent::prepareUI(ctx);
        m_inpSignalKinds.resize(m_outputSlots.size() - 1,
                                SimEngine::SignalKind::none);

        if (m_inputCtrls.size() > m_outputSlots.size() - 1) {
            size_t diff = m_inputCtrls.size() - (m_outputSlots.size() - 1);

            for (size_t i = 0; i < diff; i++) {
                auto btn = m_inputCtrls.back();
                if (btn) {
                    ctx.sceneState->removeComponent(btn->getUuid());
                }
                m_inputCtrls.pop_back();
            }

            m_setBtnCbs = true;
        } else if (m_inputCtrls.size() < m_outputSlots.size() - 1) {

            auto n = m_outputSlots.size() - 1;
            const auto start = m_inputCtrls.size();
            m_inputCtrls.resize(n, nullptr);

            for (size_t i = start; i < n; i++) {
                const auto &slotUuid = m_outputSlots[m_inputCtrls.size()];
                const auto &slotComp =
                    ctx.sceneState->getComponentByUuid<SlotSceneComponent>(
                        slotUuid);
                BESS_ASSERT(slotComp,
                            "SlotSceneComponent not found for slotUuid: {}",
                            (uint64_t)slotUuid);

                if (!slotComp) {
                    BESS_WARN("SlotSceneComponent not found for slotUuid: {}",
                              (uint64_t)slotUuid);
                    continue;
                }

                const auto type = slotComp ? slotComp->getSignalKind()
                                           : SimEngine::SignalKind::none;

                m_inpSignalKinds[i] = type;

                if (type == SimEngine::SignalKind::digital) {
                    m_inputCtrls[i] = addToggleBtn(ctx);
                } else if (type == SimEngine::SignalKind::scalar) {
                    m_inputCtrls[i] = addTextBox(ctx);
                    BESS_TRACE(
                        "Added TextBox for scalar input control at index {}",
                        i);
                } else {
                    BESS_WARN("Unsupported signal kind for input control: {}",
                              (int)type);
                }
            }
        }

        BESS_ASSERT(m_inputCtrls.size() == m_outputSlots.size() - 1,
                    "Input controls size does not match output slots size "
                    "| InputCtrls Size = {}, OutputSlots Size = {}",
                    m_inputCtrls.size(),
                    m_outputSlots.size() - 1);

        auto prevParentNode = ctx.parentNode;
        ctx.parentNode = m_inpSlotsContainer->getUINode();

        for (const auto &btn : m_inputCtrls) {
            if (btn)
                btn->prepareUI(ctx);
        }

        ctx.parentNode = prevParentNode;
        m_setBtnCbs = true;
        m_isUIDirty = false;
    }

    std::vector<UUID>
    InputSceneComponent::getDependants(const SceneState &state) const {
        auto deps = SimulationSceneComponent::getDependants(state);
        for (auto &btn : m_inputCtrls) {
            const auto &childDeps = btn->getDependants(state);
            deps.insert(deps.end(), childDeps.begin(), childDeps.end());
            deps.push_back(btn->getUuid());
        }
        return deps;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    InputSceneComponent::clone(const SceneState &sceneState) const {
        auto clonedComponent = std::make_shared<InputSceneComponent>(*this);
        clonedComponent->m_inputCtrls.clear();
        clonedComponent->m_isUIDirty = true;
        return cloneSimulationComponent(sceneState, clonedComponent);
    }

    namespace {

        void setToggleCb(const std::shared_ptr<UI::UISceneComponent> &comp,
                         SceneDrawContext &context,
                         const UUID &slotUuid) {

            auto btn = std::dynamic_pointer_cast<UI::ToggleBtnComp>(comp);
            const auto state = context.sceneState;
            BESS_ASSERT(btn, "Component is not a ToggleBtnComp");

            btn->setCallback([state, slotUuid](bool toggled) {
                const auto slotComp =
                    state->getComponentByUuid<SlotSceneComponent>(slotUuid);
                BESS_ASSERT(slotComp,
                            "Slot component not found for UUID: {}",
                            (uint64_t)slotUuid);

                if (!slotComp) {
                    return;
                }

                setOutputPortState(*state, slotComp, toggled);
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

        void setTextBoxCb(const std::shared_ptr<UI::UISceneComponent> &comp,
                          SceneDrawContext &context,
                          const UUID &slotUuid,
                          SimEngine::SignalKind sigKind) {

            auto textBox = std::dynamic_pointer_cast<UI::TextBoxComp>(comp);
            const auto state = context.sceneState;
            BESS_ASSERT(textBox, "Component is not a TextBoxComp");

            textBox->setSubmittedCallback(
                [state, slotUuid](const std::string &text) {
                    const auto slotComp =
                        state->getComponentByUuid<SlotSceneComponent>(slotUuid);
                    BESS_ASSERT(slotComp,
                                "Slot component not found for UUID: {}",
                                (uint64_t)slotUuid);

                    if (!slotComp) {
                        return;
                    }

                    try {
                        const auto value = std::stod(text);
                        setScalarPortState(*state, slotComp, value);
                    } catch (const std::invalid_argument &) {
                        BESS_WARN("Invalid input for scalar value: {}", text);
                    }
                });

            const auto slotComp =
                state->getComponentByUuid<SlotSceneComponent>(slotUuid);
            if (slotComp) {
                // Assuming you have a function to get the current scalar value
                // float currentValue = getOutputPortScalarValue(*state,
                // slotComp); textBox->setText(std::to_string(currentValue));
            }
        }
    } // namespace

    void InputSceneComponent::draw(SceneDrawContext &context) {

        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        if (m_setBtnCbs) {
            BESS_ASSERT(m_inputCtrls.size() == m_outputSlots.size() - 1,
                        "Toggle buttons size does not match output slots size "
                        "| ToggleBtns Size = {}",
                        m_inputCtrls.size());
            for (size_t i = 0; i < m_inputCtrls.size(); i++) {
                auto ctrl = m_inputCtrls[i];
                const auto &slotUuid = m_outputSlots[i];

                const auto &sigType = m_inpSignalKinds[i];

                if (sigType == SimEngine::SignalKind::digital) {
                    setToggleCb(ctrl, context, slotUuid);
                } else {
                }
            }

            m_setBtnCbs = false;
        }

        SimulationSceneComponent::draw(context);
    }

    void InputSceneComponent::calculateSchematicScale(const SceneState &state) {
        SimulationSceneComponent::calculateSchematicScale(state);
        m_schematicTransform.scale.x = 50.f;
    }

} // namespace Bess::Canvas
