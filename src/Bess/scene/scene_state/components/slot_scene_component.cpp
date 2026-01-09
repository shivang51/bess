#include "scene/scene_state/components/slot_scene_component.h"
#include "event_dispatcher.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
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
        EventSystem::EventDispatcher::instance().dispatch(Events::SlotClickedEvent{
            e.mousePos,
            m_uuid,
            e.button,
            e.action});
    }

    void SlotSceneComponent::draw(SceneState &state,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        const auto &parentPos = parentComp->getTransform().position;
        const auto pos = parentPos + m_transform.position;
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
        if (m_slotType == SlotType::digitalOutput) {
            offset.x = Styles::compSchematicStyles.pinSize;
        } else {
            offset.x = -Styles::compSchematicStyles.pinSize;
        }

        pathRenderer->beginPathMode(pos, nodeWeight, pinColor, pinId);
        pathRenderer->pathLineTo({pos.x + offset.x, pos.y + offset.y, pos.z},
                                 nodeWeight, pinColor, pinId);
        pathRenderer->endPathMode(false);

        if (!m_name.empty()) {
            const auto textSize = materialRenderer->getTextRenderSize(m_name,
                                                                      Styles::componentStyles.slotLabelSize);

            float textOffsetX = 0.f;

            if (m_slotType == SlotType::digitalOutput)
                textOffsetX -= textSize.x;

            materialRenderer->drawText(m_name,
                                       {pos.x + offset.x + textOffsetX, pos.y + offset.y - nodeWeight, pos.z},
                                       Styles::componentStyles.slotLabelSize,
                                       ViewportTheme::colors.text, pinId,
                                       0.f);
        }
    }

    SimEngine::SlotState SlotSceneComponent::getSlotState(const SceneState *state) const {
        BESS_ASSERT(m_index >= 0, "Slot index is negative");

        const auto parentComp = state->getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputStates[m_index]
                   : compState.outputStates[m_index];
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

    std::vector<UUID> SlotSceneComponent::cleanup(SceneState &state, UUID caller) {
        for (const auto &connUuid : m_connectedConnections) {
            auto connComp = state.getComponentByUuid(connUuid);
            if (connComp) {
                state.removeComponent(connUuid, m_uuid);
            }
        }
        return m_connectedConnections;
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

    glm::vec3 SlotSceneComponent::getSchematicPosAbsolute(const SceneState &state) const {
        return state.getComponentByUuid(m_parentComponent)->getAbsolutePosition(state) +
               m_schematicPos;
    }

    bool SlotSceneComponent::isResizeSlot() const {
        return m_slotType == SlotType::inputsResize ||
               m_slotType == SlotType::outputsResize;
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SlotSceneComponent &component, Json::Value &j) {
        toJsonValue(static_cast<const Bess::Canvas::SceneComponent &>(component), j);

        j["slotType"] = static_cast<uint8_t>(component.getSlotType());
        j["index"] = component.getIndex();
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SlotSceneComponent &component) {
        fromJsonValue(j, static_cast<Bess::Canvas::SceneComponent &>(component));

        if (j.isMember("slotType")) {
            component.setSlotType(static_cast<Bess::Canvas::SlotType>(j["slotType"].asUInt()));
        }

        if (j.isMember("index")) {
            component.setIndex(j["index"].asInt());
        }
    }
} // namespace Bess::JsonConvert
