#include "input_scene_component.h"
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
#include <algorithm>
#include <stdexcept>
#include <string>

namespace Bess::Canvas {
    namespace {
        bool makeAllLow = false;

        std::vector<UUID>
        getRealOutputSlots(const SceneState &state,
                           const std::vector<UUID> &outputSlots) {
            std::vector<UUID> realOutputSlots;
            realOutputSlots.reserve(outputSlots.size());

            for (const auto &slotUuid : outputSlots) {
                const auto slotComp =
                    state.getComponentByUuid<SlotSceneComponent>(slotUuid);
                if (!slotComp || slotComp->isResizeSlot()) {
                    continue;
                }

                realOutputSlots.push_back(slotUuid);
            }

            return realOutputSlots;
        }

        bool setDigPortState(const SceneState &state,
                             const SlotSceneComponent *slotComp,
                             bool isHigh) {
            const auto slotParentComp =
                state.getComponentByUuid<SimulationSceneComponent>(
                    slotComp->getParentComponent());
            if (!slotParentComp) {
                return false;
            }

            auto *simEngine = state.runtime().sim;
            if (!simEngine) {
                return false;
            }

            simEngine->setOutputPortState(
                slotParentComp->getSimEngineId(),
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
                BESS_WARN("Parent SimulationSceneComponent not found for "
                          "SlotSceneComponent with UUID: {}",
                          (uint64_t)slotComp->getUuid());
                return false;
            }

            auto *simEngine = state.runtime().sim;
            if (!simEngine) {
                BESS_WARN("Simulation engine is unavailable");
                return false;
            }

            simEngine->setOutputPortState(
                slotParentComp->getSimEngineId(),
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

        for (auto &ctrl : m_inputCtrls) {
            if (ctrl) {
                ctrl->update(ts, state);
            }
        }

        if (makeAllLow) {
            makeAllLow = false;
            auto *simEngine = state.runtime().sim;
            BESS_ASSERT(simEngine, "Simulation engine is unavailable");

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

                    const auto signalKind = slotComp->getSignalKind();

                    if (signalKind == SimEngine::SignalKind::digital) {
                        simEngine->setOutputPortState(
                            slotParentComp->getSimEngineId(),
                            slotComp->getIndex(),
                            SimEngine::LogicState::low);
                    } else if (signalKind == SimEngine::SignalKind::scalar) {
                        simEngine->setOutputPortState(
                            slotParentComp->getSimEngineId(),
                            slotComp->getIndex(),
                            SimEngine::PortState::scalar(0.0));
                    } else {
                        BESS_WARN("Unsupported signal kind for reset: {}",
                                  (int)signalKind);
                    }
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
            textBox->getStyle().padding =
                Core::Style::Padding::fromSymmetric(4.f, 1.f);
            textBox->getStyle().fontSize =
                Styles::simCompStyles.slotLabelSize - 2.f;
            ctx.sceneState->addComponent(textBox);
            return textBox;
        }

        bool controlMatchesSignalKind(
            const std::shared_ptr<UI::UISceneComponent> &control,
            SimEngine::SignalKind signalKind) {
            if (!control) {
                return false;
            }

            switch (signalKind) {
            case SimEngine::SignalKind::digital:
                return std::dynamic_pointer_cast<UI::ToggleBtnComp>(control) !=
                       nullptr;
            case SimEngine::SignalKind::scalar:
                return std::dynamic_pointer_cast<UI::TextBoxComp>(control) !=
                       nullptr;
            default:
                return false;
            }
        }

        std::shared_ptr<UI::UISceneComponent>
        addInputControl(SceneUIPrepareCtx &ctx,
                        SimEngine::SignalKind signalKind) {
            switch (signalKind) {
            case SimEngine::SignalKind::digital:
                return addToggleBtn(ctx);
            case SimEngine::SignalKind::scalar:
                return addTextBox(ctx);
            default:
                BESS_WARN("Unsupported signal kind for input control: {}",
                          (int)signalKind);
                return nullptr;
            }
        }
    } // namespace

    void InputSceneComponent::prepareUI(SceneUIPrepareCtx &ctx) {
        SimulationSceneComponent::prepareUI(ctx);
        const auto realOutputSlots =
            getRealOutputSlots(*ctx.sceneState, m_outputSlots);
        m_inpSignalKinds.assign(realOutputSlots.size(),
                                SimEngine::SignalKind::none);

        if (m_inputCtrls.size() > realOutputSlots.size()) {
            size_t diff = m_inputCtrls.size() - realOutputSlots.size();

            for (size_t i = 0; i < diff; i++) {
                auto btn = m_inputCtrls.back();
                if (btn) {
                    ctx.sceneState->removeComponent(btn->getUuid());
                }
                m_inputCtrls.pop_back();
            }

            m_setBtnCbs = true;
        } else if (m_inputCtrls.size() < realOutputSlots.size()) {
            m_inputCtrls.resize(realOutputSlots.size(), nullptr);
            m_setBtnCbs = true;
        }

        for (size_t i = 0; i < realOutputSlots.size(); i++) {
            const auto &slotUuid = realOutputSlots[i];
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

            const auto signalKind = slotComp->getSignalKind();
            m_inpSignalKinds[i] = signalKind;

            if (controlMatchesSignalKind(m_inputCtrls[i], signalKind)) {
                continue;
            }

            if (m_inputCtrls[i]) {
                ctx.sceneState->removeComponent(m_inputCtrls[i]->getUuid());
            }

            m_inputCtrls[i] = addInputControl(ctx, signalKind);
        }

        BESS_ASSERT(m_inputCtrls.size() == realOutputSlots.size(),
                    "Input controls size does not match output slots size "
                    "| InputCtrls Size = {}, OutputSlots Size = {}",
                    m_inputCtrls.size(),
                    realOutputSlots.size());

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
            if (!btn) {
                continue;
            }

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

                setDigPortState(*state, slotComp, toggled);
            });

            const auto slotComp =
                state->getComponentByUuid<SlotSceneComponent>(slotUuid);
            auto isHigh = false;

            if (slotComp) {
                isHigh = slotComp->getSlotState(context).getLogicState() ==
                         SimEngine::LogicState::high;
            }
            btn->setToggled(isHigh);
        }

        void setTextBoxCb(const std::shared_ptr<UI::UISceneComponent> &comp,
                          SceneDrawContext &context,
                          const UUID &slotUuid) {

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
                const auto slotState = slotComp->getSlotState(context);
                if (slotState.isScalar()) {
                    textBox->setValue(std::to_string(slotState.scalarValue));
                }
            }
        }
    } // namespace

    void InputSceneComponent::draw(SceneDrawContext &context) {

        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        if (m_setBtnCbs) {
            const auto realOutputSlots =
                getRealOutputSlots(*context.sceneState, m_outputSlots);
            BESS_ASSERT(m_inputCtrls.size() == realOutputSlots.size(),
                        "Toggle buttons size does not match output slots size "
                        "| ToggleBtns Size = {}",
                        m_inputCtrls.size());
            const auto n =
                std::min(m_inputCtrls.size(), realOutputSlots.size());
            for (size_t i = 0; i < n; i++) {
                auto ctrl = m_inputCtrls[i];
                if (!ctrl) {
                    continue;
                }

                const auto &slotUuid = realOutputSlots[i];
                const auto sigType = i < m_inpSignalKinds.size()
                                         ? m_inpSignalKinds[i]
                                         : SimEngine::SignalKind::none;

                if (sigType == SimEngine::SignalKind::digital) {
                    setToggleCb(ctrl, context, slotUuid);
                } else if (sigType == SimEngine::SignalKind::scalar) {
                    setTextBoxCb(ctrl, context, slotUuid);
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
