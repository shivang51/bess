#include "scene/scene_state/components/slot_scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "simulation_engine.h"
#include "ui/ui.h"

namespace Bess::Canvas {
    SlotSceneComponent::SlotSceneComponent(UUID uuid)
        : SceneComponent(uuid) {
        m_type = SceneComponentType::slot;
    }

    SlotSceneComponent::SlotSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {
        m_type = SceneComponentType::slot;
    }

    void SlotSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        UI::setCursorPointer();
    }

    void SlotSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UI::setCursorNormal();
    }

    void SlotSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        Events::EventDispatcher::instance().trigger(Events::SlotClickedEvent{
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

        const auto &slotState = getSlotState(state);

        const auto pickingId = PickingId{m_runtimeId, 0};

        // state color
        auto bg = ViewportTheme::colors.stateLow;
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

        auto border = bg;

        if (!isSlotConnected(state))
            bg.a = 0.1f;

        const float ir = Styles::simCompStyles.slotRadius - Styles::simCompStyles.slotBorderSize;
        const float r = Styles::simCompStyles.slotRadius;
        materialRenderer->drawCircle(pos, r, border, pickingId, ir);
        materialRenderer->drawCircle(pos, ir - 1.f, bg, pickingId);

        const float labeldx = Styles::simCompStyles.slotMargin + (Styles::simCompStyles.slotRadius * 2.f);
        float labelX = pos.x;
        if (m_slotType == SlotType::digitalInput) {
            labelX += labeldx;
        } else {
            const auto labelSize = materialRenderer->getTextRenderSize(m_name, Styles::simCompStyles.slotLabelSize);
            labelX -= labeldx + labelSize.x;
        }
        float dY = componentStyles.slotRadius - (std::abs((componentStyles.slotRadius * 2.f) - componentStyles.slotLabelSize) / 2.f);

        materialRenderer->drawText(m_name,
                                   {labelX, pos.y + dY, pos.z},
                                   componentStyles.slotLabelSize,
                                   ViewportTheme::colors.text,
                                   PickingId{parentComp->getRuntimeId(), 0},
                                   parentComp->getTransform().angle);
    }

    SimEngine::PinState SlotSceneComponent::getSlotState(const SceneState *state) const {
        const auto parentComp = state->getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputStates[m_index]
                   : compState.outputStates[m_index];
    }

    SimEngine::PinState SlotSceneComponent::getSlotState(const SceneState &state) const {
        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputStates[m_index]
                   : compState.outputStates[m_index];
    }

    SimEngine::PinState SlotSceneComponent::isSlotConnected(const SceneState &state) const {
        const auto parentComp = state.getComponentByUuid<SimulationSceneComponent>(m_parentComponent);
        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &compState = simEngine.getComponentState(parentComp->getSimEngineId());

        return (m_slotType == SlotType::digitalInput)
                   ? compState.inputConnected[m_index]
                   : compState.outputConnected[m_index];
    }

} // namespace Bess::Canvas
