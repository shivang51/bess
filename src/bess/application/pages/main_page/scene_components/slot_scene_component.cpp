#include "slot_scene_component.h"
#include "conn_joint_scene_component.h"
#include "connection_scene_component.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/services/connection_service.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/ui.h"

namespace Bess::Canvas {
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

    void SlotSceneComponent::draw(SceneState &state,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
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
        materialRenderer->drawCircle(pos, r, border, pickingId, ir);
        materialRenderer->drawCircle(pos, ir - radiusGap, bg, pickingId);

        if (!m_name.empty()) {
            const float labeldx = Styles::simCompStyles.slotMargin +
                                  (Styles::simCompStyles.slotRadius * 2.f);
            float labelX = pos.x;
            if (m_slotType == SlotType::digitalInput) {
                labelX += labeldx;
            } else {
                const auto labelSize = materialRenderer->getTextRenderSize(m_name,
                                                                           Styles::simCompStyles.slotLabelSize);
                labelX -= labeldx + labelSize.x;
            }
            float dY = Styles::componentStyles.slotRadius -
                       (std::abs((Styles::componentStyles.slotRadius * 2.f) - Styles::componentStyles.slotLabelSize) / 2.f);

            const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
            materialRenderer->drawText(m_name,
                                       {labelX, pos.y + dY, pos.z},
                                       Styles::componentStyles.slotLabelSize,
                                       ViewportTheme::colors.text,
                                       PickingId{parentComp->getRuntimeId(), 0},
                                       parentComp->getTransform().angle);
        }
    }

    void SlotSceneComponent::drawSchematic(SceneState &state,
                                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (isResizeSlot())
            return;

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

        pathRenderer->beginPathMode(startPos, nodeWeight, pinColor, pinId);
        pathRenderer->pathLineTo({pos.x + offset.x, pos.y + offset.y, pos.z},
                                 nodeWeight, pinColor, pinId);
        pathRenderer->endPathMode(false);

        if (!m_name.empty()) {
            const auto textSize = materialRenderer->getTextRenderSize(m_name,
                                                                      Styles::componentStyles.slotLabelSize);

            float textOffsetX = 4.f;

            if (m_slotType == SlotType::digitalOutput)
                textOffsetX -= textSize.x + 6.f;

            const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
            // not using schematic slot pos for text as in schematic view,
            // slot is rendered behind the component but text should be in front of component
            // so using z of node view
            materialRenderer->drawText(m_name,
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
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputStates[m_index]
                   : compState.outputStates[m_index];
    }

    bool SlotSceneComponent::isSlotConnected(const SceneState &state) const {
        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputConnected[m_index]
                   : compState.outputConnected[m_index];
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

        if (startSlot->getSlotType() == SlotType::inputsResize ||
            startSlot->getSlotType() == SlotType::outputsResize) {
            const auto startParent = e.sceneState->getComponentByUuid<SimulationSceneComponent>(startSlot->getParentComponent());
            const auto &simEngineId = startParent->getSimEngineId();
            const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simEngineId);

            std::shared_ptr<SlotSceneComponent> newSlot = std::make_shared<SlotSceneComponent>();

            if (startSlot->getSlotType() == SlotType::inputsResize) {
                const auto newSize = digitalComp->incrementInputCount();
                // we check against -1 because the resize slot is also there
                if (newSize == startParent->getInputSlotsCount() - 1) {
                    BESS_WARN("[Scene] Failed to resize input slots for component {}", (uint64_t)startParent->getUuid());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                    return;
                }

                newSlot->setSlotType(SlotType::digitalInput);
                newSlot->setIndex((int)newSize - 1);
                newSlot->setName(std::string(1, (char)('A' + newSize - 1)));
                startParent->addInputSlot(newSlot->getUuid());
            } else {
                const auto newSize = digitalComp->incrementOutputCount();
                // we check against -1 because the resize slot is also there
                if (newSize == startParent->getOutputSlotsCount() - 1) {
                    BESS_WARN("[Scene] Failed to resize output slots for component {}, returnedNewSize = {}",
                              (uint64_t)startParent->getUuid(),
                              newSize);
                    e.sceneState->setConnectionStartSlot(UUID::null);
                    return;
                }

                newSlot->setSlotType(SlotType::digitalOutput);
                newSlot->setIndex((int)newSize - 1);
                newSlot->setName(std::string(1, (char)('a' + newSize - 1)));
                startParent->addOutputSlot(newSlot->getUuid());
            }

            e.sceneState->addComponent<SlotSceneComponent>(newSlot);
            e.sceneState->attachChild(startParent->getUuid(), newSlot->getUuid());

            startParent->setScaleDirty();
            startSlot = newSlot;

            BESS_INFO("[Scene] Resized component {}, new slot index = {}, new total slots = {}",
                      (uint64_t)startParent->getUuid(),
                      newSlot->getIndex(),
                      startSlot->getSlotType() == SlotType::digitalInput
                          ? startParent->getInputSlotsCount()
                          : startParent->getOutputSlotsCount());
        }

        if (endSlot->getSlotType() == SlotType::inputsResize ||
            endSlot->getSlotType() == SlotType::outputsResize) {
            const auto endParent = e.sceneState->getComponentByUuid<SimulationSceneComponent>(endSlot->getParentComponent());

            const auto &simEngineId = endParent->getSimEngineId();

            const auto &digitalComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simEngineId);

            std::shared_ptr<SlotSceneComponent> newSlot = std::make_shared<SlotSceneComponent>();

            if (endSlot->getSlotType() == SlotType::inputsResize) {
                const auto newSize = digitalComp->incrementInputCount();
                // we check against -1 because the resize slot is also there
                if (newSize == endParent->getInputSlotsCount() - 1) {
                    BESS_WARN("[Scene] Failed to resize input slots for component {}", (uint64_t)endParent->getUuid());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                    return;
                }

                newSlot->setSlotType(SlotType::digitalInput);
                newSlot->setIndex((int)newSize - 1);
                newSlot->setName(std::string(1, (char)('A' + newSize - 1)));
                endParent->addInputSlot(newSlot->getUuid());
            } else {
                const auto newSize = digitalComp->incrementOutputCount();
                // we check against -1 because the resize slot is also there
                if (newSize == endParent->getOutputSlotsCount() - 1) {
                    BESS_WARN("[Scene] Failed to resize output slots for component {}", (uint64_t)endParent->getUuid());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                    return;
                }

                newSlot->setSlotType(SlotType::digitalOutput);
                newSlot->setIndex((int)newSize - 1);
                newSlot->setName(std::string(1, (char)('a' + newSize - 1)));
                endParent->addOutputSlot(newSlot->getUuid());
            }

            e.sceneState->addComponent<SlotSceneComponent>(newSlot);
            e.sceneState->attachChild(endParent->getUuid(), newSlot->getUuid());

            endParent->setScaleDirty();
            endSlot = newSlot;

            BESS_INFO("[Scene] Resized component {}, new slot index = {}, new total slots = {}",
                      (uint64_t)endParent->getUuid(),
                      newSlot->getIndex(),
                      endSlot->getSlotType() == SlotType::digitalInput
                          ? endParent->getInputSlotsCount()
                          : endParent->getOutputSlotsCount());
        }

        const auto startParent = e.sceneState->getComponentByUuid<SimulationSceneComponent>(startSlot->getParentComponent());
        const auto endParent = e.sceneState->getComponentByUuid<SimulationSceneComponent>(endSlot->getParentComponent());

        const auto startPinType = startSlot->getSlotType() == SlotType::digitalInput
                                      ? SimEngine::SlotType::digitalInput
                                      : SimEngine::SlotType::digitalOutput;

        const auto endPinType = endSlot->getSlotType() == SlotType::digitalInput
                                    ? SimEngine::SlotType::digitalInput
                                    : SimEngine::SlotType::digitalOutput;

        const auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto [canConnect, reason] = simEngine.canConnectComponents(startParent->getSimEngineId(), startSlot->getIndex(), startPinType,
                                                                         endParent->getSimEngineId(), endSlot->getIndex(), endPinType);

        if (!canConnect) {
            BESS_WARN("Cannot create connection between component {} slot {} and component {} slot {}: {}",
                      (uint64_t)startParent->getUuid(), startSlot->getIndex(),
                      (uint64_t)endParent->getUuid(), endSlot->getIndex(),
                      reason);
            e.sceneState->setConnectionStartSlot(UUID::null);
            return;
        }

        auto conn = std::make_shared<ConnectionSceneComponent>();
        UUID starSlotUuid = jointComp ? jointComp->getUuid() : startSlot->getUuid();

        conn->setStartEndSlots(starSlotUuid, endSlot->getUuid());

        if (jointComp) {
            jointComp->addConnection(conn->getUuid());
        } else {
            startSlot->addConnection(conn->getUuid());
        }

        endSlot->addConnection(conn->getUuid());

        auto &cmdManager = Pages::MainPage::getInstance()->getState().getCommandSystem();
        cmdManager.execute(std::make_unique<Cmd::AddCompCmd<ConnectionSceneComponent>>(conn));

        BESS_INFO("[Scene] Created connection {} between slots {} and {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)starSlotUuid,
                  (uint64_t)endSlot->getUuid());

        e.sceneState->setConnectionStartSlot(UUID::null);
    }
} // namespace Bess::Canvas
