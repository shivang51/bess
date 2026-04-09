#include "slot_scene_component.h"
#include "conn_joint_scene_component.h"
#include "connection_scene_component.h"
#include "expression_evalutator/expr_evaluator.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/services/connection_service.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/ui.h"

namespace Bess::Canvas {
    std::vector<std::shared_ptr<SceneComponent>> SlotSceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<SlotSceneComponent>(*this);
        prepareClone(*clonedComponent);
        clonedComponent->setConnectedConnections({});
        return {clonedComponent};
    }

    void SlotSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        UI::setCursorPointer();
    }

    void SlotSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UI::setCursorNormal();
    }

    void SlotSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press) {
            if (e.button == Events::MouseButton::left) {
                onMouseLeftClick(e);
            }
        }
    }

    void SlotSceneComponent::draw(SceneDrawContext &drawContext) {
        const auto &state = *drawContext.sceneState;
        const auto pos = getAbsolutePosition(state);
        const auto pickingId = PickingId{m_runtimeId, PickingId::InfoFlags::unSelectable};

        auto bg = ViewportTheme::colors.stateLow;
        auto border = bg;

        const bool isResizeSlot = m_slotType == SlotType::inputsResize ||
                                  m_slotType == SlotType::outputsResize;
        const bool isConnected = !isResizeSlot && isSlotConnected(state);
        float radiusGap = 1.f;

        if (isResizeSlot) {
            bg.a = 0.1f;
            radiusGap = 0.25f;
        } else {
            const auto &slotState = getSlotState(state);

            // state color
            switch (slotState.state) {
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
        drawContext.materialRenderer->drawCircle(pos, r, border, pickingId, ir);
        drawContext.materialRenderer->drawCircle(pos, ir - radiusGap, bg, pickingId);

        if (!m_name.empty()) {
            const float labeldx = Styles::simCompStyles.slotMargin +
                                  (Styles::simCompStyles.slotRadius * 2.f);
            float labelX = pos.x;
            if (m_slotType == SlotType::digitalInput) {
                labelX += labeldx;
            } else {
                const auto labelSize = Renderer::MaterialRenderer::getTextRenderSize(m_name,
                                                                                     Styles::simCompStyles.slotLabelSize);
                labelX -= labeldx + labelSize.x;
            }
            float dY = Styles::componentStyles.slotRadius -
                       (std::abs((Styles::componentStyles.slotRadius * 2.f) - Styles::componentStyles.slotLabelSize) / 2.f);

            const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
            drawContext.materialRenderer->drawText(m_name,
                                                   {labelX, pos.y + dY, pos.z},
                                                   Styles::componentStyles.slotLabelSize,
                                                   ViewportTheme::colors.text,
                                                   PickingId{parentComp->getRuntimeId(), 0},
                                                   parentComp->getTransform().angle);
        }
    }

    void SlotSceneComponent::drawSchematic(SceneDrawContext &drawContext) {

        if (isResizeSlot())
            return;

        const auto &state = *drawContext.sceneState;
        const auto &pos = getSchematicPosAbsolute(state);
        const auto pinId = PickingId{m_runtimeId, PickingId::InfoFlags::unSelectable};
        constexpr float nodeWeight = Styles::compSchematicStyles.strokeSize;
        const auto &pinColor = ViewportTheme::schematicViewColors.pin;
        glm::vec2 offset = {0.f, 0.f};

        // will begin pin from little behind than the position
        // so that line joins nicely with the components like OR gate or xor gate which have
        // curved edges
        auto startPos = pos;
        if (m_slotType == SlotType::digitalOutput) {
            offset.x = Styles::compSchematicStyles.pinSize;
        } else {
            offset.x = -Styles::compSchematicStyles.pinSize;
            startPos.x += 5.f;
        }

        drawContext.pathRenderer->beginPathMode(startPos, nodeWeight, pinColor, pinId);
        drawContext.pathRenderer->pathLineTo({pos.x + offset.x, pos.y + offset.y, pos.z},
                                             nodeWeight, pinColor, pinId);
        drawContext.pathRenderer->endPathMode(false,
                                              false, {}, true, false, m_invalidateCache);
        m_invalidateCache = true;

        if (!m_name.empty()) {
            const auto textSize = Renderer::MaterialRenderer::getTextRenderSize(m_name,
                                                                                Styles::componentStyles.slotLabelSize);

            float textOffsetX = 4.f;

            if (m_slotType == SlotType::digitalOutput)
                textOffsetX -= textSize.x + 6.f;

            const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
            // not using schematic slot pos for text as in schematic view,
            // slot is rendered behind the component but text should be in front of component
            // so using z of node view
            drawContext.materialRenderer->drawText(m_name,
                                                   {pos.x + textOffsetX,
                                                    pos.y + (textSize.y / 2.f) - 2.f,
                                                    SceneComponent::getAbsolutePosition(state).z}, // because we don't want schematic pos
                                                   Styles::componentStyles.slotLabelSize,
                                                   ViewportTheme::schematicViewColors.componentStroke,
                                                   PickingId{parentComp->getRuntimeId(), 0},
                                                   0.f);
        }
    }

    SimEngine::SlotState SlotSceneComponent::getSlotState(const SceneState &state) const {
        BESS_ASSERT(m_parentComponent != UUID::null, "Parent component UUID is null");
        BESS_ASSERT(m_index >= 0, "Slot index is negative");

        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        if (!parentComp || m_index < 0) {
            return {SimEngine::LogicState::unknown, SimEngine::SimTime(0)};
        }

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto digitalComp = simEngine.getDigitalComponent(parentComp->getSimEngineId());
        if (!digitalComp) {
            return {SimEngine::LogicState::unknown, SimEngine::SimTime(0)};
        }

        if (isInputSlot()) {
            const auto &compState = digitalComp->state;
            BESS_ASSERT(static_cast<size_t>(m_index) < compState.inputStates.size(),
                        "Slot index greater than input states size");
            if (static_cast<size_t>(m_index) >= compState.inputStates.size()) {
                return {SimEngine::LogicState::unknown, SimEngine::SimTime(0)};
            }
            return compState.inputStates[m_index];
        }

        const auto &compState = digitalComp->state;
        BESS_ASSERT(static_cast<size_t>(m_index) < compState.outputStates.size(),
                    "Slot index greater than output states size");
        if (static_cast<size_t>(m_index) >= compState.outputStates.size()) {
            return {SimEngine::LogicState::unknown, SimEngine::SimTime(0)};
        }

        return compState.outputStates[m_index];
    }

    bool SlotSceneComponent::isSlotConnected(const SceneState &state) const {
        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        if (!parentComp || m_index < 0) {
            return false;
        }

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto digitalComp = simEngine.getDigitalComponent(parentComp->getSimEngineId());
        if (!digitalComp) {
            return false;
        }
        const auto &compState = digitalComp->state;

        if (isInputSlot()) {
            BESS_ASSERT(static_cast<size_t>(m_index) < compState.inputStates.size(),
                        "Slot index greater than inputs in sim engine");
            if (static_cast<size_t>(m_index) >= compState.inputConnected.size()) {
                return false;
            }
            return compState.inputConnected[m_index];
        } else {
            BESS_ASSERT(static_cast<size_t>(m_index) < compState.outputStates.size(),
                        "Slot index greater than outputs in sim engine");
            if (static_cast<size_t>(m_index) >= compState.outputConnected.size()) {
                return false;
            }
            return compState.outputConnected[m_index];
        }
    }

    void SlotSceneComponent::addConnection(const UUID &connectionId) {
        m_connectedConnections.emplace_back(connectionId);
    }

    void SlotSceneComponent::removeConnection(const UUID &connectionId) {
        m_connectedConnections.erase(std::ranges::remove(m_connectedConnections,
                                                         connectionId)
                                         .begin(),
                                     m_connectedConnections.end());
    }

    glm::vec3 SlotSceneComponent::getConnectionPos(const SceneState &state) const {
        auto pos = getAbsolutePosition(state);

        if (state.getIsSchematicView()) {
            const float offsetX = (m_slotType == SlotType::digitalInput)
                                      ? -Styles::compSchematicStyles.pinSize
                                      : Styles::compSchematicStyles.pinSize;
            pos.x += offsetX;
        }

        return pos;
    }

    bool SlotSceneComponent::isResizeSlot() const {
        return m_slotType == SlotType::inputsResize ||
               m_slotType == SlotType::outputsResize;
    }
    bool SlotSceneComponent::isInputSlot() const {
        return m_slotType == SlotType::digitalInput || m_slotType == SlotType::inputsResize;
    }

    glm::vec3 SlotSceneComponent::getAbsolutePosition(const SceneState &state) const {
        if (state.getIsSchematicView()) {
            return getSchematicPosAbsolute(state);
        }

        return SceneComponent::getAbsolutePosition(state);
    }

    glm::vec3 SlotSceneComponent::getSchematicPosAbsolute(const SceneState &state) const {
        return state.getComponentByUuid(m_parentComponent)->getAbsolutePosition(state) +
               m_schematicPos;
    }

    void SlotSceneComponent::onMouseLeftClick(const Events::MouseButtonEvent &e) {
        const auto &connStartSlot = e.sceneState->getConnectionStartSlot();
        if (connStartSlot == UUID::null) {
            e.sceneState->setConnectionStartSlot(m_uuid);
            return;
        }

        auto startComp = e.sceneState->getComponentByUuid(connStartSlot);
        std::shared_ptr<ConnJointSceneComp> jointComp = nullptr;
        std::shared_ptr<SlotSceneComponent> startSlot = nullptr;
        if (startComp->getType() == SceneComponentType::connJoint) {
            jointComp = startComp->cast<ConnJointSceneComp>();
            startSlot = e.sceneState->getComponentByUuid<SlotSceneComponent>(jointComp->getOutputSlotId());
        } else {
            startSlot = e.sceneState->getComponentByUuid<SlotSceneComponent>(connStartSlot);
        }
        auto endSlot = e.sceneState->getComponentByUuid<SlotSceneComponent>(m_uuid);

        const auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
        const auto [canConnect, reason] = Svc::SvcConnection::instance().canConnect(connStartSlot,
                                                                                    m_uuid,
                                                                                    sceneDriver.getSceneWithId(e.sceneState->getSceneId()).get());

        if (!canConnect) {
            BESS_WARN("Cannot create connection between component {} and component {}: {}",
                      (uint64_t)connStartSlot, (uint64_t)m_uuid, reason);
            e.sceneState->setConnectionStartSlot(UUID::null);
            return;
        }

        UUID starSlotUuid = jointComp ? jointComp->getUuid() : startSlot->getUuid();
        auto conn = Svc::SvcConnection::instance().createConnection(starSlotUuid,
                                                                    m_uuid,
                                                                    sceneDriver.getSceneWithId(e.sceneState->getSceneId()).get());

        auto &cmdManager = Pages::MainPage::getInstance()->getState().getCommandSystem();
        cmdManager.push(std::make_unique<Cmd::AddCompCmd<ConnectionSceneComponent>>(conn));

        BESS_INFO("[Scene] Created connection {} between slots {} and {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)starSlotUuid,
                  (uint64_t)m_uuid);

        e.sceneState->setConnectionStartSlot(UUID::null);
    }

    void SlotSceneComponent::onRuntimeIdChanged() {
        m_invalidateCache = true;
    }

    std::vector<UUID> SlotSceneComponent::getDependants(const SceneState &state) const {
        auto dependants = SceneComponent::getDependants(state);
        if (isResizeSlot()) {
            return dependants;
        }

        for (const auto &connUuid : m_connectedConnections) {
            const auto &connComp = state.getComponentByUuid<ConnectionSceneComponent>(connUuid);
            const auto &connDeps = connComp->getDependants(state);
            dependants.insert(dependants.end(), connDeps.begin(), connDeps.end());
            dependants.push_back(connUuid);
        }

        const auto &simComp = state.getComponentByUuid<SimulationSceneComponent>(
            m_parentComponent);

        const bool isUnirary = SimEngine::ExprEval::isUninaryOperator(
            simComp->getCompDef()->getOpInfo().op);

        if (!isUnirary) {
            return dependants;
        }

        if (isInputSlot()) {
            dependants.push_back(simComp->getOutputSlots()[m_index]);
        } else {
            dependants.push_back(simComp->getInputSlots()[m_index]);
        }

        return dependants;
    }
} // namespace Bess::Canvas
