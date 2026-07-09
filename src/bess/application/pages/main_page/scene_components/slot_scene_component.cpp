#include "slot_scene_component.h"
#include "bess_core/commands/add_component_command.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/styles/sim_comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/settings/viewport_theme.h"
#include "bess_core/style/bess_theme.h"
#include "conn_joint_scene_component.h"
#include "connection_scene_component.h"
#include "dig_sim_driver.h"
#include "expression_evalutator/expr_evaluator.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/services/connection_service.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

namespace Bess::Canvas {
    namespace {
        constexpr glm::vec2 kScalarSlotTextBoxSize{44.f, 0.f};

        std::string formatScalarSlotValue(double value) {
            return std::format("{:.6g}", value);
        }

        bool parseScalarSlotValue(const std::string &text, double &value) {
            try {
                size_t parsed = 0;
                const double next = std::stod(text, &parsed);
                while (parsed < text.size() &&
                       std::isspace(static_cast<unsigned char>(text[parsed]))) {
                    ++parsed;
                }
                if (parsed != text.size() || !std::isfinite(next)) {
                    return false;
                }

                value = next;
                return true;
            } catch (const std::invalid_argument &) {
                return false;
            } catch (const std::out_of_range &) {
                return false;
            }
        }

        bool setScalarSlotPortState(const SceneState &state,
                                    const SlotSceneComponent &slot,
                                    double value) {
            const auto port = slot.getPortRef(state);
            if (!port.isValid() ||
                port.signalKind != SimEngine::SignalKind::scalar) {
                return false;
            }

            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            if (!projectCtx) {
                return false;
            }

            auto &simEngine = projectCtx->getSimEngine();
            if (!port.isInput()) {
                return false;
            }

            simEngine.setInputPortState(
                port.componentId,
                port.index,
                SimEngine::PortState::scalar(value));
            return true;
        }
    } // namespace

    std::vector<std::shared_ptr<SceneComponent>>
    SlotSceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<SlotSceneComponent>(*this);
        prepareClone(*clonedComponent);
        clonedComponent->setConnectedConnections({});
        return {clonedComponent};
    }

    void SlotSceneComponent::resetCloneRuntimeState() {
        SceneComponent::resetCloneRuntimeState();

        m_label = nullptr;
        m_slotNode = nullptr;
        m_container = nullptr;
        m_scalarValueTextBox = nullptr;

        m_isHovered = false;
        m_invalidateCache = true;
    }

    bool SlotSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        auto &appCtx = GAppContext::getInstance();
        auto window = appCtx.getSubSystem<Window>();
        window->getui().setCursorPointer();
        m_isHovered = true;
        return true;
    }

    bool SlotSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        auto &appCtx = GAppContext::getInstance();
        auto window = appCtx.getSubSystem<Window>();
        window->getui().setCursorNormal();
        m_isHovered = false;
        return true;
    }

    bool SlotSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press) {
            if (e.button == Events::MouseButton::left) {
                onMouseLeftClick(e);
                return true;
            }
        }
        return false;
    }

    void SlotSceneComponent::prepareUI(SceneUIPrepareCtx &ctx) {
        auto uiNodeReg = ctx.sceneState->getUINodeRegistry();
        if (m_container == nullptr) {
            m_slotNode = uiNodeReg->addNode(UUID());

            m_container = UI::ContainerComp::create();
            auto &style = m_container->getStyle();
            style.padding = Core::Style::Padding::zero();
            style.margin = Core::Style::Margin::fromVertical(
                Canvas::Styles::simCompStyles.rowMargin);

            ctx.sceneState->addComponent(m_container);

            m_label = UI::LabelComp::create(m_name);
            auto &labelStyle = m_label->getStyle();
            labelStyle.fontSize = Styles::simCompStyles.slotLabelSize;
            labelStyle.padding = Core::Style::Padding::zero();
            labelStyle.margin = Core::Style::Margin::zero();
            ctx.sceneState->addComponent(m_label);

            ctx.sceneState->attachChild(m_container->getUuid(),
                                        m_label->getUuid());

            if (isInputSlot()) {
                m_slotNode->setMargin(Core::Style::Margin::onlyRight(4.f));
                m_container->setDirection(
                    Canvas::UI::LayoutDirection::horizontalReverse);
            } else {
                m_slotNode->setMargin(Core::Style::Margin::onlyLeft(4.f));
                m_container->setDirection(
                    Canvas::UI::LayoutDirection::horizontal);
            }

            const float slotSize = Styles::simCompStyles.slotRadius * 2.f;
            m_slotNode->setWidth(slotSize);
            m_slotNode->setHeight(slotSize);
        }

        const bool showScalarValueTextBox =
            !isResizeSlot() &&
            isInputSlot() &&
            m_signalKind == SimEngine::SignalKind::scalar &&
            !isSlotConnected(*ctx.sceneState);

        if (showScalarValueTextBox && m_scalarValueTextBox == nullptr) {
            m_scalarValueTextBox = std::make_shared<UI::TextBoxComp>();
            m_scalarValueTextBox->setPlaceholder("0");
            m_scalarValueTextBox->setTextBoxSize(kScalarSlotTextBoxSize);
            m_scalarValueTextBox->setMaxLength(32);
            auto &textBoxStyle = m_scalarValueTextBox->getStyle();
            textBoxStyle.margin =
                Core::Style::Margin::fromSymmetric(3.f, 0.f);
            textBoxStyle.padding =
                Core::Style::Padding::fromSymmetric(3.f, 1.f);
            textBoxStyle.fontSize =
                std::max(6.f, Styles::simCompStyles.slotLabelSize - 2.f);

            ctx.sceneState->addComponent(m_scalarValueTextBox);
            ctx.sceneState->attachChild(m_container->getUuid(),
                                        m_scalarValueTextBox->getUuid());
        } else if (!showScalarValueTextBox && m_scalarValueTextBox != nullptr) {
            if (ctx.sceneState->isComponentValid(
                    m_scalarValueTextBox->getUuid())) {
                ctx.sceneState->removeComponent(
                    m_scalarValueTextBox->getUuid(), UUID::master);
            }
            m_scalarValueTextBox = nullptr;
        }

        if (m_scalarValueTextBox) {
            if (!m_scalarValueTextBox->getFocused()) {
                const auto slotState = getSlotState(*ctx.sceneState);
                if (slotState.isScalar()) {
                    const auto valueText =
                        formatScalarSlotValue(slotState.scalarValue);
                    if (m_scalarValueTextBox->getValue() != valueText) {
                        m_scalarValueTextBox->setValue(valueText);
                    }
                }
            }

            const auto slotUuid = m_uuid;
            m_scalarValueTextBox->setChangedCallback(
                [sceneState = ctx.sceneState, slotUuid](
                    const std::string &text) {
                    if (sceneState == nullptr) {
                        return;
                    }

                    const auto slot =
                        sceneState->getComponentByUuid<SlotSceneComponent>(
                            slotUuid);
                    if (!slot) {
                        return;
                    }

                    double value = 0.0;
                    if (parseScalarSlotValue(text, value)) {
                        setScalarSlotPortState(*sceneState, *slot, value);
                    }
                });
        }

        m_container->prepareUI(ctx);

        auto uiNode = m_container->getUINode();
        uiNode->addChild(m_slotNode);

        m_isUIDirty = false;
    }

    void SlotSceneComponent::update(TimeMs frameTime, SceneState &state) {
        BESS_ASSERT(m_parentComponent != UUID::null,
                    "SlotSceneComponent must have a parent component");
    }

    void SlotSceneComponent::draw(SceneDrawContext &drawContext) {
        if (!m_container)
            return;

        const auto &state = *drawContext.sceneState;
        const auto pos =
            getAbsolutePosition(state, drawContext.isSchematicMode);
        const auto pickingId =
            PickingId{m_runtimeId, PickingId::InfoFlags::unSelectable};

        auto bg = ViewportTheme::colors.stateLow;
        auto border = bg;

        const bool isResizeTrigger = isResizeSlot();
        const bool isConnected = !isResizeTrigger && isSlotConnected(state);
        float radiusGap = 1.f;

        if (isResizeTrigger) {
            bg.a = 0.1f;
            radiusGap = 0.25f;
        } else {
            const auto &slotState = getSlotState(state);

            // state color
            switch (slotState.getLogicState()) {
            case SimEngine::LogicState::low:
                bg = ViewportTheme::colors.stateLow;
                break;
            case SimEngine::LogicState::high:
                bg = ViewportTheme::colors.stateHigh;
                break;
            case SimEngine::LogicState::unknown:
                bg = ViewportTheme::colors.stateUnknow;
                break;
            case SimEngine::LogicState::high_z:
                bg = ViewportTheme::colors.stateHighZ;
                break;
            }

            border = bg;

            if (!isConnected) {
                bg.a = 0.1f;
                radiusGap = 0.25f;
            }
        }

        const float ir = Styles::simCompStyles.slotRadius -
                         Styles::simCompStyles.slotBorderSize;
        const float r = Styles::simCompStyles.slotRadius;

        SceneDraw::drawCircle(
            drawContext, m_slotNode->getDrawPos(), r, border, pickingId, ir);
        SceneDraw::drawCircle(drawContext,
                              m_slotNode->getDrawPos(),
                              ir - radiusGap,
                              bg,
                              pickingId);
    }

    void SlotSceneComponent::drawSchematic(SceneDrawContext &drawContext) {

        if (isResizeSlot())
            return;

        const auto &state = *drawContext.sceneState;
        const auto &pos =
            getSchematicPosAbsolute(state, drawContext.isSchematicMode);
        const auto pinId =
            PickingId{m_runtimeId, PickingId::InfoFlags::unSelectable};
        constexpr float nodeWeight = Styles::compSchematicStyles.strokeSize;
        const auto &pinColor = ViewportTheme::schematicViewColors.pin;
        glm::vec2 offset = {0.f, 0.f};

        const bool isOnRight = m_schematicPos.x > 0.f;

        // will begin pin from little behind than the position
        // so that line joins nicely with the components like OR gate or xor
        // gate which have curved edges
        auto startPos = pos;
        if (isOnRight) {
            offset.x = Styles::compSchematicStyles.pinSize;
        } else {
            offset.x = -Styles::compSchematicStyles.pinSize;
            startPos.x += 5.f;
        }

        SceneDraw::drawLine(drawContext,
                            startPos,
                            {pos.x + offset.x, pos.y + offset.y, pos.z},
                            m_isHovered ? nodeWeight + 2.f : nodeWeight,
                            pinColor,
                            pinId);

        if (!m_name.empty()) {
            Core::Renderer::FontProps labelProps;
            labelProps.fontSize = Styles::simCompStyles.slotLabelSize;
            const auto textSize =
                drawContext.renderer
                    ? drawContext.renderer->measureText(m_name, labelProps)
                    : glm::vec2(0.f);

            float textOffsetX = 4.f;

            if (isOnRight)
                textOffsetX -= textSize.x + 6.f;

            const auto parentComp =
                state.getComponentByUuid<SimulationSceneComponent>(
                    m_parentComponent);
            // not using schematic slot pos for text as in schematic view,
            // slot is rendered behind the component but text should be in
            // front of component so using z of node view
            SceneDraw::drawText(
                drawContext,
                m_name,
                {pos.x + textOffsetX,
                 pos.y + (textSize.y / 2.f) - 2.f,
                 SceneComponent::getAbsolutePosition(
                     state, drawContext.isSchematicMode)
                     .z}, // because we don't want schematic pos
                static_cast<std::size_t>(labelProps.fontSize),
                ViewportTheme::schematicViewColors.componentStroke,
                PickingId{parentComp->getRuntimeId(), 0},
                0.f);
        }
    }

    SimEngine::PortState
    SlotSceneComponent::getSlotState(const SceneState &state) const {
        BESS_ASSERT(m_parentComponent != UUID::null,
                    "Parent component UUID is null, {}",
                    (uint64_t)m_uuid);

        const auto port = getPortRef(state);
        if (!port.isValid()) {
            return {SimEngine::LogicState::unknown, SimEngine::SimTime(0)};
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        return simEngine.getPortState(port);
    }

    bool SlotSceneComponent::isSlotConnected(const SceneState &state) const {
        const auto port = getPortRef(state);
        if (!port.isValid()) {
            return false;
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        const auto stateSnapshot =
            simEngine.getComponentState(port.componentId);

        if (port.isInput()) {
            if (static_cast<size_t>(port.index) >=
                stateSnapshot.inputConnected.size()) {
                return false;
            }
            return stateSnapshot.inputConnected[port.index];
        }

        if (static_cast<size_t>(port.index) >=
            stateSnapshot.outputConnected.size()) {
            return false;
        }
        return stateSnapshot.outputConnected[port.index];
    }

    void SlotSceneComponent::addConnection(const UUID &connectionId) {
        m_connectedConnections.emplace_back(connectionId);
        setSlotLayoutDirty();
    }

    void SlotSceneComponent::removeConnection(const UUID &connectionId) {
        m_connectedConnections.erase(
            std::ranges::remove(m_connectedConnections, connectionId).begin(),
            m_connectedConnections.end());
        setSlotLayoutDirty();
    }

    glm::vec3 SlotSceneComponent::getConnectionPos(const SceneState &state,
                                                   bool isSchematicMode) const {
        auto pos = getAbsolutePosition(state, isSchematicMode);

        if (isSchematicMode) {
            const float offsetX = isInputSlot()
                                      ? -Styles::compSchematicStyles.pinSize
                                      : Styles::compSchematicStyles.pinSize;
            pos.x += offsetX;
        }

        return pos;
    }

    SimEngine::PortRef
    SlotSceneComponent::getPortRef(const SceneState &state) const {
        const auto parentComp =
            state.getComponentByUuid<SimulationSceneComponent>(
                m_parentComponent);
        if (!parentComp) {
            return {};
        }

        return {.componentId = parentComp->getSimEngineId(),
                .direction = m_portDirection,
                .signalKind = m_signalKind,
                .index = m_index};
    }

    bool SlotSceneComponent::isResizeSlot() const {
        return m_resizeTrigger;
    }
    bool SlotSceneComponent::isInputSlot() const {
        return m_portDirection == SimEngine::PortDirection::input;
    }

    glm::vec3
    SlotSceneComponent::getAbsolutePosition(const SceneState &state,
                                            bool isSchematicMode) const {
        if (isSchematicMode) {
            return getSchematicPosAbsolute(state, isSchematicMode);
        }

        return m_slotNode ? m_slotNode->getDrawPos()
                          : SceneComponent::getAbsolutePosition(
                                state, isSchematicMode);
    }

    glm::vec3
    SlotSceneComponent::getSchematicPosAbsolute(const SceneState &state,
                                                bool isSchematicMode) const {
        return state.getComponentByUuid(m_parentComponent)
                   ->getAbsolutePosition(state, isSchematicMode) +
               m_schematicPos;
    }

    bool
    SlotSceneComponent::onMouseLeftClick(const Events::MouseButtonEvent &e) {
        const auto &connStartSlot = e.sceneState->getConnectionStartSlot();
        if (connStartSlot == UUID::null) {
            e.sceneState->setConnectionStartSlot(m_uuid);
            return true;
        }

        SceneComponent *startComp =
            e.sceneState->getComponentByUuid(connStartSlot);
        ConnJointSceneComp *jointComp = nullptr;
        SlotSceneComponent *startSlot = nullptr;
        if (startComp->getType() == SceneComponentType::connJoint) {
            jointComp = dynamic_cast<ConnJointSceneComp *>(startComp);
            startSlot = e.sceneState->getComponentByUuid<SlotSceneComponent>(
                jointComp->getOutputSlotId());
        } else {
            startSlot = e.sceneState->getComponentByUuid<SlotSceneComponent>(
                connStartSlot);
        }
        auto endSlot =
            e.sceneState->getComponentByUuid<SlotSceneComponent>(m_uuid);

        auto projCtx =
            GAppContext::getInstance().getSubSystem<Bess::ProjectContext>();
        auto sceneDriver = projCtx->getSubSystem<SceneDriver>();
        auto connectionsSvc = projCtx->getSubSystem<Svc::SvcConnection>();
        const auto [canConnect, reason] = connectionsSvc->canConnect(
            connStartSlot,
            m_uuid,
            sceneDriver->getSceneWithId(e.sceneState->getSceneId()));

        if (!canConnect) {
            BESS_WARN("Cannot create connection between component {} and "
                      "component {}: {}",
                      (uint64_t)connStartSlot,
                      (uint64_t)m_uuid,
                      reason);
            e.sceneState->setConnectionStartSlot(UUID::null);
            return true;
        }

        UUID starSlotUuid =
            jointComp ? jointComp->getUuid() : startSlot->getUuid();
        auto conn = connectionsSvc->createConnection(
            starSlotUuid,
            m_uuid,
            sceneDriver->getSceneWithId(e.sceneState->getSceneId()));

        if (!conn) {
            BESS_ERROR("Failed to create connection between component {} and "
                       "component {}",
                       (uint64_t)connStartSlot,
                       (uint64_t)m_uuid);
            e.sceneState->setConnectionStartSlot(UUID::null);
            return false;
        }

        auto &cmdManager =
            Pages::MainPage::getInstance()->getState().getCommandSystem();
        cmdManager.push(
            std::make_unique<Cmd::AddCompCmd<ConnectionSceneComponent>>(conn));

        BESS_INFO("[Scene] Created connection {} between slots {} and {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)starSlotUuid,
                  (uint64_t)m_uuid);

        e.sceneState->setConnectionStartSlot(UUID::null);
        return true;
    }

    void SlotSceneComponent::onRuntimeIdChanged() {
        m_invalidateCache = true;
    }

    void SlotSceneComponent::setSlotLayoutDirty() {
        m_isUIDirty = true;
        if (m_container && m_container->getUINode()) {
            m_container->getUINode()->setSizeDirty();
        }
    }

    std::vector<UUID>
    SlotSceneComponent::getDependants(const SceneState &state) const {
        auto dependants = SceneComponent::getDependants(state);
        if (isResizeSlot()) {
            return dependants;
        }

        for (const auto &connUuid : m_connectedConnections) {
            const auto &connComp =
                state.getComponentByUuid<ConnectionSceneComponent>(connUuid);
            if (!connComp) {
                continue;
            }

            const auto &connDeps = connComp->getDependants(state);
            dependants.insert(
                dependants.end(), connDeps.begin(), connDeps.end());
            dependants.push_back(connUuid);
        }

        const auto &simComp =
            state.getComponentByUuid<SimulationSceneComponent>(
                m_parentComponent);
        if (!simComp) {
            return dependants;
        }

        auto def =
            std::dynamic_pointer_cast<SimEngine::Drivers::Digital::DigCompDef>(
                simComp->getCompDef());
        if (!def) {
            return dependants;
        }

        const bool isUnirary =
            SimEngine::ExprEval::isUninaryOperator(def->getOpInfo().op);

        if (isUnirary) {
            if (isInputSlot()) {
                dependants.push_back(simComp->getOutputSlots()[m_index]);
            } else {
                dependants.push_back(simComp->getInputSlots()[m_index]);
            }
        }

        return dependants;
    }

    std::vector<UUID> SlotSceneComponent::cleanup(SceneState &state,
                                                  UUID caller) {
        auto removedIds = SceneComponent::cleanup(state, caller);

        if (m_container && state.isComponentValid(m_container->getUuid())) {
            const auto ids =
                state.removeComponent(m_container->getUuid(), UUID::master);
            removedIds.insert(removedIds.end(), ids.begin(), ids.end());
        }

        m_container = nullptr;
        m_label = nullptr;
        m_slotNode = nullptr;
        m_isUIDirty = true;
        return removedIds;
    }

    void SlotSceneComponent::onNameChanged() {
        if (m_label) {
            m_label->setName(m_name);
        }
    }
} // namespace Bess::Canvas
